#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

char *process_script(const char *path){
    FILE *file = fopen(path, "r");
    if (!file) return "Error during opening file";

    char line[128]; // 128 for file size 
    char *process_script_output = strdup("");
    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; //delete new line symbol
        if (strlen(line) == 0) continue; // skip empty lines

        char *command_output = shell_process_input(line);
        if (!command_output) continue;

        // Realoc memoery for new output of script command
        size_t new_size = strlen(process_script_output) + strlen(command_output) + 2; // for new line and /0
        char *new_output = realloc(process_script_output, new_size);
        if (!new_output) {
            free(process_script_output);
            fclose(file);
            return "Memory allocation error";
        } else {
            process_script_output = new_output;
        }
        
        strcat(process_script_output, command_output);
        // strcat(process_script_output, "\n");
        // sprintf(process_script_output + strlen(process_script_output), "%s\n", command_output);

        free(command_output);
    }

    fclose(file);
    return process_script_output;
}