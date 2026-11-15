#ifndef MAXOS_PIC_H
#define MAXOS_PIC_H

#include <stdint.h>

void pic_init(void);              /* ремап 0x20/0x28, все IRQ замаскированы */
void pic_set_mask(uint8_t irq, int enable);
void pic_eoi(uint8_t irq);

#endif
