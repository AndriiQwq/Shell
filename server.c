#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/un.h>
#include <dirent.h>

#include "shell.h"

void run_server(int port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024];

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen");
        close(server_sock);
        exit(1);
    }

    printf("Waiting for connections...\n");
    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("accept");
        close(server_sock);
        exit(1);
    }

    printf("Client connected\n");
    write(client_sock, get_prompt(), strlen(get_prompt()));

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read < 0) {
            perror("read");
            close(client_sock);
            close(server_sock);
            exit(1);
        } else if (bytes_read == 0) {
            printf("Client disconnected\n");
            break;
        }

        char *result = shell_process_command(buffer, client_sock);
        
        char combined[1024];
        snprintf(combined, sizeof(combined), "%s\n%s", result, get_prompt());
        write(client_sock, combined, strlen(combined));

        printf("%s\n", buffer);
        printf("%s\n", result);
        printf("%s", get_prompt());

        if (strcmp(buffer, "quit") == 0) {
            printf("Client requested to quit\n");
            break;
        } else if (strcmp(buffer, "halt") == 0) {
            printf("Server halting\n");
            close(client_sock);
            close(server_sock);
            exit(0);
        } else if (strcmp(buffer, "help") == 0) {
            write(client_sock, "Available commands: help, quit, halt", 36);
        } else if (strncmp(buffer, "receivefile ", 12) == 0) {
            const char *output_file = buffer + 12;
            receive_file(client_sock, output_file);
            continue;
        }
    }

    close(client_sock);
    close(server_sock);
}

void run_unix_server(const char *socket_path) {
    printf("Server is running on socket %s\n", socket_path);

    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path), "%s", socket_path);

    unlink(server_addr.sun_path);
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) == -1) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections on %s...\n", socket_path);
    int client_sock = accept(server_sock, NULL, NULL);
    if (client_sock == -1) {
        perror("Accept failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");
    write(client_sock, get_prompt(), strlen(get_prompt()));

    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
        if (bytes_read < 0) {
            perror("read");
            close(client_sock);
            close(server_sock);
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            printf("Client disconnected\n");
            break;
        }

        char *result = shell_process_command(buffer, client_sock);
        
        char combined[1024];
        snprintf(combined, sizeof(combined), "%s\n%s", result, get_prompt());
        write(client_sock, combined, strlen(combined));

        printf("%s\n", buffer);
        printf("%s\n", result);
        printf("%s", get_prompt());

        if (strcmp(buffer, "quit") == 0) {
            printf("Client requested to quit\n");
            break;
        } else if (strcmp(buffer, "halt") == 0) {
            printf("Server halting\n");
            close(client_sock);
            close(server_sock);
            unlink(socket_path);
            exit(0);
        }
    }

    close(client_sock);
    close(server_sock);
    unlink(socket_path);
}