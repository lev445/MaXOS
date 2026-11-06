#include "types.h"
#include "console.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "ps2.h"
#include "shell.h"
#include "printf.h"
#include "io.h"
#include "vfs.h"
#include "initramfs.h"
#include "panic.h"
#include "heap.h"
#include "pmm.h"

extern uint8_t _initramfs_start[];
extern uint8_t _initramfs_end[];

static void debug_puts(const char *s) {
    while (*s) outb(0xE9, *s++);
}

void kmain(void) {
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)BOOTINFO_ADDR;
    uint32_t initramfs_size = (uint32_t)(_initramfs_end - _initramfs_start);
    
    if (initramfs_size >= 6) {
        //debug_puts("[kernel] CPIO magic bytes: ");
       // for (int i = 0; i < 6; i++) {
           // outb(0xE9, _initramfs_start[i]);
        //}
       // debug_puts("\n");

        if (_initramfs_start[0] == '0' && _initramfs_start[1] == '7' &&
            _initramfs_start[2] == '0' && _initramfs_start[3] == '7' &&
            _initramfs_start[4] == '0' && _initramfs_start[5] == '1') {
            //debug_puts("[kernel:initramfs] Valid CPIO archive detected.\n");
        } else {
            panic("The 'CPIO' archive is invalid.");
        }
    } else {
        panic("Initramfs is too small to be valid.");
    }
    pmm_init(128 * 1024 * 1024);
    heap_init();
    console_init(bi);
    if (!console_ready()) {
        panic("No VBE framebuffer, cannot init console.");
    }

    idt_init();
    pic_init();
    pit_init();
    ps2_init();
    vfs_init();
    initramfs_parse();

    sti();

    shell_banner();
    kprintf("\n");
    shell_init();
    shell_run();

    while(1) {
        hlt();	
    }
}
