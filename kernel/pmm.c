#include "pmm.h"
#include "printf.h"
#include "string.h"

#define PAGE_SIZE 4096
#define MAX_PAGES 32768  // 128 mib
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE] __attribute__((aligned(4096)));
static uint64_t total_pages = 0;

void pmm_init(uint64_t memory_size_bytes) {
    total_pages = memory_size_bytes / PAGE_SIZE;
    if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;
    memset(pmm_bitmap, 0xFF, BITMAP_SIZE);
    for (uint64_t i = 1024; i < total_pages; i++) {
        pmm_bitmap[i / 8] &= ~(1 << (i % 8));
    }

    kprintf("[pmm] Initialized: %lu MB total, %lu pages\n", 
            memory_size_bytes / (1024 * 1024), total_pages);
    kprintf("[pmm] Reserved: 0x000000-0x400000 (4 MB)\n");
    kprintf("[pmm] Free: 0x400000-%p\n", (void *)((total_pages - 1) * PAGE_SIZE));
}

void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        uint8_t byte = pmm_bitmap[i / 8];
        uint8_t bit = 1 << (i % 8);
        
        if (!(byte & bit)) {
            pmm_bitmap[i / 8] |= bit;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *addr) {
    uint64_t index = ((uint64_t)addr) / PAGE_SIZE;
    if (index < total_pages) {
        pmm_bitmap[index / 8] &= ~(1 << (index % 8));
    }
}

void pmm_get_stats(uint64_t *total_bytes, uint64_t *used_bytes, uint64_t *free_bytes) {
	uint64_t used_pages = 0;
	for (uint64_t i = 0; i < total_pages; i++) {
		if (pmm_bitmap[i / 8] & (1 << (i % 8))) {
			used_pages++;
		}
	}
	uint64_t free_pages = total_pages - used_pages;
	if (total_bytes) *total_bytes = total_pages * 4096;
	if (used_bytes)  *used_bytes  = used_pages * 4096;
	if (free_bytes)  *free_bytes  = free_pages * 4096;
}
