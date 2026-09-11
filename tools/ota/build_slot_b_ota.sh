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

if header[0:4] != bytes([0x4B, 0x53, 0x01, 0x00]):
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
sha256sum "$OTA"

echo
echo "READY:"
echo "$REL/$OTA"
