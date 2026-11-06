#include "dev.h"
#include "drivers/blk/atapio.h"
#include "printf.h"
#include "string.h"

#define MAX_BLOCK_DEVICES 4

static block_device_t devices[MAX_BLOCK_DEVICES];
static int num_devices =0;

void dev_init(void) {
	kprintf("Initializing block devices...\n");
	for (uint8_t drive = 0; drive < 4; drive++) {
		uint16_t identify_buf[256];
		if (ata_identify(drive, identify_buf) == 0) {
			devices[num_devices].drive = drive;
			devices[num_devices].sector_count = 
			    ((uint64_t)identify_buf[61] << 16) | identify_buf[60];
			devices[num_devices].sector_size = 512;
			kprintf("Reached /dev/hd%c.", num_devices);
			num_devices++;
		}
	}
	kprintf("Summary: found %d blk device(s)\n", num_devices);
}

int dev_read_block(uint8_t dev_id, uint64_t sector, void *buf) {
    if (dev_id >= num_devices) return -1;
    return ata_read_sector(devices[dev_id].drive, sector, 1, buf);
}

int dev_write_block(uint8_t dev_id, uint64_t sector, const void *buf) {
    if (dev_id >= num_devices) return -1;
    return ata_write_sector(devices[dev_id].drive, sector, 1, buf);
}
