#include <msp430.h>
#include <stdint.h>

#include "app_header.h"
#include "boot_meta.h"

#define APP_A_HEADER_ADDR   0x5000u
#define APP_B_HEADER_ADDR   0xA7C0u

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

#pragma CODE_SECTION(boot_get_app, ".boot_text")
#pragma RETAIN(boot_get_app)
static const AppHeader *boot_get_app(void)
{
    const BootMeta *meta =
        (const BootMeta *)(uintptr_t)BOOT_META_ADDR;

    /*
     * Порожня або пошкоджена BOOT_META:
     * завжди використовуємо Slot A.
     */
    if ((meta->magic != BOOT_META_MAGIC) ||
        (meta->format_version != BOOT_META_FORMAT_VERSION))
    {
        return (const AppHeader *)(uintptr_t)APP_A_HEADER_ADDR;
    }

    if (meta->active_slot == BOOT_SLOT_B)
    {
        return (const AppHeader *)(uintptr_t)APP_B_HEADER_ADDR;
    }

    /*
     * Slot A також є fallback для будь-якого
     * невідомого значення active_slot.
     */
    return (const AppHeader *)(uintptr_t)APP_A_HEADER_ADDR;
}

typedef union
{
    app_code_ptr_t code;
    uint16_t word[2];

} BootCodePtr;

#pragma CODE_SECTION(boot_get_isr16, ".boot_text")
#pragma RETAIN(boot_get_isr16)
static uint16_t boot_get_isr16(app_code_ptr_t isr)
{
    BootCodePtr p;

    p.code = isr;

    /*
     * Усі ISR A/B повинні бути нижче 0x10000.
     * Для boot proxy використовується молодше 16 біт.
     */
    if (p.word[1] != 0u)
    {
        boot_fail();
    }

    if (p.word[0] == 0u)
    {
        boot_fail();
    }

    return p.word[0];
}

#pragma CODE_SECTION(boot_set_dispatch, ".boot_text")
#pragma RETAIN(boot_set_dispatch)
static void boot_set_dispatch(const AppHeader *app)
{
    *(volatile uint16_t *)BOOT_DISPATCH_PORT1_ADDR =
        boot_get_isr16(app->isr_port1);

    *(volatile uint16_t *)BOOT_DISPATCH_PORT2_ADDR =
        boot_get_isr16(app->isr_port2);

    *(volatile uint16_t *)BOOT_DISPATCH_PORT3_ADDR =
        boot_get_isr16(app->isr_port3);

    *(volatile uint16_t *)BOOT_DISPATCH_WDT_ADDR =
        boot_get_isr16(app->isr_wdt);

    *(volatile uint16_t *)BOOT_DISPATCH_USCI_A0_ADDR =
        boot_get_isr16(app->isr_usci_a0);

    *(volatile uint16_t *)BOOT_DISPATCH_USCI_B0_ADDR =
        boot_get_isr16(app->isr_usci_b0);

    *(volatile uint16_t *)BOOT_DISPATCH_ADC12_ADDR =
        boot_get_isr16(app->isr_adc12);
}

#pragma CODE_SECTION(boot_reset, ".boot_text")
#pragma RETAIN(boot_reset)
void boot_reset(void)
{
    const AppHeader *app;

    __disable_interrupt();

    app = boot_get_app();

    if (app->magic != APP_HEADER_MAGIC)
    {
        boot_fail();
    }

    if (app->format_version != APP_HEADER_FORMAT_VERSION)
    {
        boot_fail();
    }

    if (app->entry == 0)
    {
        boot_fail();
    }

    boot_set_dispatch(app);
    
    app->entry();

    boot_fail();
}
