#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>
#include <ctype.h>

#include <fcntl.h>
#include <sys/wait.h> // waitpid
// #include <asm-generic/fcntl.h>

// include functions 
#include "shell.h"
#include "utils.h"


char isServer = 0;
char isClient = 0;
int port = 0;
char *socketName = NULL;
char *ip = "127.0.0.1";

char *get_prompt() {
    static char prompt[256];

    // https://stackoverflow.com/questions/5141960/get-the-current-time-in-c
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if (!local_time) perror("localtime");

    char time_string[6];
    if (strftime(time_string, sizeof(time_string), "%H:%M", local_time) == 0) {
        fprintf(stderr, "strftime failed\n");
        strncpy(time_string, "00:00", sizeof(time_string) - 1);
        time_string[sizeof(time_string) - 1] = '\0';
    }

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) perror("getpwuid");

    char hostname[32];
    if (gethostname(hostname, sizeof(hostname)) != 0)  perror("gethostname");

    snprintf(prompt, sizeof(prompt), "%s %s@%s$", time_string, pw->pw_name, hostname);

    return prompt;
}

// void send_file(int client_sock, const char *file_path) {
//     printf("TODO: send file %s\n", file_path);
// }

// void receive_file(int sock, const char *output_file) {
//     printf("TODO: receive file %s\n", output_file);
// }


char *shell_process_command(const char *command) {
    if (strcmp(command, "help") == 0) {
        return cmd_help();
    } else if (strcmp(command, "quit") == 0) {
        return cmd_quit();
    } else if (strcmp(command, "halt") == 0) {
        return cmd_halt();
    } else if (strcmp(command, "whoami") == 0) {
        return cmd_whoami();
    } else if (strcmp(command, "hostname") == 0) {
        return cmd_hostname();
    } else if (strcmp(command, "pwd") == 0) {
        return cmd_pwd();
    } else if (strncmp(command, "cd ", 3) == 0) {
        const char *path = command + 3;
        return cmd_cd(path);
    } else if (strcmp(command, "ls") == 0) {
        return cmd_ls();
    } else if (strncmp(command, "cat ", 4) == 0) {
        const char *file_path = command + 4;
        return cmd_cat(file_path);
    } else if (strncmp(command, "echo ", 5) == 0) {
        const char *message = command + 5;
        return strdup(message); // Duplicate S, returning an identical malloc'd string.
    } else {
        return "";
    }
}


char *cmd_help() {
    return "Available commands:\n"
           "help - display this help message\n"
           "quit - close the connection\n"
           "halt - stop the server\n"
           "whoami - display the current username\n"
           "hostname - display the hostname\n"
           "pwd - display the current directory\n"
           "cd <path> - change the current directory\n"
           "ls - list files in the current directory\n"
           "cat <file_path> - display the contents of a file\n"
           "echo <message> - display a message\n"
           "Note: commands are case-sensitive"
           "ls > file.txt - save the output of ls to a file\n";
}

char *cmd_quit() {
    return "Connection closed";
}

char *cmd_halt() {
    return "Server stopped";
}

char *cmd_whoami() {
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) return pw->pw_name;
    else return "Error to get username";
}

char *cmd_hostname() {
    static char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)  return hostname;
    else return "Error to get hostname";
}

char *cmd_pwd() {
    static char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL) return cwd;
    else return "Error to get current directory";
}

char *cmd_cd(const char *path) {
    if (chdir(path) == 0) return cmd_pwd();
    else return "Failed to change directory";
}

char *cmd_ls() {
    static char ls_buffer[1024];
    memset(ls_buffer, 0, sizeof(ls_buffer));

    DIR *dir = opendir(".");
    if (dir == NULL) {
        return "Failed to open directory";
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        strncat(ls_buffer, entry->d_name, sizeof(ls_buffer) - strlen(ls_buffer) - 1);
        strncat(ls_buffer, "\n", sizeof(ls_buffer) - strlen(ls_buffer) - 1);
    }

    closedir(dir);
    return ls_buffer;
}

char *cmd_cat(const char *file_path) {
    static char cat_buffer[4096];
    memset(cat_buffer, 0, sizeof(cat_buffer));

    FILE *file = fopen(file_path, "r");
    if (file == NULL) return "Failed to open file";

    char *current_pos = cat_buffer;
    size_t rem_size = sizeof(cat_buffer) - 1;
    size_t n;

    while ((n = fread(current_pos, 1, rem_size, file)) > 0) {
        current_pos += n;
        rem_size -= n;

        if (rem_size == 0) break;
    }

    fclose(file);
    return cat_buffer;
}

// char *cmd_send(int client_sock, const char *file_path) {














































char *execute_command(char **args, char *input_file, char *output_file) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return "Pipe error";
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fd[0]);

        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else {
            dup2(pipe_fd[1], STDOUT_FILENO);
        }

        close(pipe_fd[1]);

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("fork");
        return "Fork error";
    } else {
        close(pipe_fd[1]);

        int status;
        waitpid(pid, &status, 0);

        char buffer[4096];
        ssize_t count = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        if (count < 0) {
            perror("read");
            close(pipe_fd[0]);
            return "Read error";
        }
        buffer[count] = '\0';

        close(pipe_fd[0]);
        return strdup(buffer);
    }

    return NULL;
}

void parse_redirections(char *command, char **input_file, char **output_file) {
    char *redir_token;

    if ((redir_token = strchr(command, '<')) != NULL) {
        *redir_token = '\0';
        *input_file = strtok(redir_token + 1, " ");
        *input_file = trim(*input_file);
    }

    if ((redir_token = strchr(command, '>')) != NULL) {
        *redir_token = '\0';
        *output_file = strtok(redir_token + 1, " ");
        *output_file = trim(*output_file);
    }
}

char *shell_process_input(char *command) {
    char *output = NULL;

    char *comment_position = strchr(command, '#');
    if (comment_position) *comment_position = '\0';

    if (strchr(command, ';') == NULL) {
        char *trimmed_command = trim(command);
        char *input_file = NULL;
        char *output_file = NULL;

        parse_redirections(trimmed_command, &input_file, &output_file);

        char *args[64];
        int i = 0;
        char *arg = strtok(trimmed_command, " ");
        while (arg != NULL) {
            args[i++] = arg;
            arg = strtok(NULL, " ");
        }
        args[i] = NULL;

        output = execute_command(args, input_file, output_file);
    } else {
        char *commands = strtok(command, ";");
        while (commands != NULL) {
            char *trimmed_command = trim(commands);

            char *input_file = NULL;
            char *output_file = NULL;

            parse_redirections(trimmed_command, &input_file, &output_file);

            char *args[64];
            int i = 0;
            char *arg = strtok(trimmed_command, " ");
            while (arg != NULL) {
                args[i++] = arg;
                arg = strtok(NULL, " ");
            }
            args[i] = NULL;

            char *sub_output = execute_command(args, input_file, output_file);
            if (output == NULL) {
                output = strdup(sub_output);
            } else {
                output = realloc(output, strlen(output) + strlen(sub_output) + 2);
                strcat(output, "\n");
                strcat(output, sub_output);
            }

            commands = strtok(NULL, ";");
        }
    }

    return output ? output : strdup("");
}