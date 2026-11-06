#include "types.h"
#include "io.h"
#include "printf.h"
#include "vfs.h"
#include "string.h"

extern uint8_t _initramfs_start[];
extern uint8_t _initramfs_end[];

typedef struct {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} __attribute__((packed)) cpio_header_t;

static void debug_puts(const char *s) {
    while (*s) outb(0xE9, *s++);
}

static uint32_t parse_hex8(const char *s) {
    uint32_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 4;
        if (s[i] >= '0' && s[i] <= '9') result += s[i] - '0';
        else if (s[i] >= 'A' && s[i] <= 'F') result += s[i] - 'A' + 10;
        else if (s[i] >= 'a' && s[i] <= 'f') result += s[i] - 'a' + 10;
    }
    return result;
}

void initramfs_parse(void) {
    uint8_t *ptr = _initramfs_start;
    uint8_t *end = _initramfs_end;
    uint32_t file_count = 0;
    
    //debug_puts("[initramfs] Parsing CPIO archive...\n");
    
    while (ptr + 110 <= end) {
        cpio_header_t *hdr = (cpio_header_t *)ptr;
        
        if (hdr->magic[0] != '0' || hdr->magic[1] != '7' || 
            hdr->magic[2] != '0' || hdr->magic[3] != '7' || 
            hdr->magic[4] != '0' || hdr->magic[5] != '1') {
            //debug_puts("[initramfs] Invalid CPIO magic, stopping\n");
            break;
        }
        
        uint32_t namesize = parse_hex8(hdr->namesize);
        uint32_t filesize = parse_hex8(hdr->filesize);
        uint32_t mode = parse_hex8(hdr->mode);
        
        char *filename = (char *)(ptr + 110);
        
        if (strcmp(filename, "TRAILER!!!") == 0) {
            //debug_puts("[initramfs] End of archive (TRAILER!!!)\n");
            break;
        }
        
        if (strcmp(filename, ".") == 0 || strcmp(filename, "./") == 0) {
            //debug_puts("[initramfs] Skipping root directory '.'\n");
            
            uint32_t name_padded = (110 + namesize + 3) & ~3;
            uint32_t data_padded = (filesize + 3) & ~3;
            
            ptr = ptr + name_padded + data_padded;
            continue;
        }
        
        char *clean_path = filename;
        if (filename[0] == '.' && filename[1] == '/') {
            clean_path = filename + 2;
        }
        
        //debug_puts("[initramfs] Found: ");
        //debug_puts(clean_path);
        //debug_puts(" (size=");
        
        char size_buf[12];
        int len = 0;
        uint32_t temp = filesize;
        if (temp == 0) size_buf[len++] = '0';
        else {
            char rev[12]; int rlen = 0;
            while (temp > 0) { rev[rlen++] = '0' + (temp % 10); temp /= 10; }
            for (int i = rlen - 1; i >= 0; i--) size_buf[len++] = rev[i];
        }
        size_buf[len] = 0;
        //debug_puts(size_buf);
        //debug_puts(")\n");
        
        vfs_file_t *file = vfs_create_file(clean_path, mode, 0, 0);
        if (file) {
            uint32_t name_padded = (110 + namesize + 3) & ~3;
            uint8_t *data = ptr + name_padded;
            file->data = data;
            file->size = filesize;
            file_count++;
        } else {
            //debug_puts("[initramfs] Warning: failed to create file\n");
        }
        
        uint32_t name_padded = (110 + namesize + 3) & ~3;
        uint32_t data_padded = (filesize + 3) & ~3;
        ptr = ptr + name_padded + data_padded;
    }
    
    //debug_puts("[initramfs] Parsing complete! Loaded ");
    char count_buf[12];
    int len = 0;
    uint32_t temp = file_count;
    if (temp == 0) count_buf[len++] = '0';
    else {
        char rev[12]; int rlen = 0;
        while (temp > 0) { rev[rlen++] = '0' + (temp % 10); temp /= 10; }
        for (int i = rlen - 1; i >= 0; i--) count_buf[len++] = rev[i];
    }
    count_buf[len] = 0;
    //debug_puts(count_buf);
    //debug_puts(" files\n");
}
