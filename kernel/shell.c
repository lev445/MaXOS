#include "shell.h"
#include "console.h"
#include "ps2.h"
#include "printf.h"
#include "string.h"
#include "pit.h"
#include "rtc.h"
#include "types.h"
#include "io.h"
#include "font.h"
#include "cpuid.h"
#include "vfs.h"
#include "pmm.h"

#define PROMPT     "# "
#define MAX_CMDS   48
#define LINE_MAX   256
#define HIST_MAX   16

typedef struct {
    const char *name;
    const char *help;
    shell_cmd_fn fn;
} cmd_t;

static cmd_t cmds[MAX_CMDS];
static int ncmds;

void shell_register(const char *name, const char *help, shell_cmd_fn fn)
{
    if (ncmds < MAX_CMDS) {
        cmds[ncmds].name = name;
        cmds[ncmds].help = help;
        cmds[ncmds].fn = fn;
        ncmds++;
    }
}

/* line editor */
static char line[LINE_MAX];
static int len, pos, shown;
static int px, py, max_input;
static int prompt_len;
static int blink_vis = 1;

static char hist[HIST_MAX][LINE_MAX];
static int hcount, hcur;

static inline int input_x(void) { return px + prompt_len + pos; }

static void redraw(void) {
    line[len] = 0;
    console_set_cursor(px, py);
    console_set_color(COL_LCYAN, COL_BLACK);
    console_puts(PROMPT);
    console_set_color(COL_LGRAY, COL_BLACK);
    console_write(line, len);
    for (int i = len; i < shown; i++)
        console_putc(' ');
    shown = len;
    console_set_cursor(input_x(), py);
}

static void new_prompt(void) {
    len = pos = shown = 0;
    hcur = -1;
    prompt_len = (int)strlen(PROMPT);
    console_get_cursor(&px, &py);
    max_input = console_cols() - px - prompt_len - 1;
    if (max_input > LINE_MAX - 1)
        max_input = LINE_MAX - 1;
    redraw();
}

static int wait_key(void) {
    uint64_t last = pit_ticks() / 30;
    for (;;) {
        if (kbd_avail())
            return kbd_getchar();
        hlt();
        uint64_t now = pit_ticks() / 30;
        if (now != last) {
            last = now;
            blink_vis = !blink_vis;
            console_cursor_visible(blink_vis);
        }
    }
}

static void hist_prev(void) {
    if (hcount == 0)
        return;
    if (hcur < 0)
        hcur = hcount - 1;
    else if (hcur > 0)
        hcur--;
    else
        return;
    strncpy(line, hist[hcur], LINE_MAX - 1);
    line[LINE_MAX - 1] = 0;
    len = pos = (int)strlen(line);
    redraw();
}
static void hist_next(void) {
    if (hcur < 0)
        return;
    if (hcur + 1 < hcount) {
        hcur++;
        strncpy(line, hist[hcur], LINE_MAX - 1);
    } else {
        hcur = -1;
        line[0] = 0;
    }
    line[LINE_MAX - 1] = 0;
    len = pos = (int)strlen(line);
    redraw();
}

static void hist_push(const char *s) {
    if (!*s)
        return;
    if (hcount && !strcmp(hist[hcount - 1], s))
        return;
    if (hcount < HIST_MAX) {
        strncpy(hist[hcount++], s, LINE_MAX - 1);
    } else {
        for (int i = 0; i < HIST_MAX - 1; i++)
            strcpy(hist[i], hist[i + 1]);
        strncpy(hist[HIST_MAX - 1], s, LINE_MAX - 1);
    }
}

