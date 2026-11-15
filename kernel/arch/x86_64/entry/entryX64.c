#include "entryX64.h"
#include "../../../types.h"
#include "../../../printf.h"
#include "../../../panic.h"
#include "syscall.h"

#define USER_CODE_ADDR 0x400000
#define USER_STACK_TOP 0x800000 

//static const char test_msg[] = "MaXOS ring3: SYSCALL works!\n";

#define PUTC_BLOB_SIZE 12
static size_t emit_putc(uint8_t *p, char c) {
    p[0] = 0xB8; p[1] = SYS_PUTC; p[2] = 0; p[3] = 0; p[4] = 0; /* mov eax, imm32 */
    p[5] = 0xBF; p[6] = (uint8_t)c; p[7] = 0; p[8] = 0; p[9] = 0; /* mov edi, imm32 */
    p[10] = 0x0F; p[11] = 0x05; /* syscall */
    return PUTC_BLOB_SIZE;
}

/* mov eax, 60 / mov edi, <code> / syscall - SYS_EXIT */
#define EXIT_BLOB_SIZE 12
static size_t emit_exit(uint8_t *p, int code) {
    p[0] = 0xB8; p[1] = SYS_EXIT; p[2] = 0; p[3] = 0; p[4] = 0;
    p[5] = 0xBF; p[6] = (uint8_t)code; p[7] = 0; p[8] = 0; p[9] = 0;
    p[10] = 0x0F; p[11] = 0x05;
    return EXIT_BLOB_SIZE;
}

void enter_usermode(void) {
    //kprintf("[usermode] Preparing user-space...\n");

    uint8_t *dest = (uint8_t *)(uintptr_t)USER_CODE_ADDR;
    size_t off = 0;

   // for (const char *s = test_msg; *s; s++)
        //off += emit_putc(dest + off, *s);
    off += emit_exit(dest + off, 42);
    dest[off++] = 0xEB;
    dest[off++] = 0xFE;

    kprintf("Code at %lx (%lu bytes), stack top at %lx\n",
            (unsigned long)USER_CODE_ADDR, (unsigned long)off,
            (unsigned long)USER_STACK_TOP);
    //kprintf("[usermode] Jumping to Ring 3...\n");

    int code = (int)(int64_t)enter_usermode_asm(USER_CODE_ADDR, USER_STACK_TOP);
    //kprintf("[usermode] Back in kernel, exit code = %d\n", code);
}
