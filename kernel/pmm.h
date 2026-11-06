#ifndef PMM_H
#define PMM_H

#include "types.h"

void pmm_init(uint64_t memory_size_bytes);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
void pmm_get_stats(uint64_t *total_bytes, uint64_t *used_bytes, uint64_t *free_bytes);


#endif
