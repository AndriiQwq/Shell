#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *config_path = NULL;

// To load the .env file and export the variables
void load_env_file(const char *path) {
    FILE *env_file = fopen(path, "r"); // open file with .env path to read
    if (env_file == NULL) return;

    char line[128];
    while (fgets(line, sizeof(line), env_file)) { // read line 
        line[strcspn(line, "\n")] = '\0'; // delete new line symbol
        if (strlen(line) == 0 || line[0] == '#') continue; // skip empty lines and comments
        char *name = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (name && value) setenv(name, value, 1); // Flag 1, to overwrite the variable if it exists
    }

    fclose(env_file); // close file
}