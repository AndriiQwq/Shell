#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>

int main() {
    // Zistenie mena používateľa
    uid_t uid = getuid(); // Získanie UID aktuálneho používateľa
    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        perror("getpwuid");
        return 1;
    }
    char *username = pw->pw_name;

    // Zistenie názvu stroja
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return 1;
    }

    // Zistenie aktuálneho času
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    if (!local_time) {
        perror("localtime");
        return 1;
    }
    char time_string[6]; // Formát HH:MM
    if (strftime(time_string, sizeof(time_string), "%H:%M", local_time) == 0) {
        fprintf(stderr, "Failed to format time\n");
        return 1;
    }

    // Vytvorenie promptu
    char prompt[512];
    snprintf(prompt, sizeof(prompt), "%s %s@%s#", time_string, username, hostname);

    // Výstup promptu
    printf("%s\n", prompt);

    return 0;
}
