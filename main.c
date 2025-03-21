#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "shell.h"

extern char isServer;
extern char isClient;
extern int port;
extern char *socketName;
extern char *ip;

void help();

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
}

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
}