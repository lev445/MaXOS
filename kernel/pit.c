#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static volatile uint64_t ticks;

static void pit_irq(void)
{
    ticks++;
}

void pit_init(void)
{
    /* канал 0, режим 2 (rate generator), ~100 Hz */
    uint32_t divisor = 1193182 / PIT_HZ;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    irq_register(0, pit_irq);
    pic_set_mask(0, 1);
}

uint64_t pit_ticks(void) { return ticks; }
uint64_t uptime_ms(void) { return ticks * (1000 / PIT_HZ); }
