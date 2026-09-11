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

#endif
