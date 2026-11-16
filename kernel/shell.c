#include "shell.h"
#include "console.h"
#include "ps2.h"
#include "printf.h"
#include "string.h"
#include "pit.h"
#include "rtc.h"
#include "types.h"
#include "arch/x86_64/lib/io.h"
#include "font.h"
#include "cpuid.h"
#include "vfs.h"
#include "pmm.h"
#include "initramfs.h"
#include "loader.h"

#define PROMPT_MAX  128
static char PROMPT[PROMPT_MAX];
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
static void build_cwd_path(char *buf, size_t bufsize);
void update_prompt(void);

void shell_register(const char *name, const char *help, shell_cmd_fn fn) {
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
    char cwd[PROMPT_MAX];
    build_cwd_path(cwd, sizeof(cwd));
    snprintf(PROMPT, PROMPT_MAX, "sh:%s ", cwd);
    
    prompt_len = (int)strlen(PROMPT);
    
    console_set_cursor(px, py);
    console_write(PROMPT, prompt_len);
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
    char cwd[PROMPT_MAX];
    build_cwd_path(cwd, sizeof(cwd));
    snprintf(PROMPT, PROMPT_MAX, "levvashuck@MaXOS:%s$ ", cwd);
    prompt_len = (int)strlen(PROMPT);
    
    console_get_cursor(&px, &py);
    max_input = console_cols() - px - prompt_len - 1;
    if (max_input > LINE_MAX - 1)
        max_input = LINE_MAX - 1;
    redraw();
}

static int wait_key(void) {
    uint64_t last = pit_ticks() / 30;
    while(1) {
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
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    if (!argc) return;
    if (argv[0][0] == '.' && argv[0][1] == '/') {
        const char *program = argv[0] + 2;
        load_program(program);
        return;
    }
    for (int i = 0; i < ncmds; i++) {
        if (!strcasecmp(argv[0], cmds[i].name)) {
            cmds[i].fn(argc, argv);
            return;
        }
    }
    kprintf("sh: %s: command not found\n", argv[0]);
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
    
    if (current_dir->child_count == 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < current_dir->child_count; i++) {
        fs_node_t *child = current_dir->children[i];
        
        if (child->is_directory) {
            kprintf("\033[1;94m%s/\033[0m  ", child->name);
        } else {
            const char *ext = strrchr(child->name, '.');
            if (ext && strcmp(ext, ".max") == 0) {
                kprintf("\033[1;32m%s\033[0m  ", child->name);
            } else if (ext && (strcmp(ext, ".txt") == 0 || strcmp(ext, ".md") == 0)) {
                kprintf("\033[37m%s\033[0m  ", child->name);
            } else {
                kprintf("%s  ", child->name);
            }
        }
        count++;
    }
    kprintf("\nTotal: %d entries\n", count);
    return 0;
}

static int c_cd(int argc, char **argv) {
    if (argc < 2) {
        current_dir = &root_node;
        return 0;
    }

    const char *target = argv[1];

    if (strcmp(target, "/") == 0) {
        current_dir = &root_node;
    } else if (strcmp(target, ".") == 0) {
    } else if (strcmp(target, "..") == 0) {
        if (current_dir->parent != NULL) {
            current_dir = current_dir->parent;
        }
    } else {
        fs_node_t *next_dir = find_child(current_dir, target);
        if (next_dir && next_dir->is_directory) {
            current_dir = next_dir;
        } else {
            kprintf("cd: %s: No such directory\n", target);
            return -1;
        }
    }
    return 0;
}

static int c_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: cat <filename>\n");
        return -1;
    }

    const char *filename = argv[1];
    fs_node_t *target = find_child(current_dir, filename);
    if (!target) {
        kprintf("cat: %s: No such file or directory\n", filename);
        return -1;
    }

    if (target->is_directory) {
        kprintf("cat: %s: Is a directory\n", filename);
        return -1;
    }
    if (target->size == 0 || !target->data) {
        kprintf("\n");
        return 0;
    }
    for (uint32_t i = 0; i < target->size; i++) {
        kprintf("%c", ((char *)target->data)[i]);
    }
    kprintf("\n");

    return 0;
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

