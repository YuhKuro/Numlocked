#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <stdbool.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 256
#define QUEUE_SIZE 100

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
            } else {
                printf("Sending to USB: %s\n", messageBuffer);
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

    time_t lastUpdate = 0;
    char timeMessage[BUFFER_SIZE];
    SOCKET sock = INVALID_SOCKET;
    while (1) {
        time_t now = time(NULL);

        if (difftime(now, lastUpdate) >= 3600 || lastUpdate == 0) {
            getDesiredTime(timeMessage, sizeof(timeMessage), timeZone);
            enqueue(&queue, timeMessage);
            lastUpdate = now;
        }

        if (sock == INVALID_SOCKET) {
            sock = connectToServer(SERVER_IP, SERVER_PORT);
            if (sock == INVALID_SOCKET) {
                printf("HUB GUI is inactive. Retrying in 5 seconds...\n");
            } //else {
                //sendMessage(sock, "Backend connected to frontend\n");
           // }
            Sleep(5000); // Sleep for 5 seconds to reduce CPU usage when not connected.
        } else {
            int iResult;
            char recvbuf[BUFFER_SIZE];
            int recvbuflen = BUFFER_SIZE;

            iResult = recv(sock, recvbuf, recvbuflen - 1, 0);
            if (iResult > 0) {
                recvbuf[iResult] = '\0'; // Null-terminate the received message
                printf("Message received: %s\n", recvbuf);
                enqueue(&queue, recvbuf);
            } else {
                printf("recv failed: %d\n", WSAGetLastError());
                closesocket(sock);
                sock = INVALID_SOCKET;
            }
        }
    }
    printf("Exiting program...\n");
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    closesocket(sock);
    freeQueue(&queue);
    WSACleanup();
    return 0;
}
