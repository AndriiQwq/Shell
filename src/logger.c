#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"
#include "utils.h"

char *log_file_path = NULL; 

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
// https://github.com/aide/aide/blob/002712ac1307b4213bd8dbf5ad77b03b8b05c3e5/src/log.c#L135

// Create a log file and write in it the date of creation 
void create_log_file(const char *log_file_path) { 
    FILE *file = fopen(log_file_path, "w"); // open file to write
    if (!file) {
        perror("Error to open file");
        return;
    }

    char *current_time = get_current_date_with_time(); // get current date and time
    fprintf(file, "[%s] SYSTEM: Creation of log file.\n", current_time); // write in file
    free(current_time);  // free memory

    fclose(file); // close file
}

// Or use flock to lock the file, flock use blocking system calls, system block
void write_log(const char *source, const char *log_message) {
    if (!log_file_path) return;

    pthread_mutex_lock(&log_mutex); // lock mutex

    FILE *file = fopen(log_file_path, "a"); // open file to append
    if (!file) { // if error occurs then print error
        perror("Error to open log file");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    char *current_time = get_current_date_with_time(); // get current date and time
    if (source) { // if source is defined then write it in the log file
        fprintf(file, "[%s] %s: %s\n", current_time, source, log_message);
    } else { // else write only the log message
        fprintf(file, "[%s] %s\n", current_time, log_message);
    }
    free(current_time);

    fclose(file);
    pthread_mutex_unlock(&log_mutex); // unlock mutex
}

int log_file_exists(const char *log_file_path) { // check if log file exists
    if (!log_file_path) return 0;
    
    FILE *file = fopen(log_file_path, "r"); // open file to read
    if (file) {
        fclose(file);
        return 1; // File exists
    }

    return 0; // File does not exist
}
