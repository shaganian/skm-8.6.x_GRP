    .cdecls C,LIST,"msp430.h"

    .global boot_start
    .global boot_reset
    .global __STACK_END

    .sect ".boot_start"
    .retain
    .retainrefs

boot_start:
    ; Після апаратного RESET спочатку встановлюємо стек
    MOV.W   #__STACK_END, SP

    ; Зупиняємо watchdog до запуску C runtime/application
    MOV.W   #WDTPW+WDTHOLD, &WDTCTL

    ; Переходимо в C-частину bootloader.
    ; boot_reset() не повинна повертатися.
    BR      #boot_reset

    .global boot_reset_vector

    .sect ".boot_reset_vector"
    .retain

boot_reset_vector:
    .word boot_start
