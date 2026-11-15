/* 2/11/26 MaXOS: Input-Output subsystem
 * Job: this functions write to ports information.
 */
#ifndef MAXOS_IO_H
#define MAXOS_IO_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t v)
{
    __asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t v)
{
    __asm__ volatile("outw %0, %1" :: "a"(v), "Nd"(port));
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

#endif