static void complete(void) {
    if (strchr(line, ' '))
        return;
    int matches = 0, idx = -1;
    for (int i = 0; i < ncmds; i++) {
        if (len && !strncmp(cmds[i].name, line, len)) {
            matches++;
            idx = i;
        }
    }
    if (matches == 1) {
        const char *full = cmds[idx].name;
        int fl = (int)strlen(full);
        if (fl <= max_input) {
            strcpy(line, full);
            if (fl < max_input) {
                line[fl] = ' ';
                line[fl + 1] = 0;
                len = pos = fl + 1;
            } else {
                len = pos = fl;
            }
            redraw();
        }
    } else if (matches > 1) {
        char save[LINE_MAX];
        int slen = len, spos = pos;
        memcpy(save, line, LINE_MAX);
        console_putc('\n');
        for (int i = 0; i < ncmds; i++)
            if (!strncmp(cmds[i].name, line, len))
                kprintf("  %s\n", cmds[i].name);
        new_prompt();
        memcpy(line, save, LINE_MAX);
        len = slen;
        pos = spos;
        redraw();
    }
}

/* execute command  */
static void execute(const char *s) {
    char buf[LINE_MAX];
    strncpy(buf, s, LINE_MAX - 1);
    buf[LINE_MAX - 1] = 0;
    char *argv[32];
    int argc = 0;
    char *p = buf;
    while (*p && argc < 32) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = 0;
    }
    if (!argc)
        return;
    for (int i = 0; i < ncmds; i++) {
        if (!strcasecmp(argv[0], cmds[i].name)) {
            cmds[i].fn(argc, argv);
            return;
        }
    }
    kprintf("sh: unknown command '%s'.\n", argv[0]);
}

/* builtins */
static int c_help(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Available commands:\n");
    for (int i = 0; i < ncmds; i++)
        kprintf("  %-10s %s\n", cmds[i].name, cmds[i].help);
    return 0;
}

static int c_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        kprintf("%s%s", argv[i], i + 1 < argc ? " " : "");
    console_putc('\n');
    return 0;
}

static int c_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    console_clear();
    return 0;
}

static const char *e820_type_name(uint32_t type) {
    switch (type) {
        case 1: return "available";
        case 2: return "reserved";
        case 3: return "ACPI reclaimable";
        case 4: return "ACPI NVS";
        case 5: return "bad memory";
        default: return "unknown";
    }
}

static int c_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    vfs_file_t *root = vfs_get_root();
    if (!root) {
        kprintf("ls: VFS not initialized\n");
        return -1;
    }
    
    vfs_file_t *file = root->children;
    if (!file) {
        kprintf("(empty)\n");
        return 0;
    }
    
    int count = 0;
    while (file) {
        kprintf("%s  ", file->name);
        count++;
        file = file->next;
    }
    kprintf("\nTotal: %d entries\n", count);
    return 0;
}

static int c_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: cat <filename>\n");
        return -1;
    }
    
    vfs_file_t *root = vfs_get_root();
    vfs_file_t *file = root->children;
    while (file) {
        if (strcmp(file->name, argv[1]) == 0) {
            if (file->data && file->size > 0) {
                for (uint32_t i = 0; i < file->size; i++)
                    kprintf("%c", ((char *)file->data)[i]);
                kprintf("\n");
            } else {
                kprintf("(empty)\n");
            }
            return 0;
        }
        file = file->next;
    }
    kprintf("cat: %s: No such file\n", argv[1]);
    return -1;
}

static int c_lsmem(int argc, char **argv) {
    (void)argc; (void)argv;
    
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)BOOTINFO_ADDR;
    
    if (bi->magic != 0x31454256) {
        kprintf("no boot info\n");
        return 1;
    }
    
    if (bi->e820_count == 0) {
        kprintf("no E820 memory map available\n");
        return 1;
    }
    
    kprintf("RANGE                                  SIZE  TYPE\n");
    
    uint64_t total_available = 0;
    uint64_t total_reserved = 0;
    
    for (uint32_t i = 0; i < bi->e820_count; i++) {
        const e820_entry_t *e = &bi->e820[i];
        uint64_t end = e->base + e->length - 1;

        uint64_t size_mb = e->length / (1024 * 1024);
        const char *unit = "M";
        if (size_mb == 0) {
            size_mb = e->length / 1024;
            unit = "K";
        }
        
        kprintf("0x%016lx-0x%016lx  %4lu%s  %s\n",
                (unsigned long)e->base,
                (unsigned long)end,
                (unsigned long)size_mb,
                unit,
                e820_type_name(e->type));
        
        if (e->type == 1) total_available += e->length;
        else total_reserved += e->length;
    }
    
    kprintf("\nMemory block size:       2M\n");
    kprintf("Total online memory:     %luM\n", 
            (unsigned long)(total_available / (1024 * 1024)));
    //kprintf("Total offline memory:    0B\n");
    
    return 0;
}

