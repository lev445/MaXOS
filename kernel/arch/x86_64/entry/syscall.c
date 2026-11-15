#include "syscall.h"
#include "../../../printf.h"
#include "../../../console.h"
#include "../../../pit.h"
#include "../../../ps2.h"
#include "../../../panic.h"
#include "../lib/io.h"

#define USER_MEM_LO  0xA00000ULL
#define USER_MEM_HI  0xE00000ULL   /* exclusive */

static int user_range_ok(uint64_t addr, uint64_t len) {
    if (len == 0)
        return 1;
    if (addr < USER_MEM_LO)
        return 0;
    if (addr + len < addr) /* overflow uint64 */
        return 0;
    return (addr + len) <= USER_MEM_HI;
}

uint64_t syscall_dispatch(syscall_regs_t *r) {
    switch (r->rax) {

    case SYS_PUTC: /* putc(char c) */
        console_putc((char)r->rdi);
        return 0;

    case SYS_WRITE: { /* write(const char *buf, uint64_t n) */
        uint64_t addr = r->rdi;
        uint64_t len  = r->rsi;
        //kprintf("KernelDebug[write](addr=0x%lx, len=%lu)\n", addr, len);
        if (!user_range_ok(addr, len)) {
            kprintf("KernelError[write](Addr: %lx, LenSize: %lu, Syscall: %lu): ptr outside user memory\n",
                    (unsigned long)addr, (unsigned long)len, (unsigned long)r->rax);
            return SYSCALL_ERR;
        }
        if (len > (1u << 20)) { /* limit */
            kprintf("KernelError[write]: (Addr: %lx, LenSize: %lu, Syscall: %lu) size is too big!\n", (unsigned long)addr, (unsigned long)len, (unsigned long)r->rax);
            return SYSCALL_ERR;
        }
        const char *buf = (const char *)(uintptr_t)addr;

        for (uint64_t i = 0; i < len; i++)
            console_putc(buf[i]);
        return len;
    }

    case SYS_AI:  /* :) */
        kprintf("Sizran'\n");
        return 0;

    case SYS_UPTIME: /* uptime_ms() */
        return uptime_ms();

    case SYS_GETCHAR: /* getchar(), -1 if empty */
        return kbd_avail() ? (uint64_t)(int64_t)kbd_getchar() : SYSCALL_ERR;

    case SYS_EXIT: { /* exit(int code) */
        int code = (int)(int64_t)r->rdi;
        //kprintf("exit(%d)\n", code);
        syscall_return_to_kernel((uint64_t)(int64_t)code);
        // but if syscall_return_to_kernel get error, using hlt for security
        while(1) {
        	hlt();	
        }
    }

    default:
		kprintf("KernelError[Syscall]: syscall %lu not found", (unsigned long)r->rax);
        return SYSCALL_ERR;
    }
}