static void build_cwd_path(char *buf, size_t bufsize) {
    if (current_dir == &root_node) {
        buf[0] = '/';
        buf[1] = '\0';
        return;
    }
    const char *path_parts[32];
    int depth = 0;
    fs_node_t *curr = current_dir;
    while (curr != &root_node && curr != NULL && depth < 32) {
        path_parts[depth++] = curr->name;
        curr = curr->parent;
    }

    int pos = 0;
    buf[pos++] = '/';
    for (int i = depth - 1; i >= 0; i--) {
        const char *part = path_parts[i];
        int len = strlen(part);
        if (pos + len + 1 < bufsize) {
            memcpy(buf + pos, part, len);
            pos += len;
            if (i > 0) {
                buf[pos++] = '/';
            }
        }
    }
    buf[pos] = '\0';
}


static int c_xxd(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: xxd <filename>\n");
        return -1;
    }

    const char *filename = argv[1];
    fs_node_t *file = find_child(current_dir, filename);
    if (!file) {
        kprintf("xxd: %s: No such file or directory\n", filename);
        return -1;
    }

    if (file->is_directory) {
        kprintf("xxd: %s: Is a directory\n", filename);
        return -1;
    }

    if (file->size == 0 || !file->data) {
        kprintf("xxd: %s: Empty file\n", filename);
        return 0;
    }

    uint8_t *data = file->data;
    uint32_t size = file->size;

    for (uint32_t i = 0; i < size; i += 16) {
        kprintf("%08x: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < size) {
                kprintf("%02x", data[i + j]);
            } else {
                kprintf("  ");
            }
            if (j % 2 == 1) kprintf(" ");
        }
        kprintf(" ");
        for (int j = 0; j < 16 && i + j < size; j++) {
            uint8_t c = data[i + j];
            if (c >= 32 && c < 127) {
                kprintf("%c", c);
            } else {
                kprintf(".");
            }
        }

        kprintf("\n");
    }

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
    kprintf("Build birth: 7/11/26.\n");
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
        console_puts("\n");
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

static int c_touch(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: touch <filename>\n");
        return -1;
    }
    
    fs_node_t *file = create_file(current_dir, argv[1]);
    if (!file) {
        kprintf("touch: cannot create '%s': File exists or error\n", argv[1]);
        return -1;
    }
    
    kprintf("Created file: %s\n", argv[1]);
    return 0;
}

static int c_mkdir(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: mkdir <dirname>\n");
        return -1;
    }
    
    fs_node_t *dir = create_dir(current_dir, argv[1]);
    if (!dir) {
        kprintf("mkdir: cannot create '%s': Directory exists or error\n", argv[1]);
        return -1;
    }
    
    kprintf("Created directory: %s/\n", argv[1]);
    return 0;
}

static const char *banner_art[1] = {
	"MaXOS is booted..."
};

static int c_panic(int argc, char **argv) {
	(void)argc; (void)argv;
	__asm__ __volatile__("ud2");
	return 0;
}

static int c_file(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: file <filename>\n");
        return -1;
    }

    fs_node_t *node = find_child(current_dir, argv[1]);
    if (!node) {
        kprintf("file: %s: No such file\n", argv[1]);
        return -1;
    }

    if (node->is_directory) {
        kprintf("%s: directory\n", argv[1]);
        return 0;
    }

    if (node->size == 0 || !node->data) {
        kprintf("%s: empty\n", argv[1]);
        return 0;
    }
    if (node->size >= 4) {
        if (node->data[0] == 'M' && node->data[1] == 'A' && 
            node->data[2] == 'X' && node->data[3] == '1') {
            kprintf("%s: MaXOS executable (.max), entry=0x%08x, size=%u bytes\n", 
                    argv[1], 
                    *(uint32_t*)(node->data + 4), // entry point
                    *(uint32_t*)(node->data + 8) + 12); // code size + header
            return 0;
        }
    }
    bool is_text = true;
    uint32_t check_len = (node->size < 256) ? node->size : 256;
    for (uint32_t i = 0; i < check_len; i++) {
        uint8_t c = node->data[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            is_text = false;
            break;
        }
    }

    if (is_text) {
        kprintf("%s: ASCII text\n", argv[1]);
    } else {
        kprintf("%s: data (unknown format)\n", argv[1]);
    }

    return 0;
}

