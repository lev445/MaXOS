#include "printf.h"
#include "console.h"

typedef struct {
    char  *buf;
    size_t size;
    size_t len;
} out_t;

static void emit(out_t *o, char c)
{
    if (o->buf && o->len + 1 < o->size)
        o->buf[o->len] = c;
    o->len++;
}

static void emit_str(out_t *o, const char *s, int width, int zero_pad, int left)
{
    int slen = 0;
    while (s[slen])
        slen++;
    int pad = width > slen ? width - slen : 0;
    if (!left)
        for (int i = 0; i < pad; i++)
            emit(o, zero_pad ? '0' : ' ');
    while (*s)
        emit(o, *s++);
    if (left)
        for (int i = 0; i < pad; i++)
            emit(o, ' ');
}

static void emit_num(out_t *o, unsigned long long v, int base, int upper,
                     int width, int zero_pad, int left, int neg)
{
    char tmp[24];
    int i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0)
        tmp[i++] = '0';
    while (v) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    if (neg)
        tmp[i++] = '-';
    /* reverse into buf */
    char rev[26];
    for (int k = 0; k < i; k++)
        rev[k] = tmp[i - 1 - k];
    rev[i] = 0;
    /* zero padding applies between sign and digits; simplify: pad whole */
    int pad = width > i ? width - i : 0;
    if (!left && zero_pad) {
        /* put padding zeros right after optional sign */
        int start = (rev[0] == '-') ? 1 : 0;
        emit(o, rev[0] == '-' ? '-' : rev[0] == '+' ? '+' : rev[start]);
        if (start == 0) {
            for (int k = 0; k < pad; k++)
                emit(o, '0');
            for (int k = 1; k < i; k++)
                emit(o, rev[k]);
        } else {
            for (int k = 0; k < pad; k++)
                emit(o, '0');
            for (int k = 1; k < i; k++)
                emit(o, rev[k]);
        }
        return;
    }
    emit_str(o, rev, width, 0, left);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    out_t o = { buf, size, 0 };
    while (*fmt) {
        if (*fmt != '%') {
            emit(&o, *fmt++);
            continue;
        }
        fmt++;
        int left = 0, zero_pad = 0, width = 0;
        if (*fmt == '-') { left = 1; fmt++; }
        if (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        int longmod = 0;
        while (*fmt == 'l') { longmod++; fmt++; }
        if (*fmt == 'z') { longmod = 2; fmt++; }

        switch (*fmt) {
        case 'c': {
            char c = (char)va_arg(ap, int);
            char s[2] = { c, 0 };
            emit_str(&o, s, width, 0, left);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            emit_str(&o, s, width, 0, left);
            break;
        }
        case 'd':
        case 'i': {
            long long v = (longmod >= 1) ? va_arg(ap, long long) : va_arg(ap, int);
            int neg = v < 0;
            unsigned long long uv = neg ? (unsigned long long)(-v) : (unsigned long long)v;
            emit_num(&o, uv, 10, 0, width, zero_pad, left, neg);
            break;
        }
        case 'u': {
            unsigned long long v = (longmod >= 1) ? va_arg(ap, unsigned long long)
                                                  : va_arg(ap, unsigned int);
            emit_num(&o, v, 10, 0, width, zero_pad, left, 0);
            break;
        }
        case 'x':
        case 'X': {
            unsigned long long v = (longmod >= 1) ? va_arg(ap, unsigned long long)
                                                  : va_arg(ap, unsigned int);
            emit_num(&o, v, 16, (*fmt == 'X'), width, zero_pad, left, 0);
            break;
        }
        case 'p': {
            unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void *);
            emit(&o, '0');
            emit(&o, 'x');
            emit_num(&o, v, 16, 0, 16, 1, 0, 0);
            break;
        }
        case '%':
            emit(&o, '%');
            break;
        default:
            emit(&o, '%');
            emit(&o, *fmt);
            break;
        }
        fmt++;
    }
    if (buf && size)
        buf[o.len < size ? o.len : size - 1] = 0;
    return (int)o.len;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int kprintf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    console_puts(buf);
    return r;
}
