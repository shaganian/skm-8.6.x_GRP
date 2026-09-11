#include <msp430.h>
#include <stdint.h>
#include <intrinsics.h>

#include "app_header.h"
#include "boot_meta.h"

#define APP_A_HEADER_ADDR   0x5000u
#define APP_B_HEADER_ADDR   0xA7C0u

#define APP_A_LOW_ADDR      0x05100UL
#define APP_A_HIGH_ADDR     0x10000UL

#define APP_B_LOW_ADDR      0x0A8C0UL
#define APP_B_HIGH_ADDR     0x11FFCUL

#define APP_LOW_SIZE        0x56C0UL
#define APP_HIGH_SIZE       0x1FFCUL

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

#pragma CODE_SECTION(boot_crc32_region, ".boot_text")
#pragma RETAIN(boot_crc32_region)
static uint32_t boot_crc32_region(uint32_t crc,
                                  uint32_t address,
                                  uint32_t length)
{
    uint32_t i;
    uint8_t bit;
    uint8_t data;

    for (i = 0; i < length; i++)
    {
        data = __data20_read_char((unsigned long)address);
        address++;

        crc ^= (uint32_t)data;

        for (bit = 0; bit < 8u; bit++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}


#pragma CODE_SECTION(boot_crc32_slot, ".boot_text")
#pragma RETAIN(boot_crc32_slot)
static uint32_t boot_crc32_slot(uint8_t slot)
{
    uint32_t crc = 0xFFFFFFFFUL;

    if (slot == BOOT_SLOT_A)
    {
        crc = boot_crc32_region(
            crc,
            APP_A_LOW_ADDR,
            APP_LOW_SIZE);

        crc = boot_crc32_region(
            crc,
            APP_A_HIGH_ADDR,
            APP_HIGH_SIZE);
    }
    else if (slot == BOOT_SLOT_B)
    {
        crc = boot_crc32_region(
            crc,
            APP_B_LOW_ADDR,
            APP_LOW_SIZE);

        crc = boot_crc32_region(
            crc,
            APP_B_HIGH_ADDR,
            APP_HIGH_SIZE);
    }
    else
    {
        return 0u;
    }

    return crc ^ 0xFFFFFFFFUL;
}

#pragma CODE_SECTION(boot_validate_v2, ".boot_text")
#pragma RETAIN(boot_validate_v2)
static uint8_t boot_validate_v2(const AppHeader *app,
                                uint8_t slot)
{
    uint32_t crc;

    if (app->magic != APP_HEADER_MAGIC)
    {
        return 0u;
    }

    if (app->format_version != APP_HEADER_FORMAT_VERSION_V2)
    {
        return 0u;
    }

    if (app->entry == 0)
    {
        return 0u;
    }

    if (app->image_size != APP_IMAGE_PAYLOAD_SIZE)
    {
        return 0u;
    }

    crc = boot_crc32_slot(slot);

    if (crc != app->image_crc32)
    {
        return 0u;
    }

    return 1u;
}

#pragma CODE_SECTION(boot_get_app, ".boot_text")
#pragma RETAIN(boot_get_app)
static const AppHeader *boot_get_app(void)
{
    volatile BootMeta *meta =
        (volatile BootMeta *)(uintptr_t)BOOT_META_ADDR;

    const AppHeader *pending_app;
    uint8_t pending;

    /*
     * Порожня або пошкоджена BOOT_META:
     * завжди використовуємо Slot A.
     */
    if ((meta->magic != BOOT_META_MAGIC) ||
        (meta->format_version != BOOT_META_FORMAT_VERSION))
    {
        return (const AppHeader *)(uintptr_t)APP_A_HEADER_ADDR;
    }

    /*
     * Є кандидат на trial boot.
     */
    if ((meta->pending_slot == BOOT_SLOT_A) ||
        (meta->pending_slot == BOOT_SLOT_B))
    {
        /*
         * pending == active:
         * службовий стан уже не потрібний.
         */
        if (meta->pending_slot == meta->active_slot)
        {
            meta->pending_slot = BOOT_SLOT_NONE;
            meta->flags &= (uint16_t)~BOOT_FLAG_TRIAL_STARTED;
            meta->sequence++;
        }
        else
        {
            pending = meta->pending_slot;

            if (pending == BOOT_SLOT_B)
            {
                pending_app =
                    (const AppHeader *)(uintptr_t)APP_B_HEADER_ADDR;
            }
            else
            {
                pending_app =
                    (const AppHeader *)(uintptr_t)APP_A_HEADER_ADDR;
            }

            /*
             * Перед ПЕРШИМ trial boot обов'язково
             * перевіряємо AppHeader v2 та CRC образу.
             */
            if ((meta->flags & BOOT_FLAG_TRIAL_STARTED) == 0u)
            {
                if (!boot_validate_v2(pending_app, pending))
                {
                    /*
                     * Пошкоджений/неповний OTA образ.
                     * Не запускаємо його взагалі.
                     */
                    meta->pending_slot = BOOT_SLOT_NONE;
                    meta->flags &=
                        (uint16_t)~BOOT_FLAG_TRIAL_STARTED;
                    meta->sequence++;
                }
                else
                {
                    /*
                     * CRC правильний.
                     * Позначаємо trial ДО переходу.
                     */
                    meta->flags |= BOOT_FLAG_TRIAL_STARTED;
                    meta->sequence++;

                    return pending_app;
                }
            }
            else
            {
                /*
                 * Trial уже був, але firmware
                 * не виконала boot_confirm().
                 *
                 * Rollback на active_slot.
                 */
                meta->pending_slot = BOOT_SLOT_NONE;
                meta->flags &=
                    (uint16_t)~BOOT_FLAG_TRIAL_STARTED;
                meta->sequence++;
            }
        }
    }

    /*
     * Штатний запуск активної firmware.
     */
    if (meta->active_slot == BOOT_SLOT_B)
    {
        return (const AppHeader *)(uintptr_t)APP_B_HEADER_ADDR;
    }

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

    if ((app->format_version != APP_HEADER_FORMAT_VERSION_V1) &&
        (app->format_version != APP_HEADER_FORMAT_VERSION_V2))
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