static int c_free(int argc, char **argv) {
    (void)argc; 
    (void)argv;

    uint64_t total = 0, used = 0, free = 0;
    pmm_get_stats(&total, &used, &free);
    uint64_t total_mb = total / (1024 * 1024);
    uint64_t used_mb = used / (1024 * 1024);
    uint64_t free_mb = free / (1024 * 1024);
    uint64_t available_mb = free_mb;
    kprintf("[total used free available]\n");
    kprintf("Mem: %lu %lu %lu %lu\n",
            total_mb, used_mb, free_mb, available_mb);

    return 0;
}

static int c_ver(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("[--- MaXOS 0.1.0 ---]\n");
    kprintf("Build name: maxos-rc-1 (Release Candidate).\n");
    kprintf("Build birth: 3/11/26.\n");
    kprintf("If you find any bugs, write to: levvashuck18@gmail.com.\n");
    return 0;
}

static int c_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t ms = uptime_ms();
    uint64_t s = ms / 1000;
    kprintf("up %lu:%02lu:%02lu\n",
            (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60),
            (unsigned long)(s % 60), (unsigned long)pit_ticks(), PIT_HZ);
    return 0;
}

static const char *mon_name[13] = { "", "Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec" };
static int c_date(int argc, char **argv) {
    (void)argc; (void)argv;
    rtc_time_t t;
    rtc_read(&t);
    kprintf("%04d-%02d-%02d %02d:%02d:%02d (%s)\n",
            t.year, t.mon, t.day, t.hour, t.min, t.sec,
            (t.mon >= 1 && t.mon <= 12) ? mon_name[t.mon] : "?");
    return 0;
}

static int c_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    console_puts("Rebooting...\n");
    cli();
    outb(0x64, 0xFE); /* keyboard-controller reset (KCR) */
    while(1) {
    	hlt(); // but if KCR is not work, we run halt	
    }
    return 0;
}

static int c_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    console_puts("Halting.\n");
    cli();
    outw(0x604, 0x2000); /* QEMU shutdown */
    while(1) {
    	hlt();	
    }
    return 0;
}

static int c_history(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < hcount; i++)
        kprintf(" %2d  %s\n", i + 1, hist[i]);
    if (!hcount)
        console_puts("(empty)\n");
    return 0;
}

static int c_mouse(int argc, char **argv) {
    if (argc > 1) {
        if (!strcasecmp(argv[1], "on"))
            mouse_enable(1);
        else if (!strcasecmp(argv[1], "off"))
            mouse_enable(0);
    }
    int x, y, b;
    mouse_get(&x, &y, &b);
    kprintf("mouse: %s  pos=(%d,%d) buttons=%d%d%d\n",
            mouse_enabled() ? "on " : "off", x, y,
            (b >> 2) & 1, (b >> 1) & 1, b & 1);
    return 0;
}

static int c_kbd(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("keyboard: buffered=%d (press keys; Ctrl+C to exit test)\n", kbd_avail());
    return 0;
}

static int c_vbeinfo(int argc, char **argv) {
    (void)argc; (void)argv;
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)BOOTINFO_ADDR;
    if (bi->magic != 0x31454256) {
        console_puts("no boot info\n");
        return 1;
    }
    kprintf("vesa: %ux%u bpp=%u pitch=%u fb=0x%lx ok=%u\n",
            bi->width, bi->height, bi->bpp, bi->pitch,
            (unsigned long)bi->fb, bi->ok);
    kprintf("masks: R%u@%u G%u@%u B%u@%u\n",
            bi->rmask, bi->rpos, bi->gmask, bi->gpos, bi->bmask, bi->bpos);
    kprintf("console: %dx%d cells (%dx%d font)\n",
            console_cols(), console_rows(), FONT_W, FONT_H);
    return 0;
}

