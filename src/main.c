#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h> //kill and SIGTERM
#include <sys/wait.h> // waitpid

#include <pthread.h>

#include "shell.h"
#include "keepalive.h"

#include "logger.h"
#include "env_loader.h"

// Global variables
// extern char isServer;
// extern char isClient;
// extern int port;
// extern char *socketName;
// extern char *ip;

// Defoutlt settings
char isServer = 0;
char isClient = 0;
int port = 0;
char *socketName = NULL;
char *ip = "127.0.0.1";

//void *server_thread(void *arg);

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("%s was runing as server\n", argv[0]);
        isServer = 1;
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-s") == 0) {
                printf("Server was running\n");
                isServer = 1;
            } else if (strcmp(argv[i], "-c") == 0) {
                // Internal cmmand
                if (i + 1 < argc && strcmp(argv[i + 1], "-halt") == 0) {
                    printf("Halting system...\n");
                    exit(0);
                } else if (i + 1 < argc && strcmp(argv[i + 1], "-quit") == 0) {
                    printf("Quiting system...\n");
                    exit(0);
                }

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
                    socketName = argv[++i];
                    printf("Socket set to %s\n", socketName);
                } else {
                    printf("Socket not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-h") == 0) {
                printf("%s", command_help());
                return 0;
            } else if (strcmp(argv[i], "-l") == 0) {
                if (i + 1 < argc) {
                    log_file_path = argv[++i];
                    printf("Log file path set to %s\n", log_file_path);
                } else {
                    printf("Error, log file not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-t") == 0) {
                if (i + 1 < argc) {
                    keep_alive.timeout = atoi(argv[++i]);
                    printf("Inactivity timeout set to %ld seconds\n", keep_alive.timeout);
                } else {
                    printf("Error, timeout not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-i") == 0) {
                if (i + 1 < argc) {
                    ip = argv[++i];
                    printf("IP set to %s\n", ip);
                } else {
                    printf("Error, IP not provided\n");
                    return 1;
                }
            } else if (strcmp(argv[i], "-C") == 0) {
                if (i + 1 < argc) {
                    config_path = argv[++i];
                    printf("Config file path set to %s\n", config_path);
                } else {
                    printf("Error, config path not provided\n");
                    return 1;
                }
            } else {
                printf("Error occcured, unknown argument.");
                printf("%s", command_help());
                return 1;
            }
        }
    }

    // Load environment variables from .env file
    if (config_path) load_env_file(config_path);

    // Get enveronment variables, if .env file exists
    char *keepalive_interval_env = getenv("KEEPALIVE_INTERVAL");// get .env keep_alive interval time
    if (keepalive_interval_env) {
        keep_alive.interval = (unsigned int)atoi(keepalive_interval_env); //convert to int 
    }    

    // keep_alive.probes = getenv("KEEPALIVE_PROBES") ? (unsigned int) atoi(getenv("KEEPALIVE_PROBES")) : keep_alive.probes;
    // keep_alive.timeout = getenv("KEEPALIVE_TIMEOUT") ? (unsigned int) atoi(getenv("KEEPALIVE_TIMEOUT")) : keep_alive.timeout;

    // Get log file path from environment variable
    char *log_file_path_env = getenv("LOG_FILE_PATH");
    if (log_file_path_env) {
        log_file_path = log_file_path_env;
        printf("Log file path set to %s from evirement file.\n", log_file_path);
    }
    
    // Log 
    if(log_file_path != NULL && log_file_exists(log_file_path) == 0) {
        create_log_file(log_file_path);
        if (isClient) {
            write_log("SYSTEM", "START CLIENT");
        } else {
            write_log("SYSTEM", "START SERVER");
        }
    } else if(log_file_path != NULL && log_file_exists(log_file_path) != 0){
        if (isClient) {
            write_log("SYSTEM", "RESTART CLIENT");
        } else {
            write_log("SYSTEM", "RESTART SERVER");
        }
    }

    if (isServer) {
        pid_t pid = fork(); // Create a child process
        if (pid < 0) {
            perror("Failed to fork server process");
            return 1;
        } else if (pid == 0) {
            if (socketName) {
                run_unix_server(socketName);
            } else {
                run_server(port);
            }
            exit(0); // Exit child process after server stop
        } else {
            // Parent process, continue to run shell
            printf("Server started in process %d\n", pid);
        }

        sleep(1);
        printf("%s", get_prompt());

        char buffer[1024];
        while (1) { // Internal server shell loop
            fgets(buffer, 1024, stdin);

            if (strlen(buffer) == 1 && buffer[0] == '\n') {
                printf("%s", get_prompt());
                continue;
            }

            buffer[strcspn(buffer, "\n")] = 0;

            // LOG
            write_log("SERVER", buffer);

            if (strcmp(buffer, "quit") == 0) { // Stop Server
                printf("Quiting system.\n");
                kill(pid, SIGTERM); // Send signal to stop server
                waitpid(pid, NULL, 0); // Wait for finish child process
                exit(0);
            }
    
            if (strcmp(buffer, "halt") == 0) { // Stop all conection and exit program
                printf("Halting system.\n");
                kill(pid, SIGTERM); // Send signal to stop server
                waitpid(pid, NULL, 0); // Wait for finish child process
                exit(0);
            }

            // whell in server 
            char *result = shell_process_input(buffer);
            if(result) {
                printf("%s\n", result);
                printf("%s", get_prompt());
                
                free(result);
            } else {
                printf("%s", get_prompt());
            }
        }
    } else if (isClient) {
        if (socketName) {
            run_unix_client(socketName);
        } else {
            run_client(port);
        }
    } else {
        printf("%s", command_help());
        return 1;
    }

    return 0;
}
