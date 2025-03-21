// #ifndef SHELL_H
// #define SHELL_H

// void help();
// char *get_prompt();
// void run_server(int port);
// void run_client(int port);

// #endif

#ifndef SHELL_H
#define SHELL_H

extern char isServer;
extern char isClient;
extern int port;
extern char *socketName;
extern char *ip;

char *get_prompt();
void send_file(int client_sock, const char *file_path);
void receive_file(int sock, const char *output_file);
char *shell_process_command(const char *command, int client_sock);

void run_server(int port);
void run_unix_server(const char *socket_path);
void run_client(int port);
void run_unix_client(const char *socket_path);

#endif // SHELL_H