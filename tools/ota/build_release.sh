#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REL="$ROOT/Release"
TOOLS="$ROOT/tools/ota"

HEX430="/home/skydom/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/hex430"

PROJECT="skm-8.7.x_GRP"

VERSION="$(
python3 - "$ROOT/firmware_version.h" <<'PY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text()

m = re.search(
    r'#define\s+FW_VERSION_STRING\s+"([^"]+)"',
    text
)

if not m:
    raise SystemExit("ERROR: FW_VERSION_STRING not found")

print(m.group(1))
PY
)"

DISPLAY="$(
python3 - "$ROOT/firmware_version.h" <<'PYDISPLAY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text()

m = re.search(
    r'#define\s+FW_VERSION_DISPLAY\s+"([^"]+)"',
    text
)

if not m:
    raise SystemExit("ERROR: FW_VERSION_DISPLAY not found")

print(m.group(1))
PYDISPLAY
)"

verify_out_version()
{
    local out="$1"
    local label="$2"

    if ! strings -a "$out" | grep -Fx "$DISPLAY" >/dev/null; then
        echo "ERROR: ${label} does not contain expected firmware version"
        echo "Expected display string: [$DISPLAY]"
        echo "Output file: $out"
        exit 1
    fi

    echo "${label} version verified: $VERSION"
}

DIST="$ROOT/dist/SKM8-${VERSION}"

echo "============================================================"
echo " SKM-8 Release Builder"
echo "============================================================"
echo "Project : $PROJECT"
echo "Version : $VERSION"
echo "Output  : $DIST"
echo

rm -rf "$DIST"
mkdir -p "$DIST"

# ============================================================
# SLOT A
# ============================================================

echo "=== [1/8] Build Slot A ==="

cd "$REL"

make -f makefile clean
make -f makefile all -j4

test -f "${PROJECT}.out"
verify_out_version "${PROJECT}.out" "Slot A"

echo
echo "=== [2/8] Create Slot A sparse TI-TXT ==="

A_RAW="${PROJECT}_slot_a_release_raw.txt"

"$HEX430" \
  --ti_txt \
  --memwidth=8 \
  --romwidth=8 \
  --outfile="$A_RAW" \
  --exclude=.TI.persistent \
  --exclude=.boot_text \
  --exclude=.boot_proxy \
  --exclude=.boot_start \
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
  --exclude=.boot_reset_vector \
  --exclude='$fill000' \
  --exclude='$fill001' \
  --exclude='$fill002' \
  "${PROJECT}.out"

echo
echo "=== [3/8] Build CRC-protected Slot A ==="

python3 - "$A_RAW" "$DIST" "$VERSION" <<'PY'
from pathlib import Path
import hashlib
import json
import sys
import zlib

src = Path(sys.argv[1])
dist = Path(sys.argv[2])
version = sys.argv[3]

HEADER_START = 0x5000
HEADER_SIZE = 44

LOW_START = 0x5100
LOW_SIZE = 0x56C0

HIGH_START = 0x10000
HIGH_SIZE = 0x1FFC

PAYLOAD_SIZE = LOW_SIZE + HIGH_SIZE
OTA_SIZE = HEADER_SIZE + PAYLOAD_SIZE

mem = {}
addr = None

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
        raise SystemExit("ERROR: TI-TXT data before address")

    for token in line.split():
        if addr in mem:
            raise SystemExit(
                f"ERROR: duplicate address 0x{addr:05X}"
            )
        mem[addr] = int(token, 16)
        addr += 1


def region(start, size):
    return bytearray(
        mem.get(a, 0xFF)
        for a in range(start, start + size)
    )


header = region(HEADER_START, HEADER_SIZE)
low = region(LOW_START, LOW_SIZE)
high = region(HIGH_START, HIGH_SIZE)

if header[0:4] != bytes.fromhex("4B530200"):
    raise SystemExit(
        "ERROR: invalid Slot A AppHeader"
    )

payload = bytes(low + high)

if len(payload) != PAYLOAD_SIZE:
    raise SystemExit(
        f"ERROR: Slot A payload size {len(payload)}"
    )

payload_crc = zlib.crc32(payload) & 0xFFFFFFFF

