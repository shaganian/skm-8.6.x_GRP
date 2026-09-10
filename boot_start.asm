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
    ;.global Port_1

    .sect ".boot_proxy"
    .retain

boot_port1_proxy:
    ;BR      #Port_1
    BR      &0x4720


    ; MSP430FR5949 PORT1_VECTOR = .int39 = 0xFFDE

    .sect ".int39"
    .retain

    .word boot_port1_proxy
    
    ; .word Port_1

    .global boot_port2_proxy
    ;.global Port_2

    .sect ".boot_proxy"
    .retain

boot_port2_proxy:
    BR      &0x4722


    .global boot_port3_proxy
    ;.global Port_3

    .sect ".boot_proxy"
    .retain

boot_port3_proxy:
    BR      &0x4724
    
    ; MSP430FR5949 PORT2_VECTOR = .int36 = 0xFFD8

    .sect ".int36"
    .retain
    .word boot_port2_proxy

    ; MSP430FR5949 PORT3_VECTOR = .int33 = 0xFFD2

    .sect ".int33"
    .retain
    .word boot_port3_proxy


    .global boot_wdt_proxy
    ;.global WDT_ISR

    .sect ".boot_proxy"
    .retain

boot_wdt_proxy:
    BR      &0x4726

    ; MSP430FR5949 WDT_VECTOR = .int49 = 0xFFF2

    .sect ".int49"
    .retain
    .word boot_wdt_proxy
    

    .global boot_usci_a0_proxy
    ;.global USCI_A0_ISR

    .sect ".boot_proxy"
    .retain

boot_usci_a0_proxy:
    BR      &0x4728

    ; MSP430FR5949 USCI_A0_VECTOR = .int48 = 0xFFF0

    .sect ".int48"
    .retain
    .word boot_usci_a0_proxy
    

    .global boot_usci_b0_proxy
    ;.global USCI_B0_ISR

    .sect ".boot_proxy"
    .retain

boot_usci_b0_proxy:
    BR      &0x472A

    ; MSP430FR5949 USCI_B0_VECTOR = .int47 = 0xFFEE
    .sect ".int47"
    .retain
    .word boot_usci_b0_proxy
    

    .global boot_adc12_proxy
    ;.global ADC12ISR

    .sect ".boot_proxy"
    .retain

boot_adc12_proxy:
    BR      &0x472C

    ; MSP430FR5949 ADC12_VECTOR = .int46 = 0xFFEC
    .sect ".int46"
    .retain
    .word boot_adc12_proxy
        