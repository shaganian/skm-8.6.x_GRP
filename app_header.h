#ifndef APP_HEADER_H_
#define APP_HEADER_H_

#include <stdint.h>

#define APP_HEADER_MAGIC          0x534Bu   /* "SK" */
#define APP_HEADER_FORMAT_VERSION_V1 1u
#define APP_HEADER_FORMAT_VERSION_V2 2u
#define APP_HEADER_FORMAT_VERSION    APP_HEADER_FORMAT_VERSION_V2
#define APP_IMAGE_PAYLOAD_SIZE   30396UL

typedef void (*app_code_ptr_t)(void);

typedef struct
{
    uint16_t magic;
    uint16_t format_version;

    app_code_ptr_t entry;

    app_code_ptr_t isr_port1;
    app_code_ptr_t isr_port2;
    app_code_ptr_t isr_port3;

    app_code_ptr_t isr_wdt;

    app_code_ptr_t isr_usci_a0;
    app_code_ptr_t isr_usci_b0;

    app_code_ptr_t isr_adc12;

    uint32_t image_size;
    uint32_t image_crc32;

} AppHeader;

extern const AppHeader g_app_header;

#endif