static int c_meminfo(int argc, char **argv) {
    (void)argc; (void)argv;
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)BOOTINFO_ADDR;
    kprintf("physical map : identity 0..4GiB\n");
    kprintf("kernel       : 0x100000.. (loaded by stage2)\n");
    kprintf("stack        : grows down from 0x80000\n");
    if (bi->ok)
        kprintf("framebuffer  : 0x%lx, %u KiB\n",
                (unsigned long)bi->fb,
                (unsigned)(bi->pitch * bi->height / 1024));
    return 0;
}

static const char *color_names[16] = {
    "black","blue","green","cyan","red","magenta","brown","lgray",
    "dgray","lblue","lgreen","lcyan","lred","lmagenta","yellow","white"
};
static int color_parse(const char *s)
{
    for (int i = 0; i < 16; i++)
        if (!strcasecmp(s, color_names[i]))
            return i;
    if (s[0] >= '0' && s[0] <= '9')
        return atoi(s) & 15;
    return -1;
}
static int c_color(int argc, char **argv)
{
    if (argc < 2) {
        uint8_t fg, bg;
        console_get_color(&fg, &bg);
        kprintf("color: fg=%s(%u) bg=%s(%u). usage: color <fg> [bg]\n",
                color_names[fg], fg, color_names[bg], bg);
        return 0;
    }
    int fg = color_parse(argv[1]);
    int bg = (argc > 2) ? color_parse(argv[2]) : COL_BLACK;
    if (fg < 0 || bg < 0) {
        console_puts("bad color\n");
        return 1;
    }
    console_set_color((uint8_t)fg, (uint8_t)bg);
    return 0;
}

static int c_banner(int argc, char **argv)
{
    (void)argc; (void)argv;
    extern void shell_banner(void);
    shell_banner();
    return 0;
}

static int c_fsstub(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_puts("filesystem not implemented yet (TODO: user FS layer)\n");
    return 1;
}

static const char *banner_art[5] = {
    " __  __      __  _____  ____",
    "|  \\/  | __ _\\ \\/ / _ \\/ ___|",
    "| |\\/| |/ _` |\\  / | | \\___ \\",
    "| |  | | (_| |/  \\ |_| |___) |",
    "|_|  |_|\\__,_/_/\\_\\___/|____/",
};
static const char *banner_side[2] = {
	"Welcome to MaXOS."
	" "
};

