#ifndef APP_HEADER_H_
#define APP_HEADER_H_

#include <stdint.h>

#define APP_HEADER_MAGIC          0x534Bu   /* "SK" */
#define APP_HEADER_FORMAT_VERSION 1u

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

} AppHeader;

extern const AppHeader g_app_header;

#endif
