#ifndef PANIC_H
#define PANIC_H

#include "idt.h"

void panic(const char *message);
void panic_regs(const char *message, registers_t *r);

#endif
