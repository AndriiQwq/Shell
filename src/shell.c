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

#define TOK_BUFSIZE 64 // define tok buffer size 
#define TOK_DELIM " \t\r\n\a" // def telime

char *get_prompt() {
    static char prompt[256];

    // https://stackoverflow.com/questions/5141960/get-the-current-time-in-c
    char *current_time = get_current_time();

    uid_t uid = getuid(); // Get the real user ID of the calling process.
    struct passwd *pw = getpwuid(uid); // Retrieve the user database entry for the given user ID.
    if (!pw) perror("getpwuid");

    char hostname[32];
    if (gethostname(hostname, sizeof(hostname)) != 0)  perror("gethostname"); //Put the name of the current host

    // copy to prompt 
    snprintf(prompt, sizeof(prompt), "%s %s@%s$", current_time, pw->pw_name, hostname);

    free(current_time);
    return prompt;
}


char **split_command(const char *command) {
    char *command_copy = strdup(command);
    if (!command_copy) {
        perror("strdup");
        return NULL;
    }
    int bufsize = TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;
    if (!tokens) {
        free(command_copy);
        return NULL;
    }
    token = strtok(command_copy, TOK_DELIM);
    while (token != NULL) {
        tokens[position] = strdup(token);
        position++;
        if (position >= bufsize) {
            bufsize += TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                free(command_copy);
                return NULL;
            }
        }
        token = strtok(NULL, TOK_DELIM);
    }
    tokens[position] = NULL;
    
    free(command_copy);
    return tokens;
}

void free_args(char **args) {
    if (!args) return;
    
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

// Launching external commands
char *launch_process(const char *command) {
    char **args = split_command(command);
    if (!args || !args[0]) {
        if (args) free_args(args);
        return strdup("Invalid command");
    }
    pid_t pid;
    int pipefd[2];
    
    // Create a pipe 
    if (pipe(pipefd) == -1) {
        perror("pipe");
        free_args(args);
        return strdup("Failed to create pipe");
    }
    pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]); // close pipe read 
        
        // Stdout -> pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Execute command
        if (execvp(args[0], args) == -1) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), "Command not found: %s", args[0]);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            exit(EXIT_FAILURE);
        }
        
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        // Fork error
        perror("fork");
        free_args(args);
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("Failed to fork process");
    } else {
        // Parent process
        close(pipefd[1]);
        
        // Read pipe output
        char buffer[4096];
        ssize_t bytes_read;
        char *result = malloc(1);
        if (!result) {
            free_args(args);
            close(pipefd[0]);
            return strdup("Memory allocation error");
        }
        result[0] = '\0';
        size_t total_bytes = 0;
        
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            char *new_result = realloc(result, total_bytes + bytes_read + 1);
            if (!new_result) {
                free(result);
                free_args(args);
                close(pipefd[0]);
                return strdup("Memory allocation error");
            }
            result = new_result;
            strcat(result, buffer);
            total_bytes += bytes_read;
        }
        
        close(pipefd[0]);
        
        // Waiting for finish child process
        int status;
        waitpid(pid, &status, 0);
        
        free_args(args);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return result[0] ? result : strdup("");
        } else {
            if (result[0]) {
                return result;
            } else {
                free(result);
                return strdup("Command execution failed");
            }
        }
    }
}



char *shell_process_command(const char *command) {
    if (strcmp(command, "help") == 0) {
        return command_help();
    } else if (strcmp(command, "quit") == 0) {
        return command_quit();
    } else if (strcmp(command, "halt") == 0) {
        return command_halt();
    } else if (strncmp(command, "run ", 4) == 0) {  // Run script file
        const char *script_path = command + 4;
        char *script_output = process_script(script_path);
        return script_output;
    } else if (strncmp(command, "cd ", 3) == 0) { // Change directory
        const char *dir = command + 3;
        if (chdir(dir) != 0) {
            perror("chdir");
            return strdup("Failed to change directory");
        }
        return strdup("");
    } else {
        return launch_process(command);
    }
}


