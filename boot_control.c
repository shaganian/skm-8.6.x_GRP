#include <stdint.h>

#include "app_header.h"
#include "boot_meta.h"
#include "boot_control.h"

#define APP_A_HEADER_ADDR   0x5000u
#define APP_B_HEADER_ADDR   0xA7C0u


uint8_t boot_get_current_slot(void)
{
    uintptr_t header_addr = (uintptr_t)&g_app_header;

    if (header_addr == APP_A_HEADER_ADDR)
    {
        return BOOT_SLOT_A;
    }

    if (header_addr == APP_B_HEADER_ADDR)
    {
        return BOOT_SLOT_B;
    }

    return BOOT_SLOT_NONE;
}


uint8_t boot_set_pending(uint8_t slot)
{
    volatile BootMeta *meta =
        (volatile BootMeta *)(uintptr_t)BOOT_META_ADDR;

    uint8_t current_slot;

    if ((meta->magic != BOOT_META_MAGIC) ||
        (meta->format_version != BOOT_META_FORMAT_VERSION))
    {
        return 0u;
    }

    if ((slot != BOOT_SLOT_A) &&
        (slot != BOOT_SLOT_B))
    {
        return 0u;
    }

    current_slot = boot_get_current_slot();

    if (current_slot == BOOT_SLOT_NONE)
    {
        return 0u;
    }

    /*
     * OTA дозволено готувати тільки в неактивний Slot.
     */
    if (slot == current_slot)
    {
        return 0u;
    }

    /*
     * Стан BootMeta повинен відповідати firmware,
     * яка реально зараз виконується.
     */
    if (meta->active_slot != current_slot)
    {
        return 0u;
    }

    /*
     * Новий pending ще не є trial.
     *
     * pending_slot записуємо ОСТАННІМ:
     * це commit-marker готового OTA образу.
     *
     * Якщо живлення зникне раніше, bootloader
     * не побачить нового pending Slot.
     */
    meta->flags &= (uint16_t)~BOOT_FLAG_TRIAL_STARTED;
    meta->sequence++;
    meta->pending_slot = slot;

    return 1u;
}


uint8_t boot_confirm(void)
{
    volatile BootMeta *meta =
        (volatile BootMeta *)(uintptr_t)BOOT_META_ADDR;

    uint8_t current_slot;

    if ((meta->magic != BOOT_META_MAGIC) ||
        (meta->format_version != BOOT_META_FORMAT_VERSION))
    {
        return 0u;
    }

    current_slot = boot_get_current_slot();

    if (current_slot == BOOT_SLOT_NONE)
    {
        return 0u;
    }

    /*
     * Підтверджувати можна тільки firmware,
     * яка зараз записана як pending.
     */
    if (meta->pending_slot != current_slot)
    {
        return 0u;
    }

    /*
     * І тільки після того, як bootloader уже
     * позначив її як trial boot.
     */
    if ((meta->flags & BOOT_FLAG_TRIAL_STARTED) == 0u)
    {
        return 0u;
    }

    /*
     * Commit нового Slot.
     */
    meta->active_slot = current_slot;
    meta->pending_slot = BOOT_SLOT_NONE;
    meta->flags &= (uint16_t)~BOOT_FLAG_TRIAL_STARTED;
    meta->sequence++;

    return 1u;
}
