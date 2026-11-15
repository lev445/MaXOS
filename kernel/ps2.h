#ifndef MAXOS_PS2_H
#define MAXOS_PS2_H

#include <stdint.h>

/* Специальные коды, возвращаемые kbd_getchar() (>= 0x80 - не ASCII) */
#define KEY_UP    0x80
#define KEY_DOWN  0x81
#define KEY_LEFT  0x82
#define KEY_RIGHT 0x83
#define KEY_HOME  0x84
#define KEY_END   0x85
#define KEY_DEL   0x86
#define KEY_PGUP  0x87
#define KEY_PGDN  0x88
#define KEY_INS   0x89
#define KEY_F1    0x8A
#define KEY_F2    0x8B
#define KEY_F3    0x8C

void ps2_init(void);

int  kbd_getchar(void);     /* блокирующее чтение символа/клавиши */
int  kbd_avail(void);       /* есть ли что-то в буфере */

void mouse_enable(int on);
int  mouse_enabled(void);
void mouse_get(int *x, int *y, int *buttons);

#endif
