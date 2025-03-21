#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "shell.h"

void run_client(int port) {
    printf("Client is running on port %d\n", port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    char buffer[1024];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    read(sock, buffer, sizeof(buffer));
    printf("%s", buffer);

    while (1) {
        fgets(buffer, sizeof(buffer), stdin);

        if (strlen(buffer) == 1 && buffer[0] == '\n') {
            write(sock, buffer, strlen(buffer));
            memset(buffer, 0, sizeof(buffer));
            read(sock, buffer, sizeof(buffer));
            printf("%s", buffer);
            continue;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "quit") == 0) {
            write(sock, buffer, strlen(buffer));
            break;
        }

        write(sock, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    
    close(sock);
}

void run_unix_client(const char *socket_path) {
    printf("Client is connecting to socket %s\n", socket_path);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path), "%s", socket_path);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char buffer[1024];
    read(sock, buffer, sizeof(buffer));
    printf("%s", buffer);

    while (1) {
        fgets(buffer, sizeof(buffer), stdin);

        if (strlen(buffer) == 1 && buffer[0] == '\n') {
            write(sock, buffer, strlen(buffer));
            memset(buffer, 0, sizeof(buffer));
            read(sock, buffer, sizeof(buffer));
            printf("%s", buffer);
            continue;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "quit") == 0) {
            write(sock, buffer, strlen(buffer));
            break;
        }

        write(sock, buffer, strlen(buffer));
        
        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    
    close(sock);
}