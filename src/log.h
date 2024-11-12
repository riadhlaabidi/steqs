#pragma once

#include <stdio.h>

#define LOG(level, ...) log_msg((level), __func__, __LINE__, __VA_ARGS__)

enum LOG_LEVEL { DEBUG = 1, INFO, WARN, ERROR };

int log_msg(int log_level, const char *func_name, int line, const char *fmt,
            ...);
