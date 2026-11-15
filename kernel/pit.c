#include "pit.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/lib/pic.h"
#include "arch/x86_64/lib/io.h"

static volatile uint64_t ticks;

static void pit_irq(void) {
    ticks++;
}

void pit_init(void) {
    uint32_t divisor = 1193182 / PIT_HZ;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    irq_register(0, pit_irq);
    pic_set_mask(0, 1);
}

uint64_t pit_ticks(void) { return ticks; }
uint64_t uptime_ms(void) { return ticks * (1000 / PIT_HZ); }
