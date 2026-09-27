#include <stdint.h>
#include <stddef.h>
#include <intrinsics.h>

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


/* Only an erased record is eligible for initialization. A torn or corrupt
 * record requires explicit recovery; FINISH must never repair it. */
typedef char boot_meta_size_must_be_14[(sizeof(BootMeta) == 14u) ? 1 : -1];

static uint32_t boot_image_crc_region(uint32_t crc, unsigned long address,
                                      unsigned long length)
{
    unsigned char bit;
    while (length-- != 0UL)
    {
        crc ^= (uint8_t)__data20_read_char(address++);
        for (bit = 0u; bit < 8u; bit++)
        {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320UL : 0UL);
        }
    }
    return crc;
}

static uint8_t boot_current_image_valid(void)
{
    uint8_t slot = boot_get_current_slot();
    unsigned long entry = (unsigned long)g_app_header.entry;
    unsigned long low;
    unsigned long high;
    uint32_t crc;

    if ((g_app_header.magic != APP_HEADER_MAGIC) ||
        (g_app_header.format_version != APP_HEADER_FORMAT_VERSION_V2) ||
        (g_app_header.image_size != APP_IMAGE_PAYLOAD_SIZE))
    {
        return 0u;
    }
    if (slot == BOOT_SLOT_A)
    {
        low = 0x5100UL;
        high = 0x10000UL;
    }
    else if (slot == BOOT_SLOT_B)
    {
        low = 0xA8C0UL;
        high = 0x11FFCUL;
    }
    else return 0u;

    if ((entry & 1UL) != 0UL ||
        !((entry >= low && entry < low + 0x56C0UL) ||
          (entry >= high && entry < high + 0x1FFCUL))) return 0u;

    /* Payload CRC excludes the header, so validate vector addresses too.
     * Boot dispatch requires every ISR to be in this slot's LOW region. */
#define BOOT_ISR_VALID(field) \
    (((unsigned long)g_app_header.field & 1UL) == 0UL && \
     (unsigned long)g_app_header.field >= low && \
     (unsigned long)g_app_header.field < low + 0x56C0UL)
    if (!BOOT_ISR_VALID(isr_port1) || !BOOT_ISR_VALID(isr_port2) ||
        !BOOT_ISR_VALID(isr_port3) || !BOOT_ISR_VALID(isr_wdt) ||
        !BOOT_ISR_VALID(isr_usci_a0) || !BOOT_ISR_VALID(isr_usci_b0) ||
        !BOOT_ISR_VALID(isr_adc12)) return 0u;
#undef BOOT_ISR_VALID

    crc = boot_image_crc_region(0xFFFFFFFFUL, low, 0x56C0UL);
    crc = boot_image_crc_region(crc, high, 0x1FFCUL) ^ 0xFFFFFFFFUL;
    return crc == g_app_header.image_crc32;
}

static uint8_t boot_meta_consistent(const volatile BootMeta *meta)
{
    uint8_t current = boot_get_current_slot();
    return current != BOOT_SLOT_NONE &&
        meta->magic == BOOT_META_MAGIC &&
        meta->format_version == BOOT_META_FORMAT_VERSION &&
        meta->active_slot == current && meta->flags == 0u &&
        meta->reserved0 == 0u && meta->reserved1 == 0u &&
        (meta->pending_slot == BOOT_SLOT_NONE ||
         meta->pending_slot == (uint8_t)(current ^ 1u));
}

static uint8_t boot_write_byte(unsigned int offset, uint8_t value)
{
    if (offset >= sizeof(BootMeta)) return 0u;
    __data20_write_char((unsigned long)BOOT_META_ADDR + offset, value);
    return (uint8_t)__data20_read_char(
        (unsigned long)BOOT_META_ADDR + offset) == value;
}

uint8_t boot_prepare_ota(void)
{
    volatile BootMeta *meta =
        (volatile BootMeta *)(uintptr_t)BOOT_META_ADDR;
    BootMeta initial;
    const uint8_t *bytes;
    unsigned int i;

    if (!boot_current_image_valid())
    {
        return 0u;
    }
    if (boot_meta_consistent(meta))
    {
        /* Do not overwrite a pending update or a trial boot. */
        return meta->pending_slot == BOOT_SLOT_NONE;
    }
    for (i = 0u; i < sizeof(BootMeta); i++)
    {
        if ((uint8_t)__data20_read_char(
                (unsigned long)BOOT_META_ADDR + i) != 0xFFu)
        {
            return 0u;
        }
    }

    initial.magic = BOOT_META_MAGIC;
    initial.format_version = BOOT_META_FORMAT_VERSION;
    initial.active_slot = boot_get_current_slot();
    initial.pending_slot = BOOT_SLOT_NONE;
    initial.flags = 0u;
    initial.sequence = 0u;
    initial.reserved0 = 0u;
    initial.reserved1 = 0u;
    bytes = (const uint8_t *)&initial;

    /* Invalidate first; publish magic only after every other byte verifies.
     * Interrupted initialization remains invalid and fails closed. */
    if (!boot_write_byte(0u, 0u) || !boot_write_byte(1u, 0u))
    {
        return 0u;
    }
    for (i = 2u; i < sizeof(BootMeta); i++)
    {
        if (!boot_write_byte(i, bytes[i])) return 0u;
    }
    if (!boot_write_byte(0u, bytes[0]) ||
        !boot_write_byte(1u, bytes[1])) return 0u;
    for (i = 0u; i < sizeof(BootMeta); i++)
    {
        if ((uint8_t)__data20_read_char(
                (unsigned long)BOOT_META_ADDR + i) != bytes[i]) return 0u;
    }
    return boot_meta_consistent(meta) &&
        meta->pending_slot == BOOT_SLOT_NONE;
}

uint8_t boot_set_pending(uint8_t slot)
{
    volatile BootMeta *meta =
        (volatile BootMeta *)(uintptr_t)BOOT_META_ADDR;
    uint16_t sequence;
    const uint8_t *bytes;

    /* Validation only: never initialize or repair metadata at FINISH. */
    if (!boot_current_image_valid() || !boot_meta_consistent(meta) ||
        (slot != BOOT_SLOT_A && slot != BOOT_SLOT_B) ||
        slot == boot_get_current_slot()) return 0u;
    if (meta->pending_slot == slot) return 1u; /* No repeated writes. */
    if (meta->pending_slot != BOOT_SLOT_NONE) return 0u;

    sequence = (uint16_t)(meta->sequence + 1u);
    bytes = (const uint8_t *)&sequence;
    if (!boot_write_byte(offsetof(BootMeta, sequence), bytes[0]) ||
        !boot_write_byte(offsetof(BootMeta, sequence) + 1u, bytes[1]))
    {
        return 0u;
    }
    /* pending_slot is the commit marker, written last and read back. */
    if (!boot_write_byte(offsetof(BootMeta, pending_slot), slot)) return 0u;
    return boot_meta_consistent(meta) && meta->pending_slot == slot &&
        meta->sequence == sequence;
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
