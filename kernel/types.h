#ifndef MAXOS_TYPES_H
#define MAXOS_TYPES_H
#define E820_MAX 32

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed)) e820_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t ok;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint64_t fb;
    uint8_t  rpos, gpos, bpos;
    uint8_t  rmask, gmask, bmask;
    uint8_t  _pad[2];
    uint32_t e820_count;
    e820_entry_t e820[E820_MAX];
} __attribute__((packed)) boot_info_t;

#define BOOTINFO_ADDR 0x30400u

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif
