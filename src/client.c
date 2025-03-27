#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <time.h>
#include <signal.h> // For kill() and SIGTERM
#include <errno.h>  // For ECONNRESET
#include <pthread.h>


#include "shell.h"
#include "keepalive.h"

// void run_keep_alive_daemon(int sock, KeepAlive *keep_alive) {}
// #define check(expr) if (!(expr)) { perror(#expr); exit(EXIT_FAILURE); }

#define BUFFER_SIZE 1024

void process_user_input(int sock, char *buffer) {
    fgets(buffer, BUFFER_SIZE, stdin);
    
    if (strncmp(buffer, "disconect ", 10) == 0) {
        write(sock, buffer, strlen(buffer));
        exit(0);
    }

    if (strlen(buffer) == 1 && buffer[0] == '\n') {
        write(sock, buffer, strlen(buffer));
        memset(buffer, 0, BUFFER_SIZE);
        read(sock, buffer, BUFFER_SIZE);
        printf("%s", buffer);
        return;
    }

    buffer[strcspn(buffer, "\n")] = 0;

    if (strcmp(buffer, "quit") == 0) { // Stop the client
        write(sock, buffer, strlen(buffer));
        close(sock); // exit program
        exit(0);
    }

    if (strcmp(buffer, "halt") == 0) { // Stop the server and the client
        write(sock, buffer, strlen(buffer));
        close(sock); // exit program
        exit(0);
    }

    write(sock, buffer, strlen(buffer));
    
    memset(buffer, 0, BUFFER_SIZE);
    // read(sock, buffer, BUFFER_SIZE);
    // printf("%s", buffer);
}

void run_client(int port) {
    printf("Client is running on port %d\n", port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    /* Set 
        5-second interval, 
        3 probes, 
        30-second timeout
    */
    // KeepAlive keep_alive = {5, 3, 30}; 
    //https://stackoverflow.com/questions/31426420/configuring-tcp-keepalive-after-accept
    //https://riptutorial.com/posix/example/17423/enabling-tcp-keepalive-at-server-side
    enable_keepalive(sock); // Using TCP keep-alive conections
    // run_keep_alive_daemon(sock, &keep_alive);

    // receive prompt
    char buffer[BUFFER_SIZE]; 
    read(sock, buffer, BUFFER_SIZE);
    printf("%s", buffer);


    time_t last_activity_time = time(NULL);

    // Thread for keep-alive
    pthread_t keepalive_thread_id;
    KeepAliveArgs keepalive_args = {sock, &last_activity_time, keep_alive.timeout};
    if (pthread_create(&keepalive_thread_id, NULL, keepalive_thread, &keepalive_args) != 0) {
        perror("pthread_create");
        close(sock);
        exit(1);
    }
    
    

    while (1) {
        process_user_input(sock, buffer); // send user input, clear buffer

        last_activity_time = time(NULL); // Update last activity time

        // READ RESPONSE FROM SERVER

        int bytes_read = read(sock, buffer, BUFFER_SIZE);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Temporary error, continue
                continue;
            } else if (errno == ECONNRESET) { 
                printf("Connection closed due to Keep-Alive timeout\n");
                // stop_keepalive = 1;
                // pthread_join(keepalive_thread_id, NULL); // Wait for the end of the thread
                break;
            } else {
                perror("read");
                close(sock);
                exit(1);
            }
        } else if (bytes_read == 0) {
            printf("Server disconnected\n"); // SErver close the connection
            break;
        }

        printf("%s", buffer);
    }
    
    
    close(sock);
    if (pthread_cancel(keepalive_thread_id) != 0) {
        perror("pthread_cancel");
    }
    exit(0); // Exit the client program and stop all sub-threads
}

void run_unix_client(const char *socket_path) {
    printf("Client is connecting to socket %s\n", socket_path);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;

    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path), "%s", socket_path);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);
        exit(1);
    }

    enable_keepalive(sock);

    char buffer[BUFFER_SIZE];
    read(sock, buffer, BUFFER_SIZE);
    printf("%s", buffer);



    time_t last_activity_time = time(NULL);

    // Thread for keep-alive
    pthread_t keepalive_thread_id;
    KeepAliveArgs keepalive_args = {sock, &last_activity_time, keep_alive.timeout};
    if (pthread_create(&keepalive_thread_id, NULL, keepalive_thread, &keepalive_args) != 0) {
        perror("pthread_create");
        close(sock);
        unlink(socket_path);
        exit(1);
    }
    

    while (1) {
        process_user_input(sock, buffer); // send user input, clear buffer

        last_activity_time = time(NULL); // Update last activity time

        // READ RESPONSE FROM SERVER

        int bytes_read = read(sock, buffer, BUFFER_SIZE);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Temporary error, continue
                continue;
            } else if (errno == ECONNRESET) { 
                printf("Connection closed due to Keep-Alive timeout\n");
                break;
            } else {
                perror("read");
                close(sock);
                unlink(socket_path);
                exit(1);
            }
        } else if (bytes_read == 0) {
            printf("Server disconnected\n"); // SErver close the connection
            break;
        }

        printf("%s", buffer);
    }
    
    close(sock);
    unlink(socket_path);
    if (pthread_cancel(keepalive_thread_id) != 0) {
        perror("pthread_cancel");
    }
    exit(0); // Exit the client program and stop all sub-threads
}