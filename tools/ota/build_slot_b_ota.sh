#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REL="$ROOT/Release"

HEX430="/home/skydom/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/hex430"

OUT="skm-8.6.x_GRP_slot_b_vlo"
OTA="${OUT}_ota.txt"

cd "$REL"

echo "=== Prepare Slot B makefile ==="

cp makefile makefile.slot_b

sed -i \
  -e '/"\.\/boot_start\.obj"/d' \
  -e '/"\.\/bootloader\.obj"/d' \
  -e 's/lnk_msp430fr5949_slot_a\.cmd/lnk_msp430fr5949_slot_b.cmd/g' \
  -e 's/skm-8\.6\.x_GRP\.out/skm-8.6.x_GRP_slot_b_vlo.out/g' \
  -e 's/skm-8\.6\.x_GRP\.map/skm-8.6.x_GRP_slot_b_vlo.map/g' \
  -e 's/skm-8\.6\.x_GRP_linkInfo\.xml/skm-8.6.x_GRP_slot_b_vlo_linkInfo.xml/g' \
  makefile.slot_b

echo "=== Link Slot B ==="

make -f makefile.slot_b -Onone "${OUT}.out"

echo "=== Create OTA TI-TXT ==="

"$HEX430" \
  --ti_txt \
  --memwidth=8 \
  --romwidth=8 \
  --outfile="$OTA" \
  --exclude=.TI.persistent \
  --exclude=AES256 \
  --exclude=RTC \
  --exclude=PORT4 \
  --exclude=PORT3 \
  --exclude=TIMER3_A1 \
  --exclude=TIMER3_A0 \
  --exclude=PORT2 \
  --exclude=TIMER2_A1 \
  --exclude=TIMER2_A0 \
  --exclude=PORT1 \
  --exclude=TIMER1_A1 \
  --exclude=TIMER1_A0 \
  --exclude=DMA \
  --exclude=USCI_A1 \
  --exclude=TIMER0_A1 \
  --exclude=TIMER0_A0 \
  --exclude=ADC12 \
  --exclude=USCI_B0 \
  --exclude=USCI_A0 \
  --exclude=WDT \
  --exclude=TIMER0_B1 \
  --exclude=TIMER0_B0 \
  --exclude=COMP_E \
  --exclude=UNMI \
  --exclude=SYSNMI \
  --exclude='$fill000' \
  --exclude='$fill001' \
  --exclude='$fill002' \
  "${OUT}.out"

echo "=== Validate OTA image ==="

python3 - "$OTA" <<'PY'
from pathlib import Path
import sys

fn = Path(sys.argv[1])
mem = {}
addr = None

for line in fn.read_text().splitlines():
    line = line.strip()

    if not line:
        continue
    if line.lower() == "q":
        break

    if line.startswith("@"):
        addr = int(line[1:], 16)
        continue

    if addr is None:
        raise SystemExit("ERROR: data before address record")

    for s in line.split():
        mem[addr] = int(s, 16)
        addr += 1

allowed = [
    (0xA7C0, 0xFF7F),
    (0x11FFC, 0x13FF7),
]

bad = [
    a for a in mem
    if not any(lo <= a <= hi for lo, hi in allowed)
]

if bad:
    print("ERROR: OTA contains data outside Slot B")
    for a in bad[:20]:
        print(f"  0x{a:05X}")
    raise SystemExit(1)

header = bytes(mem.get(a, 0xFF) for a in range(0xA7C0, 0xA7E4))

if header[0:4] != bytes([0x4B, 0x53, 0x02, 0x00]):
    raise SystemExit(
        "ERROR: invalid AppHeader magic/version: "
        + header[0:4].hex(" ")
    )

def u32(off):
    return int.from_bytes(header[off:off+4], "little")

names = [
    ("entry",    4),
    ("Port_1",   8),
    ("Port_2",  12),
    ("Port_3",  16),
    ("WDT",     20),
    ("USCI_A0", 24),
    ("USCI_B0", 28),
    ("ADC12",   32),
]

