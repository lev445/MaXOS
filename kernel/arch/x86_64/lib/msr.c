/* msr.c - set SYSCALL/SYSRET for MaXOS (x86_64)*/
#include "msr.h"
#include "../../../heap.h"
#include "../../../panic.h"
#include "../../../printf.h"
#include "../entry/syscall.h"
#include "io.h"

#define SYSCALL_STACK_SIZE  (16 * 1024)

extern void syscall_handler(void);

/* Верхушка стека, на который переключается syscall_handler.
 * BUGFIX: раньше стек брался через kmalloc(16 KiB), но kmalloc() выдаёт
 * максимум одну страницу (4096 - sizeof(heap_block_t) = 4072 байт) и НЕ
 * проверяет, что size влезает. Стек "на 16 KiB" реально жил в 4 KiB, а
 * его верхушка торчала на 12 KiB вперёд — в страницы, которые PMM считал
 * свободными и потом раздавал другим. Теперь это честный статический
 * массив в .bss: размер гарантирован, страницы ни с кем не пересекаются. */
static uint8_t syscall_stack[SYSCALL_STACK_SIZE] __attribute__((aligned(16)));
uint64_t syscall_stack_top = 0;

/* sel from (boot/stage2.asm)
 * 0x00 null | 0x08 code32 | 0x10 data32 | 0x18 code64 | 0x20 data64
 * 0x28 user_data64 (DPL3) | 0x30 user_code64 (DPL3) | 0x38 TSS
 */
#define SEG_KERNEL_CS   0x18    /* SYSCALL -> CS = 0x18, SS = 0x18+8 = 0x20 */
#define SEG_USER_BASE   0x23    /* SYSRET  -> CS = 0x23+16 = 0x33 | RPL3 = 0x33
                                           SS = 0x23+8  = 0x2B | RPL3 = 0x2B */

/* RFLAGS, which we extinguish when entering the poison
 * TF(8) - single step, IF(9) - irq, DF(10) - direction, AC(18) - align check */
#define SFMASK_VALUE    (EFLAGS_TF | EFLAGS_IF | EFLAGS_DF | EFLAGS_AC)

static int cpu_has_syscall(void) {
    /* CPUID.80000001H:EDX[11] = SYSCALL/SYSRT present*/
    uint32_t eax, ebx, ecx, edx;
    uint32_t leaf = 0x80000001u;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(leaf));
    return (edx & (1u << 11)) != 0;
}

void syscall_init(void) {
    if (!cpu_has_syscall()) {
        panic("KernelError[syscall_init]: CPU does not support SYSCALL/SYSRET (CPUID.80000001:EDX[11]=0)");
    }
    if (syscall_stack_top == 0)
        syscall_stack_top =
            ((uint64_t)(uintptr_t)syscall_stack + SYSCALL_STACK_SIZE) & ~0xFULL;
    uint64_t efer = rdmsr(IA32_EFER);
    wrmsr(IA32_EFER, efer | EFER_SCE);
    uint64_t star = ((uint64_t)SEG_USER_BASE << 48) | ((uint64_t)SEG_KERNEL_CS << 32);
    wrmsr(IA32_STAR, star);

    uint64_t lstar = (uint64_t)(uintptr_t)syscall_handler;
    wrmsr(IA32_LSTAR, lstar);
    wrmsr(IA32_CSTAR, lstar);
    wrmsr(IA32_FMASK, SFMASK_VALUE);
}
