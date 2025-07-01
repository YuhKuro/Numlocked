#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <stdbool.h>
#include <tlhelp32.h>


#include "cJSON.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 256
#define QUEUE_SIZE 100


HANDLE hSerial;
DWORD bytesWritten;

FILETIME lastModified = {0};
volatile bool autoGameMode = false; 

enum timeZones {
    deviceTime = 0,
    EST = 1,
    CST = 2,
    JST = 3
};

int timeZone = deviceTime;

typedef struct {
    char message[BUFFER_SIZE];
} queueItem;

typedef struct{
    queueItem items[QUEUE_SIZE];
    int front, rear, size;
    CRITICAL_SECTION lock;
} messageQueue;

void initQueue(messageQueue *queue){
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    InitializeCriticalSection(&queue->lock);
}



int isQueueFull(messageQueue *queue){
    return queue->size == QUEUE_SIZE;
}

int isQueueEmpty(messageQueue *queue){
    return queue->size == 0;
}

void enqueue(messageQueue *queue, const char *message){
    EnterCriticalSection(&queue->lock);

    if (isQueueFull(queue)){
        LeaveCriticalSection(&queue->lock);
        return;
    }

    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    strcpy(queue->items[queue->rear].message, message);
    queue->size++;

    LeaveCriticalSection(&queue->lock);
}

int dequeue(messageQueue *queue, char *message){
    EnterCriticalSection(&queue->lock);

    if (isQueueEmpty(queue)){
        LeaveCriticalSection(&queue->lock);
        return 0;
    }

    strcpy(message, queue->items[queue->front].message);
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    queue->size--;

    LeaveCriticalSection(&queue->lock);
    return 1;
}

void freeQueue(messageQueue *queue){
    DeleteCriticalSection(&queue->lock);
}

void setLowPriority() {
    if (!SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS)) {
        printf("Failed to set low priority. Error: %lu\n", GetLastError());
    } else {
        printf("Process priority set to below normal.\n");
    }
}

SOCKET connectToServer(const char *serverIp, int port) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed. Error: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed. Error: %d\n", WSAGetLastError());
        WSACleanup();
        return INVALID_SOCKET;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIp);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Connection failed. Error: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    printf("Connected to server at %s:%d\n", serverIp, port);
    return sock;
}

void sendMessage(SOCKET sock, const char *message) {
    if (send(sock, message, strlen(message), 0) == SOCKET_ERROR) {
        printf("Failed to send message. Error: %d\n", WSAGetLastError());
    } else {
        printf("Message sent: %s\n", message);
    }
}

void getDesiredTime(char *buffer, size_t bufferSize, int timeZone) {
    time_t t = time(NULL);
    struct tm *timeinfo = gmtime(&t);
    if (timeZone == CST) {
        timeinfo->tm_hour -= 6;
    } else if (timeZone == JST) {
        timeinfo->tm_hour += 9;
    } else if (timeZone == EST) {
        timeinfo->tm_hour -= 5;
    } else if (timeZone == deviceTime) {
        timeinfo = localtime(&t);
    }
    strftime(buffer, bufferSize, "%I:%M%p", timeinfo);

}


bool file_has_changed(const char* filename) {
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    FILETIME currentModified;
    bool changed = false;
    
    if (GetFileTime(hFile, NULL, NULL, &currentModified)) {
        // Compare with stored time
        if (CompareFileTime(&lastModified, &currentModified) != 0) {
            lastModified = currentModified;  // Update stored time
            changed = true;
        }
    }
    
    CloseHandle(hFile);
    return changed;
}

char **gameList;
int gameListSize = 0;

int loadCJSON() {
     //extract list of games from JSON.
    FILE *fptr; 
    fptr = fopen("../frontend/settings/gameModeAppsList.json", "r");
    if (!fptr) return -1;

    fseek(fptr, 0, SEEK_END);
    long fileSize = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    char* gameListStr = malloc(fileSize + 1);
    fread(gameListStr, 1, fileSize, fptr);

    fclose(fptr);

    cJSON *gameListJson = cJSON_Parse(gameListStr);
    free(gameListStr);
    if (!gameListJson) {
        printf("Error parsing JSON\n");
        return 1;
    }

    cJSON *gameModeApps = cJSON_GetObjectItem(gameListJson, "gameModeApps");
    if (!cJSON_IsArray(gameModeApps)) {
        printf("gameModeApps not array\n");
        cJSON_Delete(gameListJson);
        return 1;
    }

    gameListSize = cJSON_GetArraySize(gameModeApps);
    if (gameList) {
        free(gameList);
    }
    gameList = malloc(gameListSize * sizeof(char*));

    for (int i = 0; i < gameListSize; i++){
        cJSON *item = cJSON_GetArrayItem(gameModeApps, i);
        if (cJSON_IsString(item)) {
            char *appName = cJSON_GetStringValue(item);
            gameList[i] = strdup(appName);
        }
    }

    cJSON_Delete(gameListJson);
    return 0;
}

