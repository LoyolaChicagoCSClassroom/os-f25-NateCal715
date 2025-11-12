
#ifndef __IDE_H__
#define __IDE_H__

int ata_lba_read(unsigned int lba, unsigned char *buffer, unsigned int numsectors);

void sector_read(unsigned int lba, void *buffer);
void driver_init(const char *disk_image_path);

#endif
