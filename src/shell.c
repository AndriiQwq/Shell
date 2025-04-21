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

#define TOK_BUFSIZE 64 // args buffer size 
#define TOK_DELIM " \t\r\n\a" // delimiters for tokenization command

char check_internal_command(const char *command) {
    if (strcmp(command, "help") == 0) return 1;
    if (strcmp(command, "quit") == 0) return 1;
    if (strcmp(command, "halt") == 0) return 1;
    if (strncmp(command, "run ", 4) == 0) return 1;
    if (strncmp(command, "cd ", 3) == 0) return 1;
    return 0;
}

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

// Split command into arguments 
// Using https://brennan.io/2015/01/16/write-a-shell-in-c/
char **split_command(const char *command) {
    char *command_copy = strdup(command); // duplicate command
    if (!command_copy) { return NULL; }

    int bufsize = TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        free(command_copy);
        fprintf(stderr, "lsh: allocation error\n");
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
                fprintf(stderr, "lsh: allocation error\n");
                return NULL;
            }
        }
        token = strtok(NULL, TOK_DELIM);
    }
    tokens[position] = NULL;
    
    free(command_copy);
    return tokens;
}

void free_args(char **args) { // free memory for args of tokens
    if (!args) return;
    
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

// Launching external commands
char *launch_process(const char *command) {
    char **args = split_command(command); // try to split command
    if (!args || !args[0]) { // handle error
        if (args) free_args(args);
        return strdup("Invalid command");
    }

    int pipefd[2]; // Pipe file descriptors, 0 for read, 1 for write
    if (pipe(pipefd) == -1) { // Create a pipe error
        free_args(args);
        return strdup("Failed to create pipe");
    }

    pid_t pid = fork(); // Fork a child process
    if (pid < 0) { // Fork error
        free_args(args);
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("Failed to fork process");
    } else if (pid == 0) { // Child process
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execvp(args[0], args);
        // fprintf(stderr, "Command not found: %s\n", args[0]);
        exit(1);
    }

    // Parent process
    close(pipefd[1]);
    char *result = NULL;
    size_t capacity = 0, total_bytes = 0;
    char buffer[1024]; // Smaller buffer for efficiency

    // Read from pipe
    ssize_t bytes_read; 
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';// add null terminator

        if (total_bytes + bytes_read >= capacity) {
            capacity = capacity ? capacity * 2 : 1024;

            char *new_result = realloc(result, capacity); // realloc memory if needed
            if (!new_result) {
                free(result);
                close(pipefd[0]);
                free_args(args);
                return strdup("Memory allocation error");
            }
            result = new_result;
        }
        memcpy(result + total_bytes, buffer, bytes_read + 1); // Copy buffer to result
        total_bytes += bytes_read; // Update total bytes read
    }
    close(pipefd[0]);

    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    free_args(args);

    // Check for errors
    if (bytes_read < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(result);
        return strdup("Command execution failed");
    }

    return result ? result : strdup("");
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
        if (chdir(dir) != 0) { return strdup("Failed to change directory"); }
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
        "  -s - Run as server\n"
        "  -c - Run as client\n"
        "  -p <port> - Specify port number\n"
        "  -u <socket> - Specify socket name\n"
        "  -i <ip> - Specify IP address\n"
        "  -h - Show help message\n"
        "  -t <timeout> - Set inactivity timeout\n"
        "  -C <path> - load .env file\n"
        "  -l <path> - Set output for log file\n"
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
        "    ps aux | grep shell\n"
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
    FILE *file = fopen(filename, "r"); // open file for reading
    if (!file) {  return strdup("Failed to open file"); } // handle error

    fseek(file, 0, SEEK_END); // move to the end of the file
    long file_size = ftell(file); // get length of the file
    // fseek(file, 0, SEEK_SET);
    rewind(file); // move to the beginning of the file

    char *content = malloc((size_t)file_size + 1); // allocate memory for file content
    if (!content) {
        fclose(file);
        return strdup("Failed to allocate memory");
    }

    size_t read_size = fread(content, 1, file_size, file); // read file content
    if (read_size != (size_t)file_size) {
        free(content);
        fclose(file);
        return strdup("Failed to read file");
    }
    content[file_size] = '\0'; // add null terminator

    fclose(file); // close file
    return content;
}

void  write_file(const char* filename, const char* content) {
    if (content) {
        FILE *file = fopen(filename, "w"); // open file for writing
        if (!file) { perror("Error to open file"); return;}
        fputs(content, file); // write content to file
        fclose(file);
    }
}


char *process_redirection_input(char *command, char *input_file) {
    input_file = trim(input_file); // trim command

    int fd = open(input_file, O_RDONLY);
    if (fd == -1) {return strdup("Failed to open input file"); }

    int pipefd[2]; // Pipe file descriptors, 0 for read, 1 for write
    if (pipe(pipefd) == -1) { // Create a pipe error
        close(fd);
        return strdup("Failed to create pipe");
    }

    pid_t pid = fork(); // Fork a child process
    if (pid < 0) { // Fork error
        close(fd);
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("Failed to fork process");
    } else if (pid == 0) { // Child process
        dup2(fd, STDIN_FILENO); // Redirect standard input to the file
        close(fd); // close the file descriptor
        close(pipefd[0]); // close the read 
        dup2(pipefd[1], STDOUT_FILENO); // Redirect standard output to the pipe
        close(pipefd[1]); // close the write 

        char **args = split_command(command); // try to split command
        if (!args || !args[0]) {
            free_args(args);
            exit(1);
        }

        execvp(args[0], args); // execute command
        // writes formatted output
        // fprintf(stderr, "Command not found: %s\n", args[0]);
        exit(1);
    }

    close(fd);
    close(pipefd[1]);

    char *result = NULL;
    size_t total_bytes = 0;
    char buffer[1024];
    ssize_t bytes_read;

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
        memcpy(result + total_bytes, buffer, bytes_read + 1);
        total_bytes += bytes_read;
    }

    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0); // wait for finish child process

    if (bytes_read < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(result);
        return strdup("Command execution failed");
    }

    return result ? result : strdup("");
}

