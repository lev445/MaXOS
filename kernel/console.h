#ifndef MAXOS_CONSOLE_H
#define MAXOS_CONSOLE_H

#include <stdint.h>
#include "types.h"

enum vga_color {
    COL_BLACK = 0, COL_BLUE, COL_GREEN, COL_CYAN,
    COL_RED, COL_MAGENTA, COL_BROWN, COL_LGRAY,
    COL_DGRAY, COL_LBLUE, COL_LGREEN, COL_LCYAN,
    COL_LRED, COL_LMAGENTA, COL_YELLOW, COL_WHITE
};

void console_init(const boot_info_t *bi);
int  console_ready(void);

void console_clear(void);
void console_putc(char c);
void console_puts(const char *s);
void console_write(const char *s, int n);
void console_scroll(int lines);
void console_refresh(void);

void console_set_color(uint8_t fg, uint8_t bg);
void console_get_color(uint8_t *fg, uint8_t *bg);

int  console_cols(void);
int  console_rows(void);
void console_set_cursor(int x, int y);
void console_get_cursor(int *x, int *y);
void console_cursor_visible(int vis);

#endif