print(f"File       : {fn}")
print(f"Data bytes : {len(mem)}")
print(f"Lowest     : 0x{min(mem):05X}")
print(f"Highest    : 0x{max(mem):05X}")
print()
print("AppHeader:")

for name, off in names:
    value = u32(off)
    print(f"  {name:8s} = 0x{value:05X}")

    if name != "entry" and value >= 0x10000:
        raise SystemExit(
            f"ERROR: ISR {name} is above 0xFFFF"
        )

print()
print("OK: Slot B OTA image is valid.")
PY

echo
echo "=== Build final CRC-protected Slot B image ==="

python3 - "$OTA" <<'PYCRC'
from pathlib import Path
import sys
import zlib

src = Path(sys.argv[1])

payload_file = Path(
    "skm-8.6.x_GRP_slot_b_payload.bin"
)

final_file = Path(
    "skm-8.6.x_GRP_slot_b_vlo_ota_crc.txt"
)

mem = {}
addr = None

# --------------------------------------------------
# Parse TI-TXT produced by hex430
# --------------------------------------------------

for raw in src.read_text().splitlines():
    line = raw.strip()

    if not line:
        continue

    if line.lower() == "q":
        break

    if line.startswith("@"):
        addr = int(line[1:], 16)
        continue

    if addr is None:
        raise SystemExit(
            "ERROR: data before address record"
        )

    for value in line.split():
        if addr in mem:
            raise SystemExit(
                f"ERROR: duplicate address 0x{addr:05X}"
            )

        mem[addr] = int(value, 16)
        addr += 1

# --------------------------------------------------
# Validate AppHeader v2
# --------------------------------------------------

HEADER_START = 0xA7C0
HEADER_END   = 0xA7EB
HEADER_SIZE  = 44

header = bytearray(
    mem.get(a, 0xFF)
    for a in range(HEADER_START, HEADER_END + 1)
)

if len(header) != HEADER_SIZE:
    raise SystemExit("ERROR: bad AppHeader size")

magic = int.from_bytes(
    header[0:2], "little"
)

version = int.from_bytes(
    header[2:4], "little"
)

if magic != 0x534B:
    raise SystemExit(
        f"ERROR: bad AppHeader magic 0x{magic:04X}"
    )

if version != 2:
    raise SystemExit(
        f"ERROR: AppHeader version {version}, expected 2"
    )

# --------------------------------------------------
# Build canonical Slot B payload
#
# B_LOW  = 0xA8C0..0xFF7F
# B_HIGH = 0x11FFC..0x13FF7
#
# Missing bytes are explicitly 0xFF.
# --------------------------------------------------

regions = [
    (0xA8C0,  0xFF7F),
    (0x11FFC, 0x13FF7),
]

payload = bytearray()

for lo, hi in regions:
    for a in range(lo, hi + 1):
        payload.append(mem.get(a, 0xFF))

EXPECTED_SIZE = 30396

if len(payload) != EXPECTED_SIZE:
    raise SystemExit(
        f"ERROR: payload size {len(payload)} "
        f"!= {EXPECTED_SIZE}"
    )

payload_file.write_bytes(payload)

# --------------------------------------------------
# Calculate canonical CRC32
# --------------------------------------------------

crc = zlib.crc32(payload) & 0xFFFFFFFF

# --------------------------------------------------
# Patch image_size + image_crc32 into AppHeader v2
#
# Header offsets:
#
# 0..35  old AppHeader fields
# 36..39 image_size
# 40..43 image_crc32
# --------------------------------------------------

header[36:40] = EXPECTED_SIZE.to_bytes(
    4, "little"
)

header[40:44] = crc.to_bytes(
    4, "little"
)

# --------------------------------------------------
# Build FINAL OTA TI-TXT
#
# Header + complete LOW + complete HIGH.
#
# This is deliberately NOT sparse inside a Slot.
# All unused bytes are explicitly written as 0xFF,
# so bootloader CRC does not depend on an old image.
# --------------------------------------------------

low = bytes(
    mem.get(a, 0xFF)
    for a in range(0xA8C0, 0xFF80)
)