char *command_help() {
    return (
        "Author: Andrii Dokaniev\n"
        "Date: 4/12/2025\n"
        "Usage: \n"
        "   ./shell ...\n"
        "   make run ARGS=\"-s -p 8071 -C .env\"\n"
        "Options:\n"
        "  -s -  Run as server\n"
        "  -c -  Run as client\n"
        "  -p port - Specify port number\n"
        "  -u socket - Specify socket name\n"
        "  -i ip - Specify IP address\n"
        "  -h - Show help message\n"
        "  -t timeout - Set inactivity timeout\n"
        "  -C \n"
        "  -l path - Set output for log file\n"
        "\n"
        "Available commands:\n"
        "    help - display the help message\n"
        "    quit - close the connection\n"
        "    halt - stop the server\n"
        "    cd <directory> - change the directory\n"
        "    run <script_path> - run a script on the server\n"
        "    And other commands which are available in the shell\n"
        "Special symbols:\n"
        "  ; - separate multiple commands\n"
        "  < - redirect input from a file\n"
        "  > - redirect output to a file\n"
        "  # - comment\n"
        "  | - pipe\n"
        "Bonus tasks:\n"
        "    1, 7, 11, 12, 13, 15, 16, 18, 20, 21, 23"
        "Input example:\n"
        "    wc -l < output.txt\n"
        "    ls > output.txt\n"
        "    pwd; ls; hostname;;;;\n"
        "    cat output.txt | wc -l\n"
    );
}

char *command_quit() {
    return "Connection closed";
}

char *command_halt() {
    return "Server stopped";
    exit(0);
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


char *process_redirection_input(char *command, char *input_file) {
    input_file = trim(input_file);

    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return strdup("Failed to open input file");
    }

    pid_t pid;
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        close(fd);
        return strdup("Failed to create pipe");
    }

    pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(fd, STDIN_FILENO);
        close(fd);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char **args = split_command(command);
        if (!args || !args[0]) {
            free_args(args);
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args);
        dprintf(STDERR_FILENO, "Command not found: %s\n", args[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        close(fd);
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("Failed to fork process");
    } else {
        close(fd);
        close(pipefd[1]);

        char buffer[4096];
        ssize_t bytes_read;
        char *result = malloc(1);
        if (!result) {
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
            return strdup("Memory allocation error");
        }
        result[0] = '\0';
        size_t total_bytes = 0;

        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            char *new_result = realloc(result, total_bytes + bytes_read + 1);
            if (!new_result) {
                free(result);
                close(pipefd[0]);
                waitpid(pid, NULL, 0);
                return strdup("Memory allocation error");
            }
            result = new_result;
            strcat(result, buffer);
            total_bytes += bytes_read;
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return result[0] ? result : strdup("");
        } else {
            if (result[0]) {
                return result;
            } else {
                free(result);
                return strdup("Command execution failed");
            }
        }
    }
}

char *process_redirection_output(char *command, char *output_file) {
    output_file = trim(output_file);

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return strdup("Failed to open output file");
    }

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        close(fd);

        char **args = split_command(command);
        if (!args || !args[0]) {
            free_args(args);
            exit(EXIT_FAILURE);
        }
        execvp(args[0], args);
        dprintf(STDERR_FILENO, "Command not found: %s\n", args[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        close(fd);
        return strdup("Failed to fork process");
    } else {
        close(fd);
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return strdup("");
        } else {
            return strdup("Command execution failed");
        }
    }
}

