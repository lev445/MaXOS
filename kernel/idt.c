/* 2/11/26 MaXOS: IDT (Interrupt Descriptor Table)
 * job: ALL interrupts of CPU has been catched here.
 * info: In isr.asm you can view stubs!
*/

#include "idt.h"
#include "pic.h"
#include "printf.h"
#include "console.h"
#include "io.h"
#include "string.h"
#include "panic.h"

typedef struct {
    uint16_t off_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t off_mid;
    uint32_t off_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[256] __attribute__((aligned(16)));
static struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr;

static irq_handler_t irq_handlers[16];

static const char *fault_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range",
    "Invalid opcode", "Device not available", "Double fault", "Coproc seg over",
    "Invalid TSS", "Segment not present", "Stack-segment fault", "GPF",
    "Page fault", "Reserved", "x87 FPU error", "Alignment check", "Machine check",
    "SIMD FP exception", "Virtualization", "Control protection", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "VMM communication", "Security exception", "Reserved"
};

void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t sel, uint8_t type_attr)
{
    idt_entry_t *e = &idt[vector];
    e->off_low   = handler & 0xFFFF;
    e->selector  = sel;
    e->ist       = 0;
    e->type_attr = type_attr;
    e->off_mid   = (handler >> 16) & 0xFFFF;
    e->off_high  = (handler >> 32) & 0xFFFFFFFF;
    e->reserved  = 0;
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 256; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x18 /* code64 */, 0x8E);
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)(uintptr_t)idt;
    __asm__ volatile("lidt %0" :: "m"(idtr));
}

void irq_register(uint8_t irq, irq_handler_t h)
{
    if (irq < 16)
        irq_handlers[irq] = h;
}

static uint64_t read_cr2(void)
{
    uint64_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

void isr_common_handler(registers_t *r)
{
   if (r->int_no < 32) {
        char message[128];
        snprintf(message, sizeof(message),
                "CPU Exception #%lu: %s",
                r->int_no,
                fault_names[r->int_no] ? fault_names[r->int_no] : "Unknown");
        
        panic_regs(message, r);
    }
    uint8_t irq = (uint8_t)(r->int_no - 32);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq]();
    pic_eoi(irq);
}
