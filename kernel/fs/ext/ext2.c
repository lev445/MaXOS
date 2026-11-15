/* EXT2 DRIVER 
 * EXT2 File system driver
 * 7/11/26 MaXOS -- ext2 driver main file 
*/
#include "ext2.h"
#include "../../drivers/blk/atapio.h"
#include "../../printf.h"
#include "../../string.h"

static ext2_superblock_t sb;

void ext2_init(uint8_t drive) {
	kprintf("Initializing file system on drive %u....\n",drive);
	uint8_t buf[1024];
	if (ata_read_sector(drive,2,1,buf)!=0) {
		kprintf("Err, failed to read superblock.\n");
		return;
	}
	memcpy(&sb, buf, sizeof(ext2_superblock_t));
	if (sb.s_magic != EXT2_SUPER_MAGIC) {
		kprintf("Err, the file system is invalid, magic: 0x%04X. Please use ext2 filesystem\nBecause MaXOS is support only ext2.\n",sb.s_magic);
		return;
	}
	kprintf("Valid superblock found.\n");
	kprintf("Inodes: %u|Blocks: %u\n", sb.s_inodes_count, sb.s_blocks_count);
	kprintf("Block size: %ubytes\n", 1024 << sb.s_log_block_size);
}

int ext2_scan_all(void) {
    kprintf("Scanning all drives for ext2 filesystem...\n");
    int found = 0;
    
    for (int drive = 0; drive < ATA_MAX_DRIVES; drive++) {
        if (!ata_is_drv_presnt(drive)) continue;
        
        uint8_t buf[1024];
        if (ata_read_sector(drive, 2, 1, buf) != 0) {
            kprintf("  [ -- ] Drive %d: Read error\n", drive);
            continue;
        }
        
        ext2_superblock_t *sb = (ext2_superblock_t *)buf;
        if (sb->s_magic == EXT2_SUPER_MAGIC) {
            kprintf("%d drive ok\n", drive);
            found++;
        } else {
            kprintf("Drive %d err, not a ext2. Magic: 0x%04X)\n", drive, sb->s_magic);
        }
    }
    
    if (found == 0) {
        kprintf("No ext2 filesystems found.\n");
    } else {
        kprintf("Found %d ext2 filesystem(s).\n", found);
    }
    
    return found;
}
