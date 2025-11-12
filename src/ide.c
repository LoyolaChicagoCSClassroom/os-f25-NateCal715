// src/ide.c
#include <stdint.h>
#include "ide.h"

// --- tiny port I/O helpers (AT&T inline asm) ---
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb_u8(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline uint16_t inw_u16(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// --- Primary IDE I/O & bits ---
enum {
    ATA_IO_DATA   = 0x1F0,
    ATA_IO_ERR    = 0x1F1,
    ATA_IO_SECCNT = 0x1F2,
    ATA_IO_LBA0   = 0x1F3,
    ATA_IO_LBA1   = 0x1F4,
    ATA_IO_LBA2   = 0x1F5,
    ATA_IO_DRV    = 0x1F6,
    ATA_IO_CMD    = 0x1F7,
    ATA_IO_STATUS = 0x1F7,

    ATA_ALT_STATUS= 0x3F6,  // read for alt status, write for device control

    ATA_CMD_READ_SECTORS = 0x20,

    ATA_SR_BSY  = 0x80,
    ATA_SR_DRDY = 0x40,
    ATA_SR_DF   = 0x20,
    ATA_SR_DRQ  = 0x08,
    ATA_SR_ERR  = 0x01,
};

// 400ns delay: 4 dummy reads of the alternate status port
static inline void ata_400ns_delay(void) {
    (void)inb_u8(ATA_ALT_STATUS);
    (void)inb_u8(ATA_ALT_STATUS);
    (void)inb_u8(ATA_ALT_STATUS);
    (void)inb_u8(ATA_ALT_STATUS);
}

// Wait for BSY=0 and DRQ=1 (or error). Returns 0 on ready, -1 on error/timeout.
static int ata_wait_drq(void) {
    // Crude bounded spin to avoid infinite loop
    for (int i = 0; i < 1000000; ++i) {
        uint8_t st = inb_u8(ATA_IO_STATUS);
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ)) return 0;
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return -1;
    }
    return -1;
}

// Read `numsectors` 512B sectors starting at LBA `lba` into `buffer`.
// Returns 0 on success, -1 on error.
int ata_lba_read(unsigned int lba, unsigned char *buffer, unsigned int numsectors) {
    if (numsectors == 0) return 0;

    // Select drive (primary master), LBA mode, top 4 bits of LBA
    outb(ATA_IO_DRV, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));

    // Program sector count & LBA bytes
    outb(ATA_IO_SECCNT, (uint8_t)numsectors);
    outb(ATA_IO_LBA0,   (uint8_t)(lba & 0xFF));
    outb(ATA_IO_LBA1,   (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_IO_LBA2,   (uint8_t)((lba >> 16) & 0xFF));

    // Issue READ SECTORS command
    outb(ATA_IO_CMD, ATA_CMD_READ_SECTORS);

    // Read sectors
    for (unsigned int s = 0; s < numsectors; ++s) {
        if (ata_wait_drq() != 0) return -1;

        // 256 words (512 bytes) per sector
        uint16_t *dst = (uint16_t*)buffer;
        for (int i = 0; i < 256; ++i) {
            dst[i] = inw_u16(ATA_IO_DATA);
        }

        buffer += 512;
        ata_400ns_delay();
    }

    return 0;
}
