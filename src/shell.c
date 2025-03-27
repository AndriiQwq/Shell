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
    char *current_time = get_current_time();

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) perror("getpwuid");

    char hostname[32];
    if (gethostname(hostname, sizeof(hostname)) != 0)  perror("gethostname");

    snprintf(prompt, sizeof(prompt), "%s %s@%s$", current_time, pw->pw_name, hostname);

    free(current_time);
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
        return strdup(cmd_cat(file_path));
    } else if (strncmp(command, "echo ", 5) == 0) {
        const char *message = command + 5;
        return strdup(message); // Duplicate S, returning an identical malloc'd string.
    } else {
        return "";
    }
}


char *cmd_help() {
    return (
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
        "  -t timeout  Set inactivity timeout\n"
        "\n"
        "Available commands:\n"
           "help - display the help message\n"
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
           "ls > file.txt - save the output of ls to a file\n"
    );
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
    static char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) return cwd;
    else return "Error to get current directory";
}

char *cmd_cd(const char *path) {
    if (chdir(path) == 0) return cmd_pwd();
    else return "Failed to change directory";
}

char *cmd_ls() {
    static char ls[1024];
    memset(ls, 0, sizeof(ls));

    DIR *dir = opendir(".");
    if (dir == NULL) {
        return "Failed to open directory";
    }

    struct dirent *node;
    while ((node = readdir(dir)) != NULL) {
        strncat(ls, node->d_name, sizeof(ls) - strlen(ls) - 1);
        strncat(ls, "\n", sizeof(ls) - strlen(ls) - 1);
    }

    closedir(dir);
    return ls;
}

// //https://github.com/bddicken/languages/blob/eaf4b48be17827f254f8ff08aca217a9605bbcc3/levenshtein/c/run.c
char *cmd_cat(const char *file_path) {
    // First read entire file content
    FILE* file = fopen(file_path, "r");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", file_path);
        return ("Failed to open file");
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return ("Failed to allocate memory");
    }

    fread(content, 1, file_size, file);
    content[file_size] = '\0';

    fclose(file);
    return (content);
}


char * read_file(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return strdup("Failed to open file");
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    // fseek(file, 0, SEEK_SET);
    rewind(file); 

    char *content = malloc((size_t)file_size + 1);
    if (!content) {
        fclose(file);
        return strdup("Failed to allocate memory");
    }

    size_t read_size = fread(content, 1, file_size, file);
    if (read_size != (size_t)file_size) {
        free(content);
        fclose(file);
        return strdup("Failed to read file");
    }
    content[file_size] = '\0';

    fclose(file);
    return content;
}

void  write_file(const char* filename, const char* content) {
    if (content) {
        FILE *file = fopen(filename, "w");
        if (!file) {
            perror("Error to open file");
            return;
        }
        fputs(content, file);
        fclose(file);
    }
}




































char *shell_process_input(char *command) {
    char *output = NULL;

    char *comment_position = strchr(command, '#');
    if (comment_position) *comment_position = '\0';

    if (strchr(command, ';') == NULL) { // Only one command
        char *trimmed_command = trim(command);
        char *input_file = NULL;
        char *output_file = NULL;
        char *redirection = NULL;

        int redirection_flag = 0;
        if ((redirection = strchr(command, '<')) != NULL) {
            redirection_flag = 1;

            *redirection = '\0';
            input_file = strtok(redirection + 1, " "); // extract filename, string after '<'
            input_file = trim(input_file); //trim name of the file 

            trimmed_command = trim(command);
        }
    
        if ((redirection = strchr(command, '>')) != NULL) {
            redirection_flag = 2;

            *redirection = '\0';
            output_file = strtok(redirection + 1, " "); // extract filename, string after '>'
            output_file = trim(output_file); // trim name of the file

            trimmed_command = trim(command);
        }

        if (redirection_flag == 1) {
            if(strcmp(trimmed_command, "cat" ) == 0) {
                char modified_command[512];
                snprintf(modified_command, sizeof(modified_command), "cat %s", input_file);

                output = shell_process_command(modified_command);
            } else {

                char *file_content = read_file(input_file);
                if (file_content) {
                    output = shell_process_command(file_content);
                    free(file_content);
                }
            }
        } else if (redirection_flag == 2) {
            char *command_output = shell_process_command(trimmed_command);
            if (command_output) {
                write_file(output_file, command_output);
                free(command_output);
            }
        } else {
            output = shell_process_command(trimmed_command);
        }
    } else {
        char *current_command = strtok(command, ";");
        size_t result_size = 0;

        while (current_command != NULL) {
            char *trimmed_command = trim(current_command);

            char *command_output = shell_process_input(trimmed_command);
            if (command_output) {
                size_t command_output_len = strlen(command_output);

                output = realloc(output, result_size + command_output_len + 2);
                if (!output) {
                    free(command_output);
                    free(command);
                    return strdup("Error: Memory allocation failed");
                }

                strcpy(output + result_size, command_output);
                result_size += command_output_len;

                if(output && (output[0] != '\n' || output[0] != '\0') ) {
                    output[result_size] = '\n';
                    result_size++;
                }

                free(command_output);
            }

            current_command = strtok(NULL, ";");
        }

        if (output) {
            output[result_size - 1] = '\0';
        }

    }

    // return output ? output : strdup("");
    return output ? strdup(output) : strdup("");
}