header[36:40] = PAYLOAD_SIZE.to_bytes(4, "little")
header[40:44] = payload_crc.to_bytes(4, "little")

image = bytes(header + low + high)

if len(image) != OTA_SIZE:
    raise SystemExit(
        f"ERROR: Slot A OTA size {len(image)}"
    )

ota_bin = dist / "slot_a.ota"
flash_txt = dist / "slot_a_flasher.txt"

ota_bin.write_bytes(image)

regions = [
    (HEADER_START, bytes(header)),
    (LOW_START, bytes(low)),
    (HIGH_START, bytes(high)),
]

lines = []

for start, data in regions:
    lines.append(f"@{start:X}")

    for i in range(0, len(data), 16):
        lines.append(
            " ".join(
                f"{b:02X}"
                for b in data[i:i + 16]
            )
        )

lines.append("q")

flash_txt.write_text("\n".join(lines) + "\n")

print("Slot A payload CRC32 : %08X" % payload_crc)
print("Slot A OTA bytes     :", len(image))
print("Slot A SHA256        :",
      hashlib.sha256(image).hexdigest())
PY

# ============================================================
# SLOT B
# ============================================================

echo
echo "=== [4/8] Build Slot B ==="

cd "$ROOT"

bash "$TOOLS/build_slot_b_ota.sh"

verify_out_version     "$REL/${PROJECT}_slot_b_vlo.out"     "Slot B"

B_TXT="$REL/${PROJECT}_slot_b_vlo_ota_crc.txt"

test -f "$B_TXT"

echo
echo "=== [5/8] Convert Slot B to binary release image ==="

python3 - "$B_TXT" "$DIST" <<'PY'
from pathlib import Path
import hashlib
import sys
import zlib

src = Path(sys.argv[1])
dist = Path(sys.argv[2])

HEADER_START = 0xA7C0
HEADER_SIZE = 44

LOW_START = 0xA8C0
LOW_SIZE = 0x56C0

HIGH_START = 0x11FFC
HIGH_SIZE = 0x1FFC

OTA_SIZE = HEADER_SIZE + LOW_SIZE + HIGH_SIZE

mem = {}
addr = None

for raw in src.read_text().splitlines():
    line = raw.strip()

    if not line:
        continue

    if line.lower() == "q":
        break

    if line.startswith("@"):
        addr = int(line[1:], 16)
        continue

    for token in line.split():
        mem[addr] = int(token, 16)
        addr += 1


def get(start, size):
    missing = [
        a for a in range(start, start + size)
        if a not in mem
    ]

    if missing:
        raise SystemExit(
            f"ERROR: missing Slot B byte "
            f"0x{missing[0]:05X}"
        )

    return bytes(
        mem[a]
        for a in range(start, start + size)
    )


header = get(HEADER_START, HEADER_SIZE)
low = get(LOW_START, LOW_SIZE)
high = get(HIGH_START, HIGH_SIZE)

image = header + low + high

if len(image) != OTA_SIZE:
    raise SystemExit(
        f"ERROR: Slot B OTA size {len(image)}"
    )

image_size = int.from_bytes(
    header[36:40], "little"
)

payload_crc = int.from_bytes(
    header[40:44], "little"
)

payload = low + high
calc_crc = zlib.crc32(payload) & 0xFFFFFFFF

if image_size != len(payload):
    raise SystemExit(
        "ERROR: Slot B image_size mismatch"
    )

if payload_crc != calc_crc:
    raise SystemExit(
        "ERROR: Slot B payload CRC mismatch"
    )

(dist / "slot_b.ota").write_bytes(image)
(dist / "slot_b_flasher.txt").write_text(
    src.read_text()
)

print("Slot B payload CRC32 : %08X" % payload_crc)
print("Slot B OTA bytes     :", len(image))
print("Slot B SHA256        :",
      hashlib.sha256(image).hexdigest())
PY

# ============================================================
# COMMAND STREAMS
# ============================================================

echo
echo "=== [6/8] Generate OTA command streams ==="

python3 - "$DIST" <<'PY'
from pathlib import Path
import sys
import zlib

dist = Path(sys.argv[1])

CHUNK_SIZE = 64


