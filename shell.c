#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <time.h> // For time functions
#include <pwd.h> // For getpwuid

#include <sys/wait.h> // For system calls
#include <sys/socket.h> // For socket definitions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h> // For inet_pton
#include <fcntl.h>
#include <sys/un.h> // For sockaddr_un, local sockets
#include <dirent.h> // For directory operations (opendir, readdir, etc.)

// INCLUDES HEADER AND SOURCE FILES
#include "config.h"
#include "shell.h"

char isServer = 0;
char isClient = 0;
int port = 0;
char *socketName = NULL;
char *ip = "127.0.0.1";

void help() {
    printf(
            "Shell\n"
            "Author: Andrii Dokaniev\n"
            "Purpose: Simple and speed client/server shell, optimized for concrete tasks\n"
            "Usage: shell [-s | -c] [-p port] [-u socket]\n"
            "Options:\n"
            "  -s          Run as server\n"
            "  -c          Run as client\n"
            "  -p port     Specify port number\n"
            "  -u socket   Specify socket name\n"
            "  -i ip       Specify IP address\n"
            "  -h          Show help message\n"
            "\n"
            "Commands:\n"
            "  help        Show help message\n"
            "  quit        Close the connection\n"
            "  halt        Stop the server\n");

    // quit - ukončenie spojenia z ktorého príkaz prišiel
    // halt - ukončenie celého programu. 
}

char *get_prompt() {
    static char prompt[512]; // Используем статический буфер для строки

    // Получение UID текущего пользователя
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        perror("getpwuid");
        return NULL;
    }

    // Получение имени хоста
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return NULL;
    }

    // Получение текущего времени
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if (!local_time) {
        perror("localtime");
        return NULL;
    }

    // Форматирование времени (HH:MM)
    char time_string[6]; // Достаточно места для формата HH:MM + null-терминатор
    if (strftime(time_string, sizeof(time_string), "%H:%M", local_time) == 0) {
        fprintf(stderr, "strftime failed\n");
        return NULL;
    }

    // Создание строки prompt с защитой от переполнения
    if (snprintf(prompt, sizeof(prompt), "%s %s@%s$", time_string, pw->pw_name, hostname) >= (int)sizeof(prompt)) {
        fprintf(stderr, "Prompt string was truncated\n");
        return NULL;
    }

    return prompt;
}




void send_file(int client_sock, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        write(client_sock, "ERROR: File not found\n", 22);
        return;
    }

    char buffer[1024];
    size_t bytes_read;

    // Send file contents in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (write(client_sock, buffer, bytes_read) < 0) {
            perror("Failed to send file");
            break;
        }
    }

    fclose(file);
    write(client_sock, "EOF", 3); // Send EOF to indicate the end of the file
}

void receive_file(int sock, const char *output_file) {
    FILE *file = fopen(output_file, "wb");
    if (file == NULL) {
        perror("Failed to create file");
        return;
    }

    char buffer[1024];
    ssize_t bytes_received;

    while ((bytes_received = read(sock, buffer, sizeof(buffer))) > 0) {
        // Check for EOF
        if (strncmp(buffer, "EOF", 3) == 0) {
            break;
        }

        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("File received and saved as %s\n", output_file);
}


char *shell_process_command(const char *command, int client_sock) {
    if (strcmp(command, "help") == 0) {
        return "Available commands: help, quit, halt";
    } else if (strcmp(command, "quit") == 0) {
        return "Connection closed";
    } else if (strcmp(command, "halt") == 0) {
        return "Server stopped";
    } else if (strcmp(command, "whoami") == 0) {
        uid_t uid = getuid();
        struct passwd *pw = getpwuid(uid);
        if (pw) 
            return pw->pw_name;
         else 
            return "Error to get username";
    } else if (strcmp(command, "hostname") == 0) {
        static char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0)
            return hostname;
        else 
            return "Error to get hostname";
    } else if (strcmp(command, "pwd") == 0) {
        static char cwd[512];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            return cwd;
        else
            return "Error to get current directory";
    } else if (strncmp(command, "cd ", 3) == 0) {
        const char *path = command + 3; // Extract the path after "cd "
        if (chdir(path) == 0) {
            return "Current directory changed";
        } else {
            return "Failed to change directory";
        }
    } else if (strcmp(command, "ls") == 0) {
        static char ls_buffer[1024];
        memset(ls_buffer, 0, sizeof(ls_buffer));
    
        DIR *dir = opendir(".");
        if (dir == NULL) {
            return "Failed to open directory";
        }
    
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            strncat(ls_buffer, entry->d_name, sizeof(ls_buffer) - strlen(ls_buffer) - 1);
            strncat(ls_buffer, "\n", sizeof(ls_buffer) - strlen(ls_buffer) - 1);
        }
    
        closedir(dir);
        return ls_buffer;
    } else if (strncmp(command, "send ", 9) == 0) {
        const char *file_path = command + 9;
        send_file(client_sock, file_path);
        return "File sent";
    } else {
        return "";
    }
}
















