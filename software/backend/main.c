
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h> 

#pragma comment(lib, "ws2_32.lib") y

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    WSADATA wsaData;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_size = sizeof(client_addr);
    char buffer[BUFFER_SIZE] = {0};
    const char *ack_message = "Message received!";

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, 3) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    printf("Server listening on port %d\n", PORT);

    if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size)) == INVALID_SOCKET) {
        printf("Accept failed. Error Code: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    printf("Client connected\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE); 
        int bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_read <= 0) {
            printf("Client disconnected\n");
            break;
        }
        printf("Received: %s\n", buffer);

        // Send acknowledgment
        send(client_socket, ack_message, strlen(ack_message), 0);
    }

    // Close sockets
    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();
    return 0;
}
