#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>

#define CONSOLE_LINE_CAPACITY 80U

size_t console_readline(char *buffer, size_t capacity);

#endif
