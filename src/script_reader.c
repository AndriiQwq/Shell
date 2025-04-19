#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

char *process_script(const char *path){
    FILE *file = fopen(path, "r"); // open file(script) for reading
    if (!file) return "Error during opening file";

    char line[128]; // 128 for file size 
    char *script_output = strdup("");
    while (fgets(line, sizeof(line), file) != NULL) { // read line from file
        line[strcspn(line, "\n")] = '\0'; //delete new line symbol
        if (strlen(line) == 0) continue; // skip empty lines

        char *command_output = shell_process_input(line); // do command
        if (!command_output) continue; // skip if command output is empty

        // Realoc memoery for new output of script command
        size_t new_size = strlen(script_output) + strlen(command_output) + 2; // for new line and /0
        char *new_output = realloc(script_output, new_size); // realloc memory
        if (!new_output) { // check if realloc was successful, if not then free memory and return error
            free(script_output);
            fclose(file);
            return "Memory allocation error";
        } else {
            script_output = new_output;
        }
        
        strcat(script_output, command_output); // add current command output to script output
        // strcat(script_output, "\n");
        // sprintf(script_output + strlen(script_output), "%s\n", command_output);

        free(command_output); // free current command output
    }

    fclose(file);
    return script_output;
}