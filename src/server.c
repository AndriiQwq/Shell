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

#include <pthread.h>

#include "shell.h"
#include "utils.h" // trim 
#include "script_reader.h" // process_script
#include "keepalive.h"
#include "server.h"

#include "logger.h"

#define BUFFER_SIZE 1024

// void log_command_execution(const char *command, const char *result, const char *prompt) {
//     printf("%s\n", command);
//     printf("%s\n", result);
//     printf("%s", prompt);
// }

void process_command_execution(int client_sock, char *buffer) {
    if(strncmp(buffer, "run ", 4) == 0) { // process script
        write_log("CLIENT", buffer); // Server write client log 

        char *script_path = buffer + 4;
        char *script_output = process_script(script_path);
        write(client_sock, script_output, strlen(script_output)); // return script output

        // log_command_execution(buffer, script_output, get_prompt());
        free(script_output);
    } else {
        char *result = shell_process_input(buffer); // process command
        char combined[BUFFER_SIZE];
        snprintf(combined, BUFFER_SIZE, "%s\n%s", result, get_prompt()); // add promt as new line
        write(client_sock, combined, strlen(combined)); //return command result

        // log_command_execution(buffer, result, get_prompt());
        free(result);
    }
}

int receive_client_input(int client_sock, char *buffer) {
    memset(buffer, 0, BUFFER_SIZE); // clear buffer
    int bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1); // read clent bytes 
    
    trim(buffer); // Trim input command

    return bytes_read;
}

// keepalive 
/// @brief 
/// @param client_sock 
/// @param server_sock 
/// @return tid
pthread_t start_keepalive_daemon(int client_sock, int server_sock) {
    enable_keepalive(client_sock);

    time_t last_activity_time = time(NULL); 

    pthread_t keepalive_thread_id; // create tread 
    KeepAliveArgs *keepalive_args = malloc(sizeof(KeepAliveArgs)); // create args to thread and malloc size for them
    if (!keepalive_args) { // error occured 
        perror("malloc");
        close(client_sock);
        close(server_sock);
        exit(1);
    }

    // set argv 
    keepalive_args->sock = client_sock;
    keepalive_args->last_activity_time = &last_activity_time;
    keepalive_args->timeout = keep_alive.timeout;

    // start tread, and check error 
    if (pthread_create(&keepalive_thread_id, NULL, keepalive_thread, keepalive_args) != 0) {
        perror("pthread_create");
        free(keepalive_args);
        close(client_sock);
        close(server_sock);
        exit(1);
    }

    return keepalive_thread_id;
}

void end_current_connection(int client_sock, pthread_t keepalive_thread_id) {
    if (pthread_cancel(keepalive_thread_id) != 0) { // stop keepalive thread 
        perror("pthread_cancel");
    }
    pthread_join(keepalive_thread_id, NULL); // wait for stoping thread

    close(client_sock);
    // printf("Waiting for a new connection...\n");
}

void process_current_server_inputting(int client_sock, int server_sock, char *buffer, const char *socket_path) {
    while (1) {
        memset(buffer, 0, BUFFER_SIZE); // clear buffer for client input

        int bytes_read = receive_client_input(client_sock, buffer); // receive client input
        if (bytes_read > 0) { // read bytes 
            if (strcmp(buffer, "quit") == 0) {
                printf("Client exit\n");
                write_log("CLIENT", "QUIT"); // log 
                break;
            } else if (strcmp(buffer, "halt") == 0) {
                printf("Server halting\n");
                close(client_sock);
                close(server_sock);
                if (socket_path) unlink(socket_path);
                write_log("CLIENT", "HALT SYSTEM"); // log
                exit(0);
            }

            // CLIENT LOG ON SERVER SIDE
            write_log("CLIENT", buffer);
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
                if (socket_path) unlink(socket_path);
                exit(1);
            }
        } else if (bytes_read == 0) {
            printf("Client disconnected\n"); // Client close the connection
            break;
        }
    }
}

void run_server(int port) {
    printf("Server is running on port %d\n", port);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0); // create socket tcp
    if (server_sock < 0) {
        perror("socket");
        exit(1);
    }

    // Resolve problem "Address already in use" after restarting program.
    // It's occurs when a TCP connection is closed, it enters the TIME_WAIT state for a period of time(typically 30 seconds to 2 minutes).
    // During this period, the IP address and port combination is considered busy by the operating system.
    int reuse = 1; 
    // option to reuse addres
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(server_sock);
        exit(1);
    }

    struct sockaddr_in server_addr, client_addr; // create struct for clent and severt addres 
    socklen_t client_len = sizeof(client_addr); // length of socket 

    server_addr.sin_family = AF_INET; //ipv4 
    server_addr.sin_addr.s_addr = INADDR_ANY;// Address to accept any incoming messages.
    server_addr.sin_port = htons(port); //set port 

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { // try to bind socket
        perror("bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) { // try to listen 
        perror("listen");
        close(server_sock);
        exit(1);
    }

    while(1){ // Granart server to work after client disconnect, and waitinf for new connection
        memset(&server_addr, 0, sizeof(server_addr)); // Clear the buffer(for server reconnection)

        printf("Waiting for connections...\n");
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len); // get client socket 
        if (client_sock < 0) {
            perror("accept");
            close(server_sock);
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        printf("Client connected\n");
        write(client_sock, get_prompt(), strlen(get_prompt())); // send prompt

        pthread_t keepalive_thread_id = start_keepalive_daemon(client_sock, server_sock); // start keep alive 

        process_current_server_inputting(client_sock, server_sock, buffer, NULL); // process client input
        
        end_current_connection(client_sock, keepalive_thread_id); // end connection
    }

    close(server_sock);
    exit(0); // Exit the server, and stop all sub-threads
}

void run_unix_server(const char *socket_path) {
    printf("Server is running on socket %s\n", socket_path);

    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_un server_addr; // server addres 
    server_addr.sun_family = AF_UNIX; // lockal socket
    // copy path to socket in adress struct
    // sun_path - path name 
    snprintf(server_addr.sun_path, sizeof(server_addr.sun_path), "%s", socket_path);

    unlink(server_addr.sun_path); // unlink path if exist to avoid "Address already in use"
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) { // try to bind
        perror("Bind failed");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) == -1) { // Prepare to accept connections on socket FD. ret -1 if error occurd .
        perror("Listen failed");
        close(server_sock);
        exit(1);
    }

    while(1){ // Granart server to work after client disconnect, and waitinf for new connection
        memset(&server_addr, 0, sizeof(server_addr)); // Clear the buffer(for server reconnection)

        printf("Waiting for connections on %s...\n", socket_path);
        int client_sock = accept(server_sock, NULL, NULL); // Await a connection on socket FD.
        if (client_sock == -1) { // error 
            perror("Accept failed");
            close(server_sock);
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        printf("Client connected\n");
        write(client_sock, get_prompt(), strlen(get_prompt())); // send prompt

        pthread_t keepalive_thread_id = start_keepalive_daemon(client_sock, server_sock); // start keep alive 

        process_current_server_inputting(client_sock, server_sock, buffer, socket_path); // process client input
        
        end_current_connection(client_sock, keepalive_thread_id); // end connection
    }

    close(server_sock);
    unlink(socket_path); // unlink path
    exit(0); // Exit the server, and stop all sub-threads
}
