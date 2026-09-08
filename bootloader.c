#include <msp430.h>
#include <stdint.h>

#define APP_A_HEADER_ADDR   0x5000u
#define APP_HEADER_MAGIC    0x534Bu

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

#pragma CODE_SECTION(boot_fail, ".boot_text")
#pragma RETAIN(boot_fail)
static void boot_fail(void)
{
    __disable_interrupt();

    for (;;)
    {
        __no_operation();
    }
}

#pragma CODE_SECTION(boot_reset, ".boot_text")
#pragma RETAIN(boot_reset)
void boot_reset(void)
{
    const AppHeader *app =
        (const AppHeader *)(uintptr_t)APP_A_HEADER_ADDR;

    __disable_interrupt();

    if (app->magic != APP_HEADER_MAGIC)
    {
        boot_fail();
    }

    if (app->entry == 0)
    {
        boot_fail();
    }

    app->entry();

    boot_fail();
}
