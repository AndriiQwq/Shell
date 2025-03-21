#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>

#include "shell.h"
#include <asm-generic/fcntl.h>

char isServer = 0;
char isClient = 0;
int port = 0;
char *socketName = NULL;
char *ip = "127.0.0.1";

char *get_prompt() {
    static char prompt[512];

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        perror("getpwuid");
        return NULL;
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return NULL;
    }

    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if (!local_time) {
        perror("localtime");
        return NULL;
    }

    char time_string[6];
    if (strftime(time_string, sizeof(time_string), "%H:%M", local_time) == 0) {
        fprintf(stderr, "strftime failed\n");
        return NULL;
    }

    if (snprintf(prompt, sizeof(prompt), "%s %s@%s$", time_string, pw->pw_name, hostname) >= (int)sizeof(prompt)) {
        fprintf(stderr, "Prompt string was truncated\n");
        return NULL;
    }

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
    }
    // else if (strncmp(command, "send ", 5) == 0) {
    //     const char *file_path = command + 5;
    //     return cmd_send(NULL, file_path);
    // } 
    else {
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
           "send <file_path> - send a file to the client\n";
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
    if (pw)
        return pw->pw_name;
    else
        return "Error to get username";
}

char *cmd_hostname() {
    static char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return hostname;
    else
        return "Error to get hostname";
}

char *cmd_pwd() {
    static char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        return cwd;
    else
        return "Error to get current directory";
}

char *cmd_cd(const char *path) {
    if (chdir(path) == 0) {
        return "Current directory changed";
    } else {
        return "Failed to change directory";
    }
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
    if (file == NULL) {
        return "Failed to open file";
    }

    size_t n;
    while ((n = fread(cat_buffer + strlen(cat_buffer), 1, sizeof(cat_buffer) - strlen(cat_buffer) - 1, file)) > 0) {
        if (strlen(cat_buffer) >= sizeof(cat_buffer) - 1) {
            break;
        }
    }

    fclose(file);
    return cat_buffer;
}

// char *cmd_send(int client_sock, const char *file_path) {
//     send_file(client_sock, file_path);
//     return "File sent";
// }




// char *shell_process_input(const char *command) {
//     char *result = NULL;

//     for (int i = 0; i < strlen(command); i++) {
//         if (command[i] != '#' && command[i] != ';' && command[i] != '<' && command[i] != '>' && command[i] != '|' && command[i] != '\\') {
//             continue;
//         } else {
//             process_special_characters(command);
//         }
//     }

//     if (result == NULL) {
//         result = shell_process_command(command);
//     }

//     return result;
// }


// Функция для удаления ведущих и завершающих пробелов
char *trim_whitespace(char *str) {
    char *end;

    // Удаление ведущих пробелов
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // Все пробелы
        return str;

    // Удаление завершающих пробелов
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Позиционирование завершающего нуля
    end[1] = '\0';

    return str;
}

// Функция для обработки специальных символов
char *shell_process_input(const char *command) {
    char *result = NULL;
    char *temp_command = strdup(command);
    char *token;

    // Обработка комментариев (#)
    if ((token = strchr(temp_command, '#')) != NULL) {
        *token = '\0';
    }

    // Обработка нескольких команд (;)
    char *commands = strtok(temp_command, ";");
    while (commands != NULL) {
        // Удаление ведущих и завершающих пробелов из команды
        char *trimmed_command = trim_whitespace(commands);

        char *sub_result = shell_process_command(trimmed_command);
        if (result == NULL) {
            result = strdup(sub_result);
        } else {
            result = realloc(result, strlen(result) + strlen(sub_result) + 2);
            strcat(result, "\n");
            strcat(result, sub_result);
        }
        commands = strtok(NULL, ";");
    }

    // Обработка перенаправления ввода/вывода (<, >)
    if (strchr(temp_command, '<') || strchr(temp_command, '>')) {
        // Пример: ls > output.txt
        // Пример: cat < input.txt
        char *input_file = NULL;
        char *output_file = NULL;
        char *base_command = strdup(temp_command);
        char *redir_token;

        // Поиск перенаправления ввода (<)
        if ((redir_token = strchr(base_command, '<')) != NULL) {
            *redir_token = '\0';
            input_file = strtok(redir_token + 1, " ");
        }

        // Поиск перенаправления вывода (>)
        if ((redir_token = strchr(base_command, '>')) != NULL) {
            *redir_token = '\0';
            output_file = strtok(redir_token + 1, " ");
        }

        // Выполнение команды с перенаправлением ввода/вывода
        char *args[64];
        int i = 0;
        char *arg = strtok(base_command, " ");
        while (arg != NULL) {
            args[i++] = arg;
            arg = strtok(NULL, " ");
        }
        args[i] = NULL;

        execute_command(args, input_file, output_file);
    }

    // Обработка пайпов (|) и других специальных символов может быть добавлена по аналогии

    free(temp_command);
    return result;
}

void execute_command(char **args, char *input_file, char *output_file) {
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
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
        }

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Ошибка при fork
        perror("fork");
    } else {
        // Родительский процесс
        int status;
        waitpid(pid, &status, 0);
    }
}