char *shell_process_input(char *command) {
    if (!command || !*command) return strdup("");

    char *comment_position = strchr(command, '#');
    if (comment_position) *comment_position = '\0';

    char *semicolon = strchr(command, ';');
    if (semicolon) {
        char *command_copy = strdup(command);
        if (!command_copy) return strdup("Memory allocation error");
        
        char *saveptr = NULL;
        char *current_command = strtok_r(command_copy, ";", &saveptr);
        char *result = strdup("");
        size_t result_len = 0;
        
        while (current_command) {
            char *trimmed_command = trim(current_command);
            if (*trimmed_command) {
                char *cmd_output = shell_process_input(trimmed_command);
                if (cmd_output) {
                    size_t cmd_len = strlen(cmd_output);
                    if (cmd_len > 0) {
                        char *new_result = realloc(result, result_len + cmd_len + 2);
                        if (!new_result) {
                            free(cmd_output);
                            free(result);
                            free(command_copy);
                            return strdup("Memory allocation error");
                        }
                        
                        result = new_result;
                        if (result_len > 0) {
                            strcat(result, "\n");
                            result_len++;
                        }
                        strcat(result, cmd_output);
                        result_len += cmd_len;
                    }
                    free(cmd_output);
                }
            }
            
            current_command = strtok_r(NULL, ";", &saveptr);
        }
        
        free(command_copy);
        return result;
    }

    // Pipe handling
    char *pipe_position = strchr(command, '|');
    if (pipe_position) {
        *pipe_position = '\0';
        char *first_command = trim(command);
        char *second_command = trim(pipe_position + 1);

        if (!*first_command || !*second_command) {
            return strdup("Invalid pipe command");
        }

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            return strdup("Failed to create pipe");
        }

        pid_t pid1 = fork();
        if (pid1 == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            char **args = split_command(first_command);
            if (!args || !args[0]) {
                free_args(args);
                exit(EXIT_FAILURE);
            }
            execvp(args[0], args);
            dprintf(STDERR_FILENO, "Command not found: %s\n", args[0]);
            exit(EXIT_FAILURE);
        }

        pid_t pid2 = fork();
        if (pid2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            char **args = split_command(second_command);
            if (!args || !args[0]) {
                free_args(args);
                exit(EXIT_FAILURE);
            }
            execvp(args[0], args);
            dprintf(STDERR_FILENO, "Command not found: %s\n", args[0]);
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        char *result = malloc(1);
        if (!result) {
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            return strdup("Memory allocation error");
        }
        result[0] = '\0';
        size_t total_bytes = 0;

        int pipefd_out[2];
        if (pipe(pipefd_out) == -1) {
            free(result);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            return strdup("Failed to create output pipe");
        }

        close(pipefd_out[1]);

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd_out[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            char *new_result = realloc(result, total_bytes + bytes_read + 1);
            if (!new_result) {
                free(result);
                close(pipefd_out[0]);
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
                return strdup("Memory allocation error");
            }
            result = new_result;
            strcat(result, buffer);
            total_bytes += bytes_read;
        }
        close(pipefd_out[0]);

        int status1, status2;
        waitpid(pid1, &status1, 0);
        waitpid(pid2, &status2, 0);

        if (WIFEXITED(status1) && WEXITSTATUS(status1) == 0 &&
            WIFEXITED(status2) && WEXITSTATUS(status2) == 0 &&
            total_bytes > 0) {
            if (total_bytes > 0 && result[total_bytes - 1] == '\n') {
                result[total_bytes - 1] = '\0';
            }
            return result;
        }

        free(result);
        return strdup(WIFEXITED(status2) && WEXITSTATUS(status2) == 0 ? "" : "Pipe execution failed");
    }

    // Redirections
    char *redirection_pos = NULL;
    
    if ((redirection_pos = strchr(command, '>'))) {
        *redirection_pos = '\0';
        char *cmd_part = trim(command);
        char *file_part = trim(redirection_pos + 1);
        
        return process_redirection_output(cmd_part, file_part);
    }
    
    if ((redirection_pos = strchr(command, '<'))) {
        *redirection_pos = '\0';
        char *cmd_part = trim(command);
        char *file_part = trim(redirection_pos + 1);
        
        return process_redirection_input(cmd_part, file_part);
    }
    
    return shell_process_command(command);
}