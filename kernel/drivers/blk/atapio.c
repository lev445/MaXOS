#include "atapio.h"
#include "../../arch/x86_64/lib/io.h"
#include "../../printf.h"
#include "../../string.h"


static int drive_count = 0;
static int drives_present[ATA_MAX_DRIVES] = {0, 0, 0, 0};

static ata_channel_t channels[2] = {
    { ATA_PRIMARY_BASE, ATA_PRIMARY_CTRL, 0xA0 },
    { ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, 0xA0 }
};


//waiting while the disk is busy
static int ata_wait_bsy(uint8_t channel) {
	uint16_t port = channels[channel].base + ATA_REG_STATUS;
	int timeout = 100000;
	while (--timeout) {
		if (!(inb(port) & ATA_SR_BSY)) {
			return 0;
		}
	} return -1; // timeout
}

// wait , while disk is ready to send some data. 
static int ata_wait_drq(uint8_t channel) {
	uint16_t port = channels[channel].base + ATA_REG_STATUS;
	int timeout = 100000;
	while (--timeout) {
		if (inb(port) & ATA_SR_DRQ) {
			return 0;
		}
	}
	return -1; // timeout
}

static int ata_wait_ready(uint8_t channel) {
    uint16_t port = channels[channel].base + ATA_REG_STATUS;
    uint8_t status;
    int timeout = 100000;
    while (--timeout) {
        status = inb(port);
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) return -1;
            return 0;
        }
    }
    return -1;  // timeout
}

void ata_init(void) {
    kprintf("Initializing ATA PIO...\n");
    uint16_t buf[256];
    drive_count = 0;
    
    for (int drive = 0; drive < ATA_MAX_DRIVES; drive++) {
        if (ata_identify(drive, buf) == 0) {
            char model[41];
            for (int i = 0; i < 40; i += 2) {
                model[i] = (buf[27 + i/2] >> 8) & 0xFF;
                model[i+1] = buf[27 + i/2] & 0xFF;
            }
            model[40] = 0;
            kprintf("Drive %d detected, model %s\n", drive, model);
            drives_present[drive] = 1;
            drive_count++;
        } else {
            kprintf("Drive %d: not found\n", drive);
        }
    }
    if (drive_count == 0) {
        //kprintf("WARNING: No ATA drives detected!\n");
        kprintf("");
    }
}


int ata_identify(uint8_t drive, uint16_t *buf) {
    uint8_t channel = drive / 2;  // 0 = primary, 1 = secondary
    uint8_t slave = drive % 2;
    uint16_t base = channels[channel].base;
    uint16_t ctrl = channels[channel].ctrl;
    uint8_t dev = channels[channel].drive | (slave << 4);
    
    outb(base + ATA_REG_DEVICE, dev);
    outb(base + ATA_REG_SECCOUNT, 0);
    outb(base + ATA_REG_LBA_LO, 0);
    outb(base + ATA_REG_LBA_MID, 0);
    outb(base + ATA_REG_LBA_HI, 0);
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    uint8_t status = inb(base + ATA_REG_STATUS);
    if (status == 0) return -1;  // disk not found
    
    if (ata_wait_ready(channel) != 0) return -1;
    
    // read 256 words
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(base + ATA_REG_DATA);
    }
    return 0;
}

int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t count, const void *buf) {
    uint8_t channel = drive / 2;
    uint8_t slave = drive % 2;
    uint16_t base = channels[channel].base;
    uint8_t dev = channels[channel].drive | (slave << 4) | 0x40 | ((lba >> 24) & 0x0F);
    
    outb(base + ATA_REG_DEVICE, dev);
    outb(base + ATA_REG_SECCOUNT, count);
    outb(base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(base + ATA_REG_COMMAND, ATA_CMD_WRITE);
    
    if (ata_wait_ready(channel) != 0) return -1;
    
    const uint16_t *data = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_drq(channel);
        for (int i = 0; i < 256; i++) {
            outw(base + ATA_REG_DATA, data[i]);
        }
        data += 256;
    }
    
    // FLUSH CACHE after write
    outb(base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    ata_wait_ready(channel);
    
    return 0;
}

int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t count, void *buf) {
	uint8_t channel = drive /2;
	uint8_t slave = drive % 2;
	uint16_t base = channels[channel].base;
	uint8_t dev = channels[channel].drive | (slave << 4) | 0x40 | ((lba >> 24) & 0x0F);
	outb(base + ATA_REG_DEVICE, dev);
	outb(base + ATA_REG_SECCOUNT, count);
	outb(base + ATA_REG_LBA_LO, lba & 0xFF);
	outb(base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
	outb(base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
	outb(base + ATA_REG_COMMAND, 0x20); // ATA_CMD_READ
	if (ata_wait_ready(channel) !=0) {
		return -1;
	}
	uint16_t *data = (uint16_t *)buf;
	for (int s=0; s < count; s++) {
		if (ata_wait_drq(channel) !=0) {
			return-1;
		}
		for (int i = 0; i < 256; i++) {
			data[i] = inw(base + ATA_REG_DATA); // read from port
		}
		data += 256;
	}
	return 0;
}

int ata_get_drv_count(void) {
    return drive_count;
}

int ata_is_drv_presnt(uint8_t drive) {
    if (drive >= ATA_MAX_DRIVES) return 0;
    return drives_present[drive];
}
