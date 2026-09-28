#ifndef OTA_CRC32_H
#define OTA_CRC32_H

/* Physical address-order bytes; no implicit seed or final XOR. */
unsigned long ota_crc32_region20(unsigned long crc, unsigned long address,
                                unsigned long size);

#endif