def crc16_ccitt(data):
    crc = 0xFFFF

    for value in data:
        crc ^= value << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = (
                    ((crc << 1) ^ 0x1021)
                    & 0xFFFF
                )
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


for slot in ("a", "b"):
    image = (dist / f"slot_{slot}.ota").read_bytes()

    if len(image) != 30440:
        raise SystemExit(
            f"ERROR: slot_{slot} OTA size "
            f"{len(image)}"
        )

    crc32 = zlib.crc32(image) & 0xFFFFFFFF

    commands = [
        f"ou=1,{len(image)},{crc32:08X};"
    ]

    for offset in range(
        0,
        len(image),
        CHUNK_SIZE
    ):
        chunk = image[offset:offset + CHUNK_SIZE]
        crc16 = crc16_ccitt(chunk)

        cmd = (
            f"oc={offset},{crc16:04X},"
            f"{chunk.hex().upper()};"
        )

        if len(cmd) > 160:
            raise SystemExit(
                f"ERROR: OTA command too long: {cmd}"
            )

        commands.append(cmd)

    commands.append(
        f"of={crc32:08X};"
    )

    out = dist / f"slot_{slot}_commands.txt"
    out.write_text("\n".join(commands) + "\n")

    print(
        f"Slot {slot.upper()} transport CRC32 : "
        f"{crc32:08X}"
    )
PY

# ============================================================
# MANIFEST
# ============================================================

echo
echo "=== [7/8] Build manifest.json ==="

python3 - "$DIST" "$VERSION" <<'PY'
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import sys
import zlib

dist = Path(sys.argv[1])
version = sys.argv[2]


def sha256(path):
    return hashlib.sha256(
        path.read_bytes()
    ).hexdigest()


def info(slot):
    ota = dist / f"slot_{slot}.ota"
    data = ota.read_bytes()

    if len(data) != 30440:
        raise SystemExit(
            f"ERROR: slot_{slot} size"
        )

    transport_crc = zlib.crc32(data) & 0xFFFFFFFF

    header = data[:44]

    payload_size = int.from_bytes(
        header[36:40],
        "little"
    )

    payload_crc = int.from_bytes(
        header[40:44],
        "little"
    )

    return {
        "file": ota.name,
        "flasher_file":
            f"slot_{slot}_flasher.txt",
        "commands_file":
            f"slot_{slot}_commands.txt",
        "ota_size": len(data),
        "payload_size": payload_size,
        "payload_crc32":
            f"{payload_crc:08X}",
        "transport_crc32":
            f"{transport_crc:08X}",
        "sha256": sha256(ota)
    }


manifest = {
    "product": "SKM-8",
    "project_generation": "8.7",
    "firmware_version": version,
    "hardware": "MSP430FR5949",
    "ota_format": 2,
    "chunk_size": 64,
    "created_at":
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
    "slot_a": info("a"),
    "slot_b": info("b")
}

(dist / "manifest.json").write_text(
    json.dumps(
        manifest,
        indent=2,
        ensure_ascii=False
    ) + "\n"
)

print(json.dumps(
    manifest,
    indent=2,
    ensure_ascii=False
))
PY

# ============================================================
# CHECKSUMS + RELEASE NOTES
# ============================================================

echo
echo "=== [8/8] SHA256 checksums ==="

cd "$DIST"

sha256sum \
  slot_a.ota \
  slot_b.ota \
  slot_a_flasher.txt \
  slot_b_flasher.txt \
  slot_a_commands.txt \
  slot_b_commands.txt \
  manifest.json \
  > checksums.sha256

cat > RELEASE_NOTES.md <<EOF_NOTES
# SKM-8 Firmware ${VERSION}

Project generation: 8.7

MCU: MSP430FR5949

OTA capabilities:

- A/B firmware slots
- AppHeader v2
- CRC32 image validation
- trial boot
- boot confirmation
- automatic rollback
- 64-byte OTA chunks
- duplicate chunk handling
- resume after reset / power loss
- dual-record CRC-protected FRAM journal

Release package generated automatically by:

    tools/ota/build_release.sh
EOF_NOTES

echo
echo "============================================================"
echo " RELEASE READY"
echo "============================================================"
echo
echo "$DIST"
echo
ls -lh "$DIST"
