#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "utils.h"

// https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trim(char *str) {
    char *end_of_str;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    // Trim trailing space
    end_of_str = str + strlen(str) - 1;
    while (end_of_str > str && isspace((unsigned char)*end_of_str)) end_of_str--;

    // Add null terminator at the end of the string
    end_of_str[1] = '\0';
    return str;
}

char *get_current_time() {
    // Get current time
    // https://stackoverflow.com/questions/5141960/get-the-current-time-in-c
    time_t current_time = time(NULL); // Return the current time and put it in *TIMER if TIMER is not NULL.
    struct tm *local_time = localtime(&current_time); // Return the `struct tm' representation of *TIMER in the local timezone.
    if (!local_time) perror("localtime"); // Check for errors

    // Formtat time as HH:MM
    char time_str[6];
    if (strftime(time_str, sizeof(time_str), "%H:%M", local_time) == 0) {
        fprintf(stderr, "strftime failed\n");
        strncpy(time_str, "00:00", sizeof(time_str) - 1);
        time_str[sizeof(time_str) - 1] = '\0';
    }

    return strdup(time_str);
}

char *get_current_date_with_time() {
    time_t current_time = time(NULL); // Return the current time and put it in *TIMER if TIMER is not NULL.
    struct tm *local_time = localtime(&current_time); // Return the `struct tm' representation of *TIMER in the local timezone.
    if (!local_time) perror("localtime"); // Check for errors

    // Format date as YYYY-MM-DD HH:MM
    char date_with_time_str[20]; // Format TP into S according to FORMAT.
    if (strftime(date_with_time_str, sizeof(date_with_time_str), "%Y-%m-%d %H:%M", local_time) == 0) {
        fprintf(stderr, "strftime failed\n");
        strncpy(date_with_time_str, "0000-00-00 00:00", sizeof(date_with_time_str) - 1);
        date_with_time_str[sizeof(date_with_time_str) - 1] = '\0';
    }

    return strdup(date_with_time_str);
}