/* 6/11/26 -- blk devices (for example disks and etc.) */
#ifndef DEV_H
#define DEV_H

#include "types.h"

typedef struct {
	uint8_t drive; // 0-3 [ata drive num]
	uint64_t sector_count;
	uint32_t sector_size;
} block_device_t;

void dev_init(void); 
int dev_read_block(uint8_t dev_id, uint64_t sector, void *buf); 
int dev_write_block(uint8_t dev_id, uint64_t sector, const void *buf); 

#endif
