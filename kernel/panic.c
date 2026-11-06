#include "types.h"
#include "io.h"
#include "printf.h"
#include "console.h"
#include "panic.h"

static void panic_puts(const char *s) {
    while (*s) outb(0xE9, *s++);
}

static void panic_print_registers(registers_t *r) {
    char buffer[512];
    int offset = 0;
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "Error code: 0x%lx\n"
                      "RIP: 0x%lx (CS: 0x%lx)\n"
                      "RSP: 0x%lx\n"
                      "RFLAGS: 0x%lx\n",
                      r->err_code, r->rip, r->cs, r->rsp, r->rflags);
    
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "RAX: 0x%lx  RBX: 0x%lx\n"
                      "RCX: 0x%lx  RDX: 0x%lx\n"
                      "RSI: 0x%lx  RDI: 0x%lx\n"
                      "RBP: 0x%lx  R8:  0x%lx\n"
                      "R9:  0x%lx  R10: 0x%lx\n"
                      "R11: 0x%lx  R12: 0x%lx\n"
                      "R13: 0x%lx  R14: 0x%lx\n"
                      "R15: 0x%lx",
                      r->rax, r->rbx, r->rcx, r->rdx,
                      r->rsi, r->rdi, r->rbp, r->r8,
                      r->r9, r->r10, r->r11, r->r12,
                      r->r13, r->r14, r->r15);
    
    if (r->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "\nCR2 (faulting address): 0x%lx", cr2);
        
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "\nPage fault details:\n"
                          "  - %s\n"
                          "  - %s\n"
                          "  - %s",
                          (r->err_code & 0x1) ? "Protection violation" : "Non-present page",
                          (r->err_code & 0x2) ? "Write operation" : "Read operation",
                          (r->err_code & 0x4) ? "User mode" : "Kernel mode");
    }
    
    buffer[offset] = '\0';
    
    panic_puts(buffer);
    
    if (console_ready()) {
        kprintf("%s", buffer);
    }
}

void panic(const char *message) {
    uint8_t fg = 0, bg = 0;
    int console_was_ready = 0;
    
    __asm__ volatile("cli");
    
    panic_puts("\n");
    panic_puts("[KERNEL PANIC]\n");
    panic_puts(message);
    panic_puts("\n");
    panic_puts("System halted.\n");
    
    if (console_ready()) {
        console_was_ready = 1;
        console_cursor_visible(0);
        console_get_color(&fg, &bg);
        console_set_color(COL_WHITE, COL_RED);
        
        kprintf("[KERNEL PANIC]\n");
        kprintf("%s\n", message);
        kprintf("System halted.\n");
    }
    
    __asm__ volatile("ud2");
    
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}

void panic_regs(const char *message, registers_t *r) {
    uint8_t fg = 0, bg = 0;
    int console_was_ready = 0;
    
    __asm__ volatile("cli");
    
    panic_puts("\n");
    panic_puts("[KERNEL PANIC]\n");
    panic_puts(message);
    panic_puts("\n\n");
    
    if (console_ready()) {
        console_was_ready = 1;
        console_cursor_visible(0);
        console_get_color(&fg, &bg);
        console_set_color(COL_WHITE, COL_RED);
        
        kprintf("[KERNEL PANIC]\n");
        kprintf("%s\n\n", message);
    }
    
    panic_print_registers(r);
    
    panic_puts("\n\nSystem halted.\n");
    
    if (console_was_ready) {
        kprintf("\n\nSystem halted.\n");
        console_set_color(fg, bg);
    }
    
    __asm__ volatile("ud2");
    
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}
