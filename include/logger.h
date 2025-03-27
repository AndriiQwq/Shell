#ifndef LOGGER_H
#define LOGGER_H

extern char *log_file_path;

void create_log_file(const char *log_file_path);
void write_log(const char *source, const char *log_message);
int log_file_exists(const char *log_file_path);

#endif