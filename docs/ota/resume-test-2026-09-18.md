# OTA Resume After Power Loss Test — 2026-09-18

MCU: MSP430FR5949

## Purpose

Verify that an interrupted A/B OTA update survives complete power loss
and resumes from the last committed offset instead of restarting from zero.

## OTA journal layout

Reserved FRAM:

    0x46C0-0x46DF  Record 0
    0x46E0-0x46FF  Record 1

Memory layout:

    0x4400-0x46BF  SHARED_FRAM
    0x46C0-0x46FF  OTA_JOURNAL
    0x4700-0x47FF  BOOT_META

Two alternating CRC-protected journal records are used.

## Test image

Target slot: B

OTA image size:

    30440 bytes

Payload CRC32:

    5DA4128D

Transport CRC32:

    D378DAF7

OTA command stream:

    1905 commands
    1903 data chunks
    16 bytes per full chunk

## Initial boot state

Before OTA:

    active_slot  = A
    pending_slot = NONE
    flags        = 0
    sequence     = 9

## Interrupted transfer

OTA A -> B was started normally.

Power was removed after the server had confirmed:

    expected_offset = 1200
    hexadecimal     = 0x04B0

Server state:

    status          = transferring
    expected_offset = 1200
    package_crc32   = D378DAF7

## Journal after power loss

Record 0:

    magic          = 0x4F4A
    version        = 1
    sequence       = 75
    target_slot    = B
    flags          = ACTIVE
    image_size     = 30440
    expected_crc32 = D378DAF7
    received       = 1184
    running_crc32  = BC4D7D09
    record_crc32   = 8662760F

Record 1:

    magic          = 0x4F4A
    version        = 1
    sequence       = 76
    target_slot    = B
    flags          = ACTIVE
    image_size     = 30440
    expected_crc32 = D378DAF7
    received       = 1200
    running_crc32  = C292E333
    record_crc32   = B5121E04

Newest valid record therefore contained:

    received = 1200

This exactly matched the server expected_offset.

## Resume after power restoration

After power was restored the controller reported:

    ack=02000004B0

The server resent the chunk at offset 1200 and transfer continued:

    ack=02000004C0
    ack=02000004D0
    ack=02000004E0
    ...

The OTA session did not restart from offset zero.

## Successful completion

Final transfer:

    ack=02000076E8 command=of=D378DAF7;
    ack=03000076E8 command=NULL

0x76E8 = 30440 bytes.

Therefore the complete OTA image was received and validated.

## Journal cleanup

After successful OTA finish both journal records had their magic
fields cleared:

    Record 0 magic = 0x0000
    Record 1 magic = 0x0000

The remaining record bytes were intentionally left unchanged.

## BootMeta after OTA finish

Before reboot:

    active_slot  = A
    pending_slot = B
    flags        = 0
    sequence     = 10

Raw BootMeta:

    4D 42 01 00 00 01 00 00 0A 00 00 00 00 00

## Final reboot and confirmation

After reboot the new Slot B successfully started and called boot_confirm().

Final BootMeta:

    active_slot  = B
    pending_slot = NONE
    flags        = 0
    sequence     = 12

Raw BootMeta:

    4D 42 01 00 01 FF 00 00 0C 00 00 00 00 00

## Result

PASS.

The complete sequence was successfully verified:

    Slot A active
        ->
    OTA to Slot B
        ->
    power loss at offset 1200
        ->
    restore OTA journal
        ->
    resume from offset 1200
        ->
    complete 30440-byte image
        ->
    validate image
        ->
    clear OTA journal
        ->
    pending Slot B
        ->
    reboot
        ->
    trial Slot B
        ->
    boot_confirm()
        ->
    Slot B active

OTA A/B now has experimentally verified:

- successful A/B update
- trial confirmation
- automatic rollback of an unconfirmed trial
- duplicate chunk handling
- resume after complete power loss
- CRC-protected dual-record FRAM journal
- journal cleanup after successful update