bool game_in_running_programs(){


    if (file_has_changed ("../frontend/settings/gameModeAppsList.json")){ 
        int success = loadCJSON();
        if (success) {
            return -1;
        }
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        printf("Error: unable to handle snapshot\n");
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            for (int i = 0; i < gameListSize; i++) {
                if (strcmp(pe32.szExeFile, gameList[i]) == 0) {
                    printf("Game found: %s\n", gameList[i]);
                    CloseHandle(hSnapshot);
                    return true;
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    printf("No game found\n");

    return false;
    
}

void send_to_usb(const char* message){
    if (hSerial == INVALID_HANDLE_VALUE){
        printf("Error opening COM port\n");
        return;
    }
    WriteFile(hSerial, message, strlen(message), &bytesWritten, NULL);
    printf("%d bytes written", bytesWritten);
}


DWORD WINAPI usbWorkerThread(LPVOID param) {
    messageQueue *queue = (messageQueue *)param;
    char messageBuffer[BUFFER_SIZE];

    while (1) {
        if (dequeue(queue, messageBuffer)) {
            //check if the message contains mention of timezone change
            if (strstr(messageBuffer, "3") != NULL) {
                if (strstr(messageBuffer, "EST") != NULL) {
                    printf("Timezone changed to EST\n");
                    timeZone = EST;
                } else if (strstr(messageBuffer, "CST") != NULL) {
                    printf("Timezone changed to CST\n");
                    timeZone = CST;
                } else if (strstr(messageBuffer, "JST") != NULL) {
                    printf("Timezone changed to JST\n");
                    timeZone = JST;
                } else if (strstr(messageBuffer, "device") != NULL) {
                    printf("Timezone changed to device time\n");
                    timeZone = deviceTime;
                } 
                char timeMessage[BUFFER_SIZE];
                getDesiredTime(timeMessage, sizeof(timeMessage), timeZone);
                printf("Sending to USB: %s\n", timeMessage);
                send_to_usb(timeMessage);

            } else if (strstr(messageBuffer, "5") != NULL){ //message starts with 5, means automatically enable game mode.
                //if an app from the game mode applications is launched, send signal 4A to enable game mode.    
                if (strstr(messageBuffer, "A")) {
                    printf("Enable auto game mode.\n");
                    autoGameMode = true;
                } else {
                    printf("Disable auto game mode.\n");
                    autoGameMode = false;
                }
            }else {
                printf("Sending to USB: %s\n", messageBuffer);
                send_to_usb(messageBuffer);
            }
        }
        Sleep(50); // Small delay to reduce CPU usage
    }
    return 0;
}


int main() {
    messageQueue queue;
    initQueue(&queue);
    setLowPriority();

    HANDLE hThread = CreateThread(NULL, 0, usbWorkerThread, &queue, 0, NULL);
    if (hThread == NULL) {
        printf("Failed to create USB worker thread. Error: %lu\n", GetLastError());
        freeQueue(&queue);
        return 1;
    }

    time_t lastClockUpdate = 0;
    time_t lastAppCheck = 0;
    char timeMessage[BUFFER_SIZE];
    SOCKET sock = INVALID_SOCKET;



    hSerial = CreateFile(
        "COM3",                         //set to COM with actual port for CDC device.
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening COM port \n");
    }


    while (1) {
        time_t now = time(NULL);

        if (difftime(now, lastClockUpdate) >= 3600 || lastClockUpdate == 0) {
            getDesiredTime(timeMessage, sizeof(timeMessage), timeZone);
            enqueue(&queue, timeMessage);
            lastClockUpdate = now;
        }

        if ((difftime(now, lastAppCheck) >= 20 || lastAppCheck == 0) && autoGameMode){
            printf("checking games... \n");
            if (game_in_running_programs()){
                printf("Game found, enable game mode\n");
                enqueue(&queue, "4A");
                sendMessage(sock, "4A\n");
            } else {
                printf("No game found, disable game mode\n");
                enqueue(&queue, "4B");
                sendMessage(sock, "4B\n");
            }
            lastAppCheck = now;
        }

        if (sock == INVALID_SOCKET) {
            sock = connectToServer(SERVER_IP, SERVER_PORT);
            if (sock != INVALID_SOCKET) {
                u_long mode = 1; // 1 = non-blocking, 0 = blocking
                ioctlsocket(sock, FIONBIO, &mode);
            } else {
                printf("HUB GUI is inactive. Retrying in 5 seconds...\n");
                Sleep(5000); // Sleep for 5 seconds to reduce CPU usage when not connected.
            } 
        } else {
            int iResult;
            char recvbuf[BUFFER_SIZE];
            int recvbuflen = BUFFER_SIZE;
            iResult = recv(sock, recvbuf, recvbuflen - 1, 0);
            if (iResult > 0) {
                recvbuf[iResult] = '\0';
                printf("Message received: %s\n", recvbuf);
                enqueue(&queue, recvbuf);
            } else if (iResult == 0) {
                // Connection closed
                printf("Connection closed by server\n");
                closesocket(sock);
                sock = INVALID_SOCKET;
            } else {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    // Real error occurred
                    printf("recv failed: %d\n", error);
                    closesocket(sock);
                    sock = INVALID_SOCKET;
                }
            }

        }
        Sleep(100); 
    }
    printf("Exiting program...\n");
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    closesocket(sock);
    freeQueue(&queue);
    free(gameList);
    WSACleanup();
    return 0;
}
