#include "keepalive.h"
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> // For kill() and SIGTERM

#include <unistd.h>  // For sleep() and close()
#include <pthread.h> // FOr pthread_create() and pthread_exit()

#define check(expr) if (!(expr)) { perror(#expr); kill(0, SIGTERM); }

// global instance
// KeepAlive keep_alive = {5, 3, 30}; // 5 seconds interval, 3 probes, 30 seconds timeout
KeepAlive keep_alive = {5, 1, 10};

void enable_keepalive(int sock) {
    int yes = 1; // Enable TCP keep-alive
    check(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) != -1);

    int idle = keep_alive.interval; // Time before sending the first keep-alive probe
    check(setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)) != -1);

    int interval = keep_alive.interval; // Interval between keep-alive probes
    check(setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)) != -1);

    int maxpkt = keep_alive.probes; // Maximum num. of keep-alive probes
    check(setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) != -1);

    // Configure receive timeout
    struct timeval timeout;
    timeout.tv_sec = keep_alive.timeout; //seconds
    timeout.tv_usec = 0; // microseconds

    // Set receive timeout
    check(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != -1);

    // Set send timeout
    check(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != -1);
}


void *keepalive_thread(void *arg) {
    KeepAliveArgs *args = (KeepAliveArgs *)arg;

    while (1) { // Check every second
        sleep(1);

        time_t current_time = time(NULL);
        if (difftime(current_time, *(args->last_activity_time)) > args->timeout) {
            printf("Connection closed due to inactivity (Keep-Alive timeout)\n");
            close(args->sock);
            pthread_exit(NULL); // Exit the thread
        }
    }

    return NULL;
}