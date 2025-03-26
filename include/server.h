#ifndef SERVER_H
#define SERVER_H

void run_server(int port);
void run_unix_server(const char *socket_path);
void log_command_execution(const char *command, const char *result, const char *prompt);
// Print the command, result and prompt on the server side

#endif