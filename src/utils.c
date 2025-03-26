#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

// https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trim(char *str) {
    char *end_of_str;

    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    end_of_str = str + strlen(str) - 1;
    while (end_of_str > str && isspace((unsigned char)*end_of_str)) end_of_str--;

    end_of_str[1] = '\0';
    return str;
}