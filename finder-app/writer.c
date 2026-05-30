#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid arguments");
        return 1;
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    openlog(NULL, 0, LOG_USER);

    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    fprintf(file, "%s", writestr);

    fclose(file);
    closelog();

    return 0;
}
