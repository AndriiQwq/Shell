#ifndef CLIENT_H
#define CLIENT_H

void run_client(int port);
void run_unix_client(const char *socket_path);
void process_user_input(int sock, char *buffer);
void enable_keepalive(int sock);
// void check(int expr, const char *msg);

#endif