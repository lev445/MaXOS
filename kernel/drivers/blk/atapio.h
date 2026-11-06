/* 6/11/26 -- ATAPIO driver. */
#ifndef ATA_H
#define ATA_H

#include "../../types.h"

#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DEVICE      0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_READ        0x20
#define ATA_CMD_WRITE       0x30
#define ATA_CMD_FLUSH       0xE7
#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

typedef struct {
	uint16_t base;
	uint16_t ctrl;
	uint8_t drive; // 0xA0 master, 0xB0 slave
} ata_channel_t;

void ata_init(void);
int ata_identify(uint8_t drive, uint16_t *buf);
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t count, void *buf);
int ata_write_sector(uint8_t drive, uint32_t lba, uint8_t count, const void *buf);

#endif
