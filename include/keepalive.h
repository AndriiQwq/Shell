#ifndef KEEPALIVE_H
#define KEEPALIVE_H

#include <time.h>

typedef struct {
    time_t interval;
    unsigned int probes;
    time_t timeout;
} KeepAlive;

typedef struct {
    int sock;
    time_t *last_activity_time;
    int timeout;
} KeepAliveArgs;

extern KeepAlive keep_alive; // Define the global KeepAlive instance
void enable_keepalive(int sock);
void *keepalive_thread(void *arg);

#endif // KEEPALIVE_H