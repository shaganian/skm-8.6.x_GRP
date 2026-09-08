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


    ; ------------------------------------------------------------
    ; PORT1 interrupt proxy
    ; Physical PORT1 vector belongs to bootloader.
    ; ------------------------------------------------------------

    .global boot_port1_proxy
    .global Port_1

    .sect ".boot_proxy"
    .retain

boot_port1_proxy:
    BR      #Port_1


    ; MSP430FR5949 PORT1_VECTOR = .int39 = 0xFFDE

    .sect ".int39"
    .retain

    .word boot_port1_proxy
    
    ; .word Port_1