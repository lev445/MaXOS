#include "heap.h"
#include "pmm.h"
#include "printf.h"
#include "string.h"

typedef struct heap_block {
    uint64_t size;
    uint8_t is_free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;

void heap_init(void) {
    void *page = pmm_alloc_page();
    if (!page) {
        kprintf("!!! PANIC: Cannot allocate initial page!\n");
        return;
    }
    
    heap_head = (heap_block_t *)page;
    heap_head->size = 4096 - sizeof(heap_block_t);
    heap_head->is_free = 1;
    heap_head->next = NULL;
    
    //kprintf("Initialized\n");
}

void *kmalloc(uint64_t size) {
	if (size ==0) return NULL;
	size = (size + 15) & ~15;
	heap_block_t *current = heap_head;
	while (current) {
	    if (current->is_free && current->size >= size) {
	        current->is_free = 0;
	        return (void *)((uint8_t *)current + sizeof(heap_block_t));
	    }
	    current = current->next;
	}
	void *new_page = pmm_alloc_page();
	if (!new_page) {
		kprintf("OOM (Out-of-memory)\n");
		return NULL;
	}
	heap_block_t *new_block = (heap_block_t *)new_page;
	new_block->size = 4096 - sizeof(heap_block_t);
	new_block->is_free = 0;
	new_block->next = NULL;
	current = heap_head;
	while (current->next) current = current->next;
	current->next = new_block;
	return (void *)((uint8_t *)new_block + sizeof(heap_block_t));
}

void kfree(void *ptr) {
	if (!ptr) return;
	heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
	block->is_free = 1;
	if (block->next && block->next->is_free) {
	    block->size += sizeof(heap_block_t) + block->next->size;
	    block->next = block->next->next;
	}
}
