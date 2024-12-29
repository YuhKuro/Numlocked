#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 256
#define QUEUE_SIZE 100

typedef struct {
    char message[BUFFER_SIZE];
} queueItem;

typedef struct{
    queueItem items[QUEUE_SIZE];
    int front, rear, size;
    CRITICAL_SECTION lock;
} messageQueue;

void initQueue(messageQueue queue){
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

void getCurrentTime(char *buffer, size_t bufferSize) {
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", timeinfo);
}


DWORD WINAPI usbWorkerThread(LPVOID param, messageQueue *queue) {
    char messageBuffer[BUFFER_SIZE];

    while (1) {
        if (dequeue(queue, messageBuffer)) {
            // Simulate sending the message via USB to the keyboard
            printf("Sending to USB: %s\n", messageBuffer);
        }
        Sleep(50); // Small delay to reduce CPU usage
    }
    return 0;
}

int main() {

    messageQueue queue;
    initQueue(queue);   
    setLowPriority();

    SOCKET sock = connectToServer(SERVER_IP, SERVER_PORT);
    if (sock == INVALID_SOCKET) {
        cleanupQueue(&queue);
        return 1;
    }

    time_t lastUpdate = 0;
    char timeMessage[BUFFER_SIZE];

    while (1) {
        time_t now = time(NULL);

        // Check if an hour has passed or it's the first run
        if (difftime(now, lastUpdate) >= 3600 || lastUpdate == 0) {
                    getCurrentTime(timeMessage, sizeof(timeMessage));
                    enqueue(&queue, timeMessage); // Add time update to the queue
                    lastUpdate = now;
        }

        Sleep(5000); // Sleep for 5 seconds to reduce CPU usage
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
