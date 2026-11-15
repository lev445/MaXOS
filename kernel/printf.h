#ifndef MAXOS_PRINTF_H
#define MAXOS_PRINTF_H

#include <stdarg.h>
#include <stddef.h>

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int kprintf(const char *fmt, ...);

#endif
