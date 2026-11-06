/* check information in idt.c */
#ifndef MAXOS_IDT_H
#define MAXOS_IDT_H

#include <stdint.h>

/* push'es from isr.asm */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

typedef void (*irq_handler_t)(void);

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t sel, uint8_t type_attr);
void irq_register(uint8_t irq, irq_handler_t h);

/* from isr.asm */
extern uint64_t isr_stub_table[256];
void isr_common_handler(registers_t *r);

#endif