static int cmd_lscpu(int argc, char **argv)
{
    (void)argc; (void)argv;     
    uint32_t eax, ebx, ecx, edx;
    char brand[49];

    // Vendor string (leaf 0)
    cpuid(0, &eax, &ebx, &ecx, &edx);
    char vendor[13];
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';

    kprintf("CPU Information:\n");
    kprintf("  Vendor:    %s\n", vendor);

    // Brand
    cpuid_get_brand(brand);
    kprintf("  Model:     %s\n", brand);

    // Family/Model/Stepping (leaf 1)
    cpuid(1, &eax, &ebx, &ecx, &edx);
    uint32_t stepping  = eax & 0xF;
    uint32_t model     = (eax >> 4) & 0xF;
    uint32_t family    = (eax >> 8) & 0xF;
    uint32_t ext_model = (eax >> 16) & 0xF;
    uint32_t ext_fam   = (eax >> 20) & 0xFF;

    uint32_t disp_family = family + ext_fam;
    uint32_t disp_model  = (ext_model << 4) | model;

    kprintf("  Family:    %u\n", disp_family);
    kprintf("  Model:     %u\n", disp_model);
    kprintf("  Stepping:  %u\n", stepping);
    kprintf("\nFeatures:\n");
    kprintf("  FPU:       %s\n", (edx & (1 << 0))  ? "yes" : "no");
    kprintf("  PAE:       %s\n", (edx & (1 << 6))  ? "yes" : "no");
    kprintf("  APIC:      %s\n", (edx & (1 << 9))  ? "yes" : "no");
    kprintf("  MMX:       %s\n", (edx & (1 << 23)) ? "yes" : "no");
    kprintf("  SSE:       %s\n", (edx & (1 << 25)) ? "yes" : "no");
    kprintf("  SSE2:      %s\n", (edx & (1 << 26)) ? "yes" : "no");
    kprintf("  SSE3:      %s\n", (ecx & (1 << 0))  ? "yes" : "no");
    kprintf("  SSSE3:     %s\n", (ecx & (1 << 9))  ? "yes" : "no");
    kprintf("  SSE4.1:    %s\n", (ecx & (1 << 19)) ? "yes" : "no");
    kprintf("  SSE4.2:    %s\n", (ecx & (1 << 20)) ? "yes" : "no");
    kprintf("  AVX:       %s\n", (ecx & (1 << 28)) ? "yes" : "no");
    kprintf("  x2APIC:    %s\n", (ecx & (1 << 21)) ? "yes" : "no");
    kprintf("  RDRAND:    %s\n", (ecx & (1 << 30)) ? "yes" : "no");
    kprintf("  NX bit:    %s\n", cpuid_has_nx()    ? "yes" : "no");

    // Extended features (leaf 7)
    cpuid(7, &eax, &ebx, &ecx, &edx);
    kprintf("  AVX2:      %s\n", (ebx & (1 << 5))  ? "yes" : "no");
    kprintf("  BMI1:      %s\n", (ebx & (1 << 3))  ? "yes" : "no");
    kprintf("  BMI2:      %s\n", (ebx & (1 << 8))  ? "yes" : "no");
    kprintf("  SHA:       %s\n", (ebx & (1 << 29)) ? "yes" : "no");
    
    return 0;
}
void shell_banner(void)
{
    console_set_color(COL_LCYAN, COL_BLACK);
    for (int i = 0; i < 5; i++) {
        int x, y;
        console_get_cursor(&x, &y);
        console_puts(banner_art[i]);
        if (i < 2) {
            console_set_cursor(32, y);
            console_puts(banner_side[i]);
        }
        console_putc('\n');
    }
    console_set_color(COL_LGRAY, COL_BLACK);
}

int c_vfs_test(int argc, char **argv) {
    char wbuf[64] = "Hello, MaXOS VFS!\n";
    char rbuf[64] = {0};
    int ret;
    
    kprintf("=== VFS TEST START ===\n");
    
    // Тест записи
    kprintf("Writing to /test.txt...\n");
    ret = vfs_write("/test.txt", wbuf, strlen(wbuf));
    kprintf("vfs_write returned: %d\n", ret);
    
    // Тест чтения
    kprintf("Reading from /test.txt...\n");
    ret = vfs_read("/test.txt", rbuf, sizeof(rbuf) - 1);
    kprintf("vfs_read returned: %d\n", ret);
    kprintf("Data: %s\n", rbuf);
    
    // Проверка, что данные совпадают
    if (strcmp(wbuf, rbuf) == 0) {
        kprintf("SUCCESS: Data matches!\n");
    } else {
        kprintf("FAIL: Data mismatch!\n");
        kprintf("Expected: %s\n", wbuf);
        kprintf("Got: %s\n", rbuf);
    }
    
    // Edge cases
    kprintf("\n=== EDGE CASES ===\n");
    
    // Чтение несуществующего файла
    ret = vfs_read("/nonexistent.txt", rbuf, sizeof(rbuf));
    kprintf("Read nonexistent: %d (expected negative)\n", ret);
    
    // Запись с размером 0
    ret = vfs_write("/test.txt", wbuf, 0);
    kprintf("Write size 0: %d\n", ret);
    
    // Чтение с размером 0
    ret = vfs_read("/test.txt", rbuf, 0);
    kprintf("Read size 0: %d\n", ret);
    
    kprintf("=== VFS TEST END ===\n");
    return 0;
}