char *process_redirection_output(char *command, char *output_file) {
    output_file = trim(output_file); // trim command

    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { return strdup("Failed to open output file");}

    pid_t pid = fork(); // Fork a child process
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO); // Redirect standard output to the file
        close(fd);

        char **args = split_command(command); // try to split command
        if (!args || !args[0]) {
            free_args(args);
            exit(1);
        }
        execvp(args[0], args);// Execute command
        // fprintf(stderr, "Command not found: %s\n", args[0]);
        exit(1);
    } else if (pid < 0) {
        close(fd);
        return strdup("Failed to fork process");
    } else {
        close(fd);
        int status;
        waitpid(pid, &status, 0); // wait for finish child process

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { // check if command executed successfully
            return strdup("");
        } else {
            return strdup("Command execution failed");
        }
    }
}

char *process_semicolon(char *command) {
    char *command_copy = strdup(command);
    if (!command_copy) return strdup("Memory allocation error");
    
    char *save_position = NULL;
    char *current_command = strtok_r(command_copy, ";", &save_position);

    char *result = NULL;
    size_t result_len = 0;
    while (current_command) {
        if (*trim(current_command)) {
            char *output = shell_process_input(trim(current_command));
            
            if (output && *output) {
                if (!result) {
                    result = strdup(output);
                    result_len = strlen(output);
                }
                else {
                    char *new_result = realloc(result, result_len + strlen(output) + 2);
                    if (!new_result) {
                        free(output);
                        free(result);
                        free(command_copy);
                        return strdup("Memory allocation error");
                    }
                    result = new_result;
                    strcat(result, "\n");
                    strcat(result, output);
                    result_len = strlen(result);
                }
            }
            free(output);
        }
        current_command = strtok_r(NULL, ";", &save_position);
    }
    
    free(command_copy);
    return result ? result : strdup("");
}

char *process_pipe(char *command) {
    char *pipe_position = strchr(command, '|'); // find pipe position
    *pipe_position = '\0'; // split command

    char *first_command = trim(command); 
    char *second_command = trim(pipe_position + 1);
    if (!*first_command || !*second_command) { return strdup("Invalid pipe command"); }// check if commands are valid
    
    int pipefd[2]; // Pipe file descriptors, 0 for read, 1 for write
    if (pipe(pipefd) == -1) { return strdup("Failed to create pipe"); }
    
    pid_t pid = fork(); // fork a child process
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("Failed to fork process");
    }
    
    if (pid == 0) {
        // Child process do first command and write to pipe the second
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        char **args = split_command(first_command); // try to split command to args
        if (!args || !args[0]) { // handle error
            free_args(args);
            exit(1);
        }
        execvp(args[0], args);
        // fprintf(stderr, "Command not found: %s\n", args[0]);
        exit(1);
    }
    
    close(pipefd[1]); // close write for pernt process
    int stdin_backup = dup(STDIN_FILENO); // save stdin 

    dup2(pipefd[0], STDIN_FILENO); // redirect stdin to read from pipe
    close(pipefd[0]); // close read 
    
    char *result = launch_process(second_command); // execute second command
    
    dup2(stdin_backup, STDIN_FILENO); // restore stdin
    close(stdin_backup); // close backup stdin
    
    int status;
    waitpid(pid, &status, 0); // wait for finishing child process
    
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { // check result
        free(result);
        return strdup("Pipe execution failed");
    }
    return result;
}

char *shell_process_input(char *command) {
    if (!command || !*command) return strdup("");
    if (check_internal_command(trim(command))) { return strdup(shell_process_command(command));}

    // check if command contains comment
    char *comment_position = strchr(command, '#');
    if (comment_position) *comment_position = '\0'; // remove comment part

    // check if command contains ';'
    char *semicolon = strchr(command, ';');
    if (semicolon) {
        return process_semicolon(command);
    }

    // Pipe handling
    char *pipe_position = strchr(command, '|'); // find pipe position
    if (pipe_position) { // if pipe found
        return process_pipe(command);
    }

    // Redirections
    char *redirection_pos = NULL;
    
    if ((redirection_pos = strchr(command, '>'))) {
        *redirection_pos = '\0'; // split command
        char *command_part = trim(command); // trim command
        char *file_part = trim(redirection_pos + 1);
        
        return process_redirection_output(command_part, file_part);
    }
    
    if ((redirection_pos = strchr(command, '<'))) {
        *redirection_pos = '\0'; // split command
        char *command_part = trim(command); // trim command
        char *file_part = trim(redirection_pos + 1);
        
        return process_redirection_input(command_part, file_part);
    }
    
    return shell_process_command(command);
}