high = bytes(
    mem.get(a, 0xFF)
    for a in range(0x11FFC, 0x13FF8)
)

lines = []

def emit(start, data):
    lines.append(f"@{start:X}")

    for i in range(0, len(data), 16):
        lines.append(
            " ".join(
                f"{b:02X}"
                for b in data[i:i + 16]
            )
        )

emit(0xA7C0, header)
emit(0xA8C0, low)
emit(0x11FFC, high)

lines.append("q")

final_file.write_text(
    "\n".join(lines) + "\n"
)

# --------------------------------------------------
# Re-parse final image and validate exact addresses
# --------------------------------------------------

final_mem = {}
addr = None

for raw in final_file.read_text().splitlines():
    line = raw.strip()

    if not line:
        continue

    if line.lower() == "q":
        break

    if line.startswith("@"):
        addr = int(line[1:], 16)
        continue

    for value in line.split():
        if addr in final_mem:
            raise SystemExit(
                f"ERROR: duplicate final address "
                f"0x{addr:05X}"
            )

        final_mem[addr] = int(value, 16)
        addr += 1

expected_addresses = set(
    range(0xA7C0, 0xA7EC)
)

expected_addresses.update(
    range(0xA8C0, 0xFF80)
)

expected_addresses.update(
    range(0x11FFC, 0x13FF8)
)

actual_addresses = set(final_mem)

missing = sorted(
    expected_addresses - actual_addresses
)

extra = sorted(
    actual_addresses - expected_addresses
)

if missing:
    raise SystemExit(
        f"ERROR: final image missing "
        f"0x{missing[0]:05X}"
    )

if extra:
    raise SystemExit(
        f"ERROR: final image contains forbidden "
        f"0x{extra[0]:05X}"
    )

# --------------------------------------------------
# Verify CRC from final image itself
# --------------------------------------------------

final_header = bytes(
    final_mem[a]
    for a in range(0xA7C0, 0xA7EC)
)

image_size = int.from_bytes(
    final_header[36:40],
    "little"
)

header_crc = int.from_bytes(
    final_header[40:44],
    "little"
)

final_payload = (
    bytes(
        final_mem[a]
        for a in range(0xA8C0, 0xFF80)
    )
    +
    bytes(
        final_mem[a]
        for a in range(0x11FFC, 0x13FF8)
    )
)

final_crc = (
    zlib.crc32(final_payload)
    & 0xFFFFFFFF
)

if image_size != EXPECTED_SIZE:
    raise SystemExit(
        f"ERROR: final header image_size "
        f"{image_size}"
    )

if header_crc != final_crc:
    raise SystemExit(
        f"ERROR: final CRC mismatch: "
        f"header={header_crc:08X}, "
        f"calc={final_crc:08X}"
    )

if len(final_mem) != 30440:
    raise SystemExit(
        f"ERROR: final byte count "
        f"{len(final_mem)} != 30440"
    )

print(f"Payload file : {payload_file}")
print(f"Payload size : {len(payload)}")
print(f"CRC32        : {crc:08X}")
print()
print(f"Final OTA    : {final_file}")
print(f"Final bytes  : {len(final_mem)}")
print(f"Header size  : {image_size}")
print(f"Header CRC32 : {header_crc:08X}")
print(f"Calc CRC32   : {final_crc:08X}")

print()
print("Final AppHeader v2:")

for a in range(0xA7C0, 0xA7EC, 16):
    end = min(a + 16, 0xA7EC)

    print(
        f"0x{a:05X}: "
        + " ".join(
            f"{final_mem[x]:02X}"
            for x in range(a, end)
        )
    )

print()
print(
    "OK: final CRC-protected "
    "Slot B OTA image is valid."
)
PYCRC

echo
echo "=== SHA256 ==="

sha256sum "$OTA"
sha256sum skm-8.6.x_GRP_slot_b_payload.bin
sha256sum skm-8.6.x_GRP_slot_b_vlo_ota_crc.txt

echo
echo "READY FOR OTA:"
echo "$REL/skm-8.6.x_GRP_slot_b_vlo_ota_crc.txt"

