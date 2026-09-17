# A/B OTA rollback test — 2026-09-17

MCU: MSP430FR5949

## Initial state

BootMeta:

    4D 42 01 00 01 FF 00 00 03 00 00 00 00 00

- active_slot = B
- pending_slot = NONE
- flags = 0
- sequence = 3

## Pending Slot A

BootMeta:

    4D 42 01 00 01 00 00 00 04 00 00 00 00 00

- active_slot = B
- pending_slot = A
- flags = 0
- sequence = 4

## First reboot — trial Slot A

BootMeta:

    4D 42 01 00 01 00 01 00 05 00 00 00 00 00

- active_slot = B
- pending_slot = A
- flags = BOOT_FLAG_TRIAL_STARTED
- sequence = 5

The test Slot A intentionally did not call boot_confirm().

## Second reboot — rollback

BootMeta:

    4D 42 01 00 01 FF 00 00 06 00 00 00 00 00

- active_slot = B
- pending_slot = NONE
- flags = 0
- sequence = 6

## Result

Rollback from an unconfirmed trial Slot A to the previously active
Slot B was successfully verified.

Production source was restored with boot_confirm() enabled.
