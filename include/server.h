#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

void run_server(int port);
void run_unix_server(const char *socket_path);
// void log_command_execution(const char *command, const char *result, const char *prompt);
// Print the command, result and prompt on the server side

void process_command_execution(int client_sock, char *buffer);
int receive_client_input(int client_sock, char *buffer);

pthread_t start_keepalive_daemon(int client_sock, int server_sock);
void end_current_connection(int client_sock, pthread_t keepalive_thread_id);
void process_current_server_inputting(int client_sock, int server_sock, char *buffer, const char *socket_path);

#endif