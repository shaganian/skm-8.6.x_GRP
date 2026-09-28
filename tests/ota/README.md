# Offline OTA regression

Set `OTA_FIXTURE_DIR` to an existing audited SKM8-8.74 or SKM8-8.75 release directory.
Run with Python 3 and a host C compiler (no TI build, device or network):

```sh
python3 -B tests/ota/run_offline.py \
  --image "$OTA_FIXTURE_DIR"/slot_b.ota \
  --commands "$OTA_FIXTURE_DIR"/slot_b_commands.txt
```

The existing audited release is read only and SHA256-checked; it is not rebuilt
or copied into this repository. The runner creates and removes host compilation
artifacts in a temporary directory. It does not connect to a controller or DB.

The harness compiles the actual BootMeta functions, OTA and ordinary response parsers, CRC, writer,
journal, START/FINISH code and modem reset gate. The runner adapts MSP430
unsigned int/long and code-pointer widths to 16/32 bits, relocates physical
pointers into mock FRAM, and mocks MPU/interrupt/reset intrinsics. Compile-time
assertions check header, BootMeta and journal sizes. There is no duplicated OTA
implementation. The modem integration is checked structurally; the complete
SIM800/UART driver and TI-generated machine code are not executed on the host.

Contract: only an entirely erased (all 0xFF) 14-byte BootMeta can be initialized,
using a recognized current header address, v2 header, payload size and entry
inside the same slot, LOW-region ISR vectors, and a matching current payload CRC. Magic is published last after read-back. Torn, zeroed,
corrupt, mismatched, pending or trial records fail the START/resume preflight.
FINISH never initializes metadata. Valid stable metadata is unchanged.

READY remains in RAM after FINISH, including matching repeated FINISH commands.
The modem snapshots the status before constructing each request. BOR is gated
on that snapshot being READY, SEND OK, and a successfully parsed response while
the current state remains READY. A response that first makes the state READY
cannot trigger BOR. Physical power loss is outside this software ordering
guarantee and continues to use the bootloader/journal recovery paths.

Malformed or unrelated OTA responses preserve committed READY but return a
rejection result that cannot authorize BOR. Metadata writes are range-checked.

READY response regression covers valid `ft=`, absent/invalid `ft=`, repeated
valid FINISH, rejected FINISH (return 2), and the first FINISH while the sent
status is CHUNK_OK. The actual parser return value feeds the production reset
gate; the reset intrinsic is counted by the host mock.