static int cmd_lscpu(int argc, char **argv) {
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

static int fetch(int argc, char **argv) {
	(void)argc; (void)argv; 
	uint32_t eax, ebx, ecx, edx;
	uint64_t total, used, free;
	char brand[49];
	char vendor[13];
	memcpy(vendor + 0, &ebx, 4);
	memcpy(vendor + 4, &edx, 4);
	memcpy(vendor + 8, &ecx, 4);
	vendor[12] = '\0';
	cpuid_get_brand(brand);
	pmm_get_stats(&total, &used, &free);
	uint64_t total_mb = total / (1024 * 1024);
	uint64_t used_mb = used / (1024 * 1024);
	uint64_t free_mb = free / (1024 * 1024);
	uint64_t ms = uptime_ms();
	uint64_t s = ms / 1000;
	kprintf("OS: MaXOS, 0.1.0\n");
	kprintf("Kernel: MaXKernel\n");
	kprintf("CPU: %s\n", brand);
	kprintf("RAM: %lu MiB\n", total_mb);
	kprintf("Uptime: %02lu\n",
	        (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60),
	        (unsigned long)(s % 60), (unsigned long)pit_ticks(), PIT_HZ);
	
	return 0;
}

void shell_banner(void) {
    for (int i = 0; i < 1; i++) {
        int x, y;
        console_get_cursor(&x, &y);
        console_puts(banner_art[i]);
        if (i < 2) {
            console_set_cursor(32, y);
            //console_puts(banner_side[i]);
        }
        console_putc('\n');
    }
    console_set_color(COL_LGRAY, COL_BLACK);
}

void shell_init(void) {
    shell_register("help",    "list commands", c_help);
    shell_register("echo",    "print arguments", c_echo);
    shell_register("clear",   "clear screen", c_clear);
    //shell_register("cls",     "clear screen", c_clear);
    shell_register("ver",     "show version", c_ver);
    shell_register("uptime",  "show uptime and ticks", c_uptime);
    shell_register("date",    "show RTC date/time", c_date);
    shell_register("fbinfo", "show framebuffer info", c_vbeinfo);
    shell_register("meminfo", "show memory layout", c_meminfo);
    //shell_register("color",   "set colors: color <fg> [bg]", c_color);
    //shell_register("mouse",   "mouse on|off|status", c_mouse);
    //shell_register("kbd",     "keyboard status", c_kbd);
    shell_register("history", "show command history", c_history);
    //shell_register("banner",  "redraw banner", c_banner);
    shell_register("reboot",  "reboot the machine", c_reboot);
    shell_register("halt",    "power off (QEMU)", c_halt);
    shell_register("shutdown","power off (QEMU)", c_halt);
    shell_register("lscpu", "print CPU information", cmd_lscpu);
    shell_register("lsmem", "show memory map", c_lsmem);
    shell_register("free", "show free ram", c_free);
    shell_register("ls", "list files", c_ls);
    shell_register("cat", "show file contents", c_cat);
    shell_register("cd",      "change directory", c_cd);
    shell_register("touch",     "create empty file", c_touch);
    shell_register("mkdir",     "create directory", c_mkdir);
    shell_register("xxd", "hex dump of file", c_xxd);
    shell_register("file", "determine file type", c_file);
    shell_register("panic", "via 'ud2' return panic", c_panic);
    shell_register("fetch", "fastfetch analogue", fetch);
}



void shell_run(void) {
    console_cursor_visible(1);
    new_prompt();
    while (1) {
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
