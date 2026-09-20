# SKM-8 OTA A/B Test — Firmware 8.73

Date: 2026-09-19

## Configuration

MCU: MSP430FR5949

Firmware version: 8.73

OTA format: 2

OTA chunk size: 64 bytes

OTA image size: 30440 bytes

Payload size: 30396 bytes

## Slot A

Payload CRC32: A8AE87FF

Transport CRC32: 1EC69603

## Slot B

Payload CRC32: 4A1DEBB1

Transport CRC32: 919D08AE

## Tested OTA sequence

### A -> B

Controller started from Slot A.

Server created OTA campaign with target Slot B.

OTA stream was transferred using 64-byte logical chunks.

The controller wrote each logical chunk to FRAM using physical writes not larger than 16 bytes.

After full image reception:

- transport CRC was verified;
- AppHeader v2 was verified;
- payload CRC was verified;
- pending slot was set to Slot B;
- OTA journal was cleared;
- controller performed software BOR reset.

Bootloader started Slot B as trial firmware.

The controller reported:

as=B

to the server.

### Finalization race discovered

During the first A -> B test, the controller rebooted immediately after successful OTA finalization.

The server did not receive the READY ACK before reboot and remained in:

finalizing

state.

After Slot B started, the server repeated:

of=<CRC>

The new application no longer had an active OTA session and returned OTA status 0x80.

The firmware update itself was successful, but the server incorrectly changed the OTA campaign to error.

### Server-side fix

The server OTA finalization logic was changed.

When the server receives:

active_slot == target_slot

while OTA status is:

finalizing

the OTA campaign is completed immediately:

status = completed

expected_offset = 30440

target_slot = NULL

The active-slot report is processed before generation of the next OTA command.

This prevents repeated `of=` commands after successful reboot.

## B -> A validation

A reverse OTA update from Slot B to Slot A was performed.

Transport CRC32:

1EC69603

The full OTA stream was transferred successfully.

After finalization the controller performed automatic BOR reset.

Bootloader started Slot A.

The controller reported:

as=A

The server completed the campaign without repeating the `of=` command.

Final server state:

active_slot = A

target_slot = NULL

status = completed

expected_offset = 30440

package_crc32 = 1EC69603

## Release builder validation

The release builder creates both Slot A and Slot B images.

The builder validates the firmware version embedded in each `.out` file against `firmware_version.h`.

This prevents creation of a release package where the manifest version differs from the firmware actually linked into the binary.

Verified firmware strings:

Slot A: 8.73

Slot B: 8.73

## Result

Firmware 8.73 successfully passed:

- A -> B OTA update
- B -> A OTA update
- 64-byte OTA chunks
- 16-byte physical FRAM writes
- AppHeader v2 validation
- payload CRC validation
- transport CRC validation
- automatic BOR reset
- active-slot reporting
- server-side OTA finalization
- symmetric A/B operation

Result: PASSED
