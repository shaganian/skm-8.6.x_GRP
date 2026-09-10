
#ifndef BOOT_META_H_
#define BOOT_META_H_

#include <stdint.h>

#define BOOT_META_ADDR            0x4700u
#define BOOT_META_MAGIC           0x424Du   /* "BM" */
#define BOOT_META_FORMAT_VERSION  1u

#define BOOT_SLOT_A               0u
#define BOOT_SLOT_B               1u
#define BOOT_SLOT_NONE            0xFFu

#define BOOT_DISPATCH_ADDR          0x4720u

#define BOOT_DISPATCH_PORT1_ADDR    0x4720u
#define BOOT_DISPATCH_PORT2_ADDR    0x4722u
#define BOOT_DISPATCH_PORT3_ADDR    0x4724u
#define BOOT_DISPATCH_WDT_ADDR      0x4726u
#define BOOT_DISPATCH_USCI_A0_ADDR  0x4728u
#define BOOT_DISPATCH_USCI_B0_ADDR  0x472Au
#define BOOT_DISPATCH_ADC12_ADDR    0x472Cu

typedef struct
{
    uint16_t magic;
    uint16_t format_version;

    uint8_t active_slot;
    uint8_t pending_slot;

    uint16_t flags;
    uint16_t sequence;

    uint16_t reserved0;
    uint16_t reserved1;

} BootMeta;

#endif
