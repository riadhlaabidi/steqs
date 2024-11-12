#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"

// NOTE: temporarily
static char *log_file_path = "/tmp/logs";

FILE *open_log_file(void)
{
    FILE *f = fopen(log_file_path, "a");
    if (f) {
        return f;
    }

    fprintf(stderr, "Failed to open log file %s (%s)", strerror(errno),
            log_file_path);

    return stderr;
}

static int do_log_to_file(FILE *log_file, int log_level, const char *func_name,
                          int line, const char *fmt, va_list args)
{
    static const char *log_levels[] = {
        [DEBUG] = "DEBUG", [INFO] = "INFO", [WARN] = "WARN", [ERROR] = "ERROR"};

    time_t raw_time = time(NULL);
    struct tm *local_time = localtime(&raw_time);

    if (!local_time) {
        return 0;
    }

    char time_buf[32];

    if (strftime(time_buf, sizeof(time_buf), "%D %X", local_time) == 0) {
        return 0;
    }

    int logged =
        (func_name == NULL || line == -1)
            ? fprintf(log_file, "%s %s ", time_buf, log_levels[log_level])
            : fprintf(log_file, "%s %s %s:%d ", time_buf, log_levels[log_level],
                      func_name, line);

    if (logged < 0) {
        return 0;
    }

    if (vfprintf(log_file, fmt, args) < 0) {
        return 0;
    }

    fputc('\n', log_file);

    return 1;
}

int log_msg(int log_level, const char *func_name, int line, const char *fmt,
            ...)
{
    FILE *log_file = open_log_file();
    va_list args;
    va_start(args, fmt);
    int logged =
        do_log_to_file(log_file, log_level, func_name, line, fmt, args);
    va_end(args);

    if (log_file != stdout && log_file != stderr) {
        fclose(log_file);
    }

    return logged;
}
