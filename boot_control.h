#ifndef BOOT_CONTROL_H
#define BOOT_CONTROL_H

#include <stdint.h>

/*
 * Підтверджує успішний trial boot поточного Slot.
 *
 * Повертає:
 *   1 - firmware була pending і успішно підтверджена
 *   0 - підтвердження не потрібне або BootMeta некоректна
 */
uint8_t boot_confirm(void);

/*
 * Повертає Slot, у якому зараз виконується application:
 * BOOT_SLOT_A, BOOT_SLOT_B або BOOT_SLOT_NONE.
 */
uint8_t boot_get_current_slot(void);

/*
 * Позначає неактивний Slot як pending.
 *
 * Повертає:
 *   1 - pending успішно встановлено
 *   0 - помилка BootMeta, невірний Slot або спроба
 *       встановити поточний активний Slot як pending
 */
uint8_t boot_set_pending(uint8_t slot);

#endif
