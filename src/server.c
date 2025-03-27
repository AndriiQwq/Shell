#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <dirent.h>

#include <netinet/tcp.h>
#include <time.h>
#include <signal.h> // For kill() and SIGTERM
#include <errno.h>  // For ECONNRESET

#include "shell.h"
#include "utils.h" // trim 
#include "script_reader.h" // process_script
#include <pthread.h>

#include "keepalive.h"

#define BUFFER_SIZE 1024

void log_command_execution(const char *command, const char *result, const char *prompt) {
    printf("%s\n", command);
    printf("%s\n", result);
    printf("%s", prompt);
}

void process_command_execution(int client_sock, char *buffer) {
    if(strncmp(buffer, "run ", 4) == 0) {
        char *script_path = buffer + 4;
        char *script_output = process_script(script_path);
        write(client_sock, script_output, strlen(script_output));

        log_command_execution(buffer, script_output, get_prompt());
        free(script_output);
    } else {
        char *result = shell_process_input(buffer);
        char combined[BUFFER_SIZE];
        snprintf(combined, BUFFER_SIZE, "%s\n%s", result, get_prompt());
        write(client_sock, combined, strlen(combined));

        log_command_execution(buffer, result, get_prompt());
        free(result);
    }
}

int receive_client_input(int client_sock, char *buffer) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1);
    
    trim(buffer); // Trim input command

    return bytes_read;
}

void run_server(int port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

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

    while(1){
        memset(&client_addr, 0, BUFFER_SIZE);

        printf("Waiting for connections...\n");
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            close(server_sock);
            exit(1);
        }

        enable_keepalive(client_sock);

        char buffer[BUFFER_SIZE];
        printf("Client connected\n");
        write(client_sock, get_prompt(), strlen(get_prompt()));



        
        time_t last_activity_time = time(NULL);

        pthread_t keepalive_thread_id;
        KeepAliveArgs keepalive_args = {client_sock, &last_activity_time, keep_alive.timeout};
        if (pthread_create(&keepalive_thread_id, NULL, keepalive_thread, &keepalive_args) != 0) {
            perror("pthread_create");
            close(client_sock);
            close(server_sock);
            exit(1);
        }

        while (1) {
            memset(buffer, 0, BUFFER_SIZE);

            int bytes_read = receive_client_input(client_sock, buffer);
            if (bytes_read > 0) {
                last_activity_time = time(NULL);

                if (strcmp(buffer, "quit") == 0) {
                    printf("Client exit\n");
                    break;
                } else if (strcmp(buffer, "halt") == 0) {
                    printf("Server halting\n");
                    close(client_sock);
                    close(server_sock);
                    exit(0);
                }

                process_command_execution(client_sock, buffer);
            } else if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Temporary error, continue
                    continue;
                } else if (errno == ECONNRESET) {
                    // the connection has been forcibly closed by another party (e.g. server or client)
                    break;
                } else {
                    perror("read");
                    close(client_sock);
                    close(server_sock);
                    exit(1);
                }
            } else if (bytes_read == 0) {
                printf("Client disconnected\n"); // Client close the connection
                break;
            }
        }
        
        if (pthread_cancel(keepalive_thread_id) != 0) {
            perror("pthread_cancel");
        }
        pthread_join(keepalive_thread_id, NULL);

        close(client_sock);
        printf("Waiting for a new connection...\n");

    }

    close(server_sock);
    exit(0); // Exit the server, and stop all sub-threads
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

    enable_keepalive(client_sock);
    
    char buffer[BUFFER_SIZE];
    printf("Client connected\n");
    write(client_sock, get_prompt(), strlen(get_prompt()));


    time_t last_activity_time = time(NULL);

    pthread_t keepalive_thread_id;
    KeepAliveArgs keepalive_args = {client_sock, &last_activity_time, keep_alive.timeout};
    if (pthread_create(&keepalive_thread_id, NULL, keepalive_thread, &keepalive_args) != 0) {
        perror("pthread_create");
        close(client_sock);
        close(server_sock);
        exit(1);
    }

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_read = receive_client_input(client_sock, buffer);
        if (bytes_read > 0) {
            last_activity_time = time(NULL);

            if (strcmp(buffer, "quit") == 0) {
                printf("Client exit\n");
                break;
            } else if (strcmp(buffer, "halt") == 0) {
                printf("Server halting\n");
                close(client_sock);
                close(server_sock);
                unlink(socket_path);
                exit(0);
            }

            process_command_execution(client_sock, buffer);
        } else if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Temporary error, continue
                continue;
            } else if (errno == ECONNRESET) {
                // the connection has been forcibly closed by another party (e.g. server or client)
                break;
            } else {
                perror("read");
                close(client_sock);
                close(server_sock);
                unlink(socket_path);
                exit(1);
            }
        } else if (bytes_read == 0) {
            printf("Client disconnected\n"); // Client close the connection
            break;
        }
    }

    close(client_sock);
    close(server_sock);
    unlink(socket_path);
    if (pthread_cancel(keepalive_thread_id) != 0) {
        perror("pthread_cancel");
    }
    exit(0); // Exit the server, and stop all sub-threads
}