static int c_testfs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    const char *content = "Hello from dynamic MaXOS VFS!\nThis file was created at runtime!\n";
    uint32_t len = 0;
    while (content[len] != '\0') len++; // strlen
    
    int res = vfs_write("runtime_test.txt", content, len);
    if (res > 0) {
        kprintf("Success! Wrote %d bytes to 'runtime_test.txt'\n", res);
    } else {
        kprintf("Failed to write file.\n");
    }
    return 0;
}

void shell_init(void)
{
    shell_register("help",    "list commands", c_help);
    shell_register("echo",    "print arguments", c_echo);
    shell_register("clear",   "clear screen (alias: cls)", c_clear);
    shell_register("cls",     "clear screen", c_clear);
    shell_register("ver",     "show version", c_ver);
    shell_register("uptime",  "show uptime and ticks", c_uptime);
    shell_register("date",    "show RTC date/time", c_date);
    shell_register("vbeinfo", "show VESA/framebuffer info", c_vbeinfo);
    shell_register("meminfo", "show memory layout", c_meminfo);
    shell_register("color",   "set colors: color <fg> [bg]", c_color);
    shell_register("mouse",   "mouse on|off|status", c_mouse);
    shell_register("kbd",     "keyboard status", c_kbd);
    shell_register("history", "show command history", c_history);
    shell_register("banner",  "redraw banner", c_banner);
    shell_register("reboot",  "reboot the machine", c_reboot);
    shell_register("halt",    "power off (QEMU)", c_halt);
    shell_register("shutdown","power off (QEMU)", c_halt);
    shell_register("lscpu", "print CPU information", cmd_lscpu);
    shell_register("lsmem", "show memory map", c_lsmem);
    shell_register("free", "show ram", c_free);
    shell_register("vfs_test", "test vfs", c_vfs_test);
    shell_register("ls", "list files", c_ls);
    shell_register("cat", "show file contents", c_cat);
    shell_register("test", "test write", c_testfs);
}

void shell_run(void)
{
    console_cursor_visible(1);
    new_prompt();
    for (;;) {
        int c = wait_key();
        switch (c) {
        case '\n':
            console_cursor_visible(1);
            console_putc('\n');
            hist_push(line);
            execute(line);
            console_set_color(COL_LGRAY, COL_BLACK);
            new_prompt();
            break;
        case 8:                                   /* backspace */
            if (pos > 0) {
                memmove(line + pos - 1, line + pos, len - pos);
                len--; pos--;
                redraw();
            }
            break;
        case KEY_DEL:
            if (pos < len) {
                memmove(line + pos, line + pos + 1, len - pos - 1);
                len--;
                redraw();
            }
            break;
        case KEY_LEFT:
            if (pos > 0) { pos--; console_set_cursor(input_x(), py); }
            break;
        case KEY_RIGHT:
            if (pos < len) { pos++; console_set_cursor(input_x(), py); }
            break;
        case KEY_HOME:
            pos = 0; console_set_cursor(px + prompt_len, py);
            break;
        case KEY_END:
            pos = len; console_set_cursor(input_x(), py);
            break;
        case KEY_UP:   hist_prev(); break;
        case KEY_DOWN: hist_next(); break;
        case '\t':     complete(); break;
        case 3:                                     /* Ctrl+C */
            console_puts("^C\n");
            new_prompt();
            break;
        case 12:                                    /* Ctrl+L */
            console_clear();
            new_prompt();
            redraw();
            break;
        case 27:                                    /* Esc */
            len = pos = 0;
            redraw();
            break;
        default:
            if (c >= 32 && c < 127 && len < max_input) {
                memmove(line + pos + 1, line + pos, len - pos);
                line[pos] = (char)c;
                len++; pos++;
                redraw();
            }
            break;
        }
    }
}
