#include "console.h"

#include "uart.h"

#define HISTORY_DEPTH 8U

#define KEY_NONE 0
#define KEY_UP 1
#define KEY_DOWN 2

enum escape_state {
    ESCAPE_NORMAL,
    ESCAPE_SEEN,
    ESCAPE_CSI,
    ESCAPE_SS3,
};

static char history[HISTORY_DEPTH][CONSOLE_LINE_CAPACITY];
static size_t history_count;
static size_t history_next;

static int is_printable(char c) {
    return c >= ' ' && c <= '~';
}

static int strings_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

static int is_blank_line(const char *line) {
    while (*line != '\0') {
        if (*line != ' ') {
            return 0;
        }
        ++line;
    }
    return 1;
}

static size_t copy_string(char *destination, size_t capacity,
                          const char *source) {
    size_t length = 0U;

    if (capacity == 0U) {
        return 0U;
    }

    while (source[length] != '\0' && length + 1U < capacity) {
        destination[length] = source[length];
        ++length;
    }
    destination[length] = '\0';
    return length;
}

static void save_line(char *destination, size_t capacity,
                      const char *source, size_t length) {
    size_t copied = 0U;

    if (capacity == 0U) {
        return;
    }

    while (copied < length && copied + 1U < capacity) {
        destination[copied] = source[copied];
        ++copied;
    }
    destination[copied] = '\0';
}

static const char *history_entry(size_t offset_from_newest) {
    size_t index =
        (history_next + HISTORY_DEPTH - offset_from_newest) % HISTORY_DEPTH;
    return history[index];
}

static void history_add(const char *line) {
    if (is_blank_line(line)) {
        return;
    }

    if (history_count != 0U && strings_equal(line, history_entry(1U))) {
        return;
    }

    (void)copy_string(history[history_next], CONSOLE_LINE_CAPACITY, line);
    history_next = (history_next + 1U) % HISTORY_DEPTH;
    if (history_count < HISTORY_DEPTH) {
        ++history_count;
    }
}

static void replace_line(char *buffer, size_t capacity, size_t *length,
                         const char *replacement) {
    while (*length != 0U) {
        uart_puts("\b \b");
        --*length;
    }

    *length = copy_string(buffer, capacity, replacement);
    for (size_t index = 0U; index < *length; ++index) {
        uart_putc(buffer[index]);
    }
}

size_t console_readline(char *buffer, size_t capacity) {
    char draft[CONSOLE_LINE_CAPACITY];
    size_t length = 0U;
    size_t history_offset = 0U;
    enum escape_state escape = ESCAPE_NORMAL;

    if (capacity == 0U) {
        return 0U;
    }
    if (capacity > CONSOLE_LINE_CAPACITY) {
        capacity = CONSOLE_LINE_CAPACITY;
    }

    buffer[0] = '\0';
    draft[0] = '\0';

    for (;;) {
        char c = uart_getc();
        int key = KEY_NONE;

        if (escape == ESCAPE_SEEN) {
            escape = ESCAPE_NORMAL;
            if (c == '[') {
                escape = ESCAPE_CSI;
                continue;
            }
            if (c == 'O') {
                escape = ESCAPE_SS3;
                continue;
            }
            // 알 수 없는 ESC 다음 문자는 일반 입력으로 다시 처리한다.
        } else if (escape == ESCAPE_CSI || escape == ESCAPE_SS3) {
            escape = ESCAPE_NORMAL;
            if (c == 'A') {
                key = KEY_UP;
            } else if (c == 'B') {
                key = KEY_DOWN;
            }

            if (key == KEY_UP && history_count != 0U) {
                if (history_offset == 0U) {
                    save_line(draft, sizeof(draft), buffer, length);
                }
                if (history_offset < history_count) {
                    ++history_offset;
                    replace_line(buffer, capacity, &length,
                                 history_entry(history_offset));
                }
            } else if (key == KEY_DOWN && history_offset != 0U) {
                --history_offset;
                if (history_offset == 0U) {
                    replace_line(buffer, capacity, &length, draft);
                } else {
                    replace_line(buffer, capacity, &length,
                                 history_entry(history_offset));
                }
            }
            continue;
        }

        if (c == '\r' || c == '\n') {
            uart_puts("\n");
            break;
        }

        if (c == '\x1b') {
            escape = ESCAPE_SEEN;
            continue;
        }

        if (c == '\b' || c == 0x7f) {
            if (length > 0U) {
                --length;
                buffer[length] = '\0';
                uart_puts("\b \b");
            }
            continue;
        }

        if (is_printable(c) && length + 1U < capacity) {
            buffer[length++] = c;
            buffer[length] = '\0';
            uart_putc(c);
        }
    }

    buffer[length] = '\0';
    history_add(buffer);
    return length;
}