void run_server(int port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0); // TCP connection
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

    // Add tread for listening server commands(on server side)
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

        printf("%s\n",  buffer);
        printf("%s\n", result);
        printf("%s", get_prompt());

        // Process the command received from the client
        // Commands: help, quit, halt
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
            const char *output_file = buffer + 12; // Extract the output file name
            receive_file(client_sock, output_file);
            continue;
        } else {
            // write(client_sock, get_prompt(), strlen(get_prompt()));
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

    unlink(server_addr.sun_path); // Remove old socket if it exists
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

        printf("%s\n",  buffer);
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
        } else if (strcmp(buffer, "help") == 0) {
            write(client_sock, "Available commands: help, quit, halt", 36);
        } else {
            // write(client_sock, get_prompt(), strlen(get_prompt()));
        }
    }

    close(client_sock);
    close(server_sock);
    unlink(socket_path); // Remove socket on exit
}

void run_client(int port) {
    printf("Client is running on port %d\n", port);
    // try to connect to server on port(and ip)

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    char buffer[1024];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr); // Assuming server is running on localhost

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); // Connection failed(connection refused, ...)
        close(sock);
        exit(1);
    }

    read(sock, buffer, sizeof(buffer));
    printf("%s", buffer); // Print the prompt 

    while (1) {
        fgets(buffer, sizeof(buffer), stdin);

        // Check if the command is '\n' and skip it
        if (strlen(buffer) == 1 && buffer[0] == '\n') {
            write(sock, buffer, strlen(buffer));
            memset(buffer, 0, sizeof(buffer));
            read(sock, buffer, sizeof(buffer));
            printf("%s", buffer);
            continue;
        }

        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

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

        // Check if the command is '\n' and skip it
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


int main(int argc, char *argv[]) {
    /*
    Check arguments:
        - (_)  - run program as server
        - (-s) - run program as server
        - (-c) - run program as client
    */ 

    if (argc == 1) {
        printf("%s was runing as server\n", argv[0]);
        isServer = 1;
    } else{
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-s") == 0) {
                printf("Server was running\n");
                isServer = 1;
            } else if (strcmp(argv[i], "-c") == 0) {
                printf("Client was running\n");
                isClient = 1;
            } else if (strcmp(argv[i], "-p") == 0) {
                if (i + 1 < argc) {
                    port = atoi(argv[++i]);
                    printf("Port set to %d\n", port);
                } else {
                    printf("Error, port not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-u") == 0) {
                if (i + 1 < argc) {
                    char *socketName = argv[++i];
                    printf("Socket set to %s\n", socketName);
                } else {
                    printf("Socket not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-h") == 0) {
                help();
                return 0;
            } else if (strcmp(argv[i], "-i") == 0) {
                if (i + 1 < argc) {
                    ip = argv[++i];
                    printf("IP set to %s\n", ip);
                } else {
                    printf("Error, IP not provided\n");
                    return 1;
                }
            } else {
                printf("Error occcured, unknown argument.");
                help();
                return 1;
            }
        }
    }

    /*
    Check arguments for shell:
        - (-p) - set port
        - (-u) - name of local socket 
        - (-h) - information about program
    */ 

    /* 
    Check server command:
        - help - show help|both sides
        - quit - close coonection with server|client side
        - halt - close server(stop server program)|server side
        - and other commands
    */

    /*
    Check client arguments:
        - (-p) - set port
        - (-u) - name of local socket
        - (-h) - information about program
    */

    /* 
    Client sends commands to the server for execution. 
    The server returns the results to the client. 
    */

    /*
    Program start variation: 
    ./shell -p 8072
    ./shell -u /tmp/shell.sock
    <=>
    ./shell -s -p 8072|the server is listening on port 80722
    ./shell -s -u /tmp/shell.sock|the server listens through the /tmp/myshell.sock socket.

    ./shell -c -p 8072|the client connects to the server on port 8072
    ./shell -c -u /tmp/shell.sock|the client connects to the server through the /tmp/myshell.sock socket.

    ./shell -h|show help information
    */

    // "Shell" musí poskytovať 
    // aspoň nasledujúce interné príkazy: help - výpis informácií ako pri -h, quit - ukončenie 
    // spojenia z ktorého príkaz prišiel, halt - ukončenie celého programu.

    // + ) '-i' - IP adres 
    
    // 1) Read arguments and prcess them
    
    // -c -s // do 
    // -i -p // do, ip don't implemented
    // -u 
    // -h // do

    if (isServer) {
        if (socketName) {
            run_unix_server(socketName);
        } else {
            run_server(port);
        }
    } else if (isClient) {
        if (socketName) {
            run_unix_client(socketName);
        } else {
            run_client(port);
        }
    } else {
        help();
        return 1;
    }

    return 0;

    

    /// SYSTEM CALLS example
    // pid_t pid = fork();
    // if (pid == 0) {  // Child process
    //     execlp("ls", "ls", "-l", NULL);  
    //     perror("execlp");  
    //     exit(1);  
    // } else if (pid > 0) {  // Parent process
    //     wait(NULL);
    // } else {
    //     perror("fork");
    // }


    // /// REDIRECTING OUTPUT example
    // int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // if (fd < 0) {
    //     perror("open");
    //     return 1;
    // }

    // dup2(fd, STDOUT_FILENO);  // Перенаправляем stdout в файл
    // close(fd);  

    // execlp("ls", "ls", "-l", NULL);  // Запускаем команду
    // perror("execlp");




    return 0;
}

