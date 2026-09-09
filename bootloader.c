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

    app->entry();

    boot_fail();
}
