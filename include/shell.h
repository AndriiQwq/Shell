#ifndef SHELL_H
#define SHELL_H

extern char isServer;
extern char isClient;
extern int port;
extern char *socketName;
extern char *ip;

// void send_file(int client_sock, const char *file_path);
// void receive_file(int sock, const char *output_file);

char *get_prompt();
char *shell_process_command(const char *command);
char *shell_process_input(char *command);
// char *execute_pipeline(char *command);

void run_server(int port);
void run_unix_server(const char *socket_path);
void run_client(int port);
void run_unix_client(const char *socket_path);

char *cmd_help();
char *cmd_quit();
char *cmd_halt();
char *cmd_whoami();
char *cmd_hostname();
char *cmd_pwd();
char *cmd_cd(const char *path);
char *cmd_ls();
char *cmd_cat(const char *file_path);
// char *cmd_send(int client_sock, const char *file_path);

void  write_file(const char* filename, const char* content);
char * read_file(const char* filename);

#endif // SHELL_H