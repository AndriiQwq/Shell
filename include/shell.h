#ifndef SHELL_H
#define SHELL_H

extern char isServer;
extern char isClient;
extern int port;
extern char *socketName;
extern char *ip;

// void send_file(int client_sock, const char *file_path);
// void receive_file(int sock, const char *output_file);

char check_internal_command(const char *command);

char *get_prompt();
char *shell_process_command(const char *command);
char *shell_process_input(char *command);
// char *execute_pipeline(char *command);

void run_server(int port);
void run_unix_server(const char *socket_path);
void run_client(int port);
void run_unix_client(const char *socket_path);

char **split_command(const char *command);
void free_args(char **args);
char *launch_process(const char *command);

char *command_help();
char *command_quit();
char *command_halt();

char *process_script(const char *script_path);
// char *command_send(int client_sock, const char *file_path);

void  write_file(const char* filename, const char* content);
char * read_file(const char* filename);

char *process_semicolon(char *command);
char *process_redirection_input(char *command, char *input_file);
char *process_redirection_output(char *command, char *output_file);
char *process_pipe(char *command);

#endif // SHELL_H