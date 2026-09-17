#!/usr/bin/env python3

from pathlib import Path
import sys
import zlib


HEADER_START = 0xA7C0
HEADER_SIZE  = 44

LOW_START = 0xA8C0
LOW_SIZE  = 0x56C0

HIGH_START = 0x11FFC
HIGH_SIZE  = 0x1FFC

OTA_SIZE = HEADER_SIZE + LOW_SIZE + HIGH_SIZE
CHUNK_SIZE = 16


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF

    for value in data:
        crc ^= value << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


def read_ti_txt(path: Path):
    mem = {}
    addr = None

    for raw in path.read_text().splitlines():
        line = raw.strip()

        if not line:
            continue

        if line.lower() == "q":
            break

        if line.startswith("@"):
            addr = int(line[1:], 16)
            continue

        if addr is None:
            raise RuntimeError("TI-TXT data before address")

        for token in line.split():
            mem[addr] = int(token, 16)
            addr += 1

    return mem


def get_region(mem, start, size):
    missing = [
        a for a in range(start, start + size)
        if a not in mem
    ]

    if missing:
        raise RuntimeError(
            f"Missing byte at 0x{missing[0]:05X}"
        )

    return bytes(
        mem[a]
        for a in range(start, start + size)
    )


def main():
    if len(sys.argv) != 3:
        print(
            f"Usage: {sys.argv[0]} "
            "<slot_b_ota_crc.txt> <commands.txt>"
        )
        return 1

    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])

    mem = read_ti_txt(src)

    image = (
        get_region(mem, HEADER_START, HEADER_SIZE)
        + get_region(mem, LOW_START, LOW_SIZE)
        + get_region(mem, HIGH_START, HIGH_SIZE)
    )

    if len(image) != OTA_SIZE:
        raise RuntimeError(
            f"Invalid OTA size: {len(image)}, "
            f"expected {OTA_SIZE}"
        )

    crc32 = zlib.crc32(image) & 0xFFFFFFFF

    commands = []

    commands.append(
        f"ou=1,{len(image)},{crc32:08X};"
    )

    for offset in range(0, len(image), CHUNK_SIZE):
        chunk = image[offset:offset + CHUNK_SIZE]
        crc16 = crc16_ccitt(chunk)

        command = (
            f"oc={offset},{crc16:04X},"
            f"{chunk.hex().upper()};"
        )

        if len(command) > 64:
            raise RuntimeError(
                f"Command too long ({len(command)} bytes): "
                f"{command}"
            )

        commands.append(command)

    commands.append(
        f"of={crc32:08X};"
    )

    dst.write_text("\n".join(commands) + "\n")

    print("Source       :", src)
    print("Output       :", dst)
    print("OTA bytes    :", len(image))
    print("Chunk size   :", CHUNK_SIZE)
    print("Chunks       :", (len(image) + CHUNK_SIZE - 1) // CHUNK_SIZE)
    print("Transport CRC:", f"{crc32:08X}")
    print("Commands     :", len(commands))
    print()
    print("START :", commands[0])
    print("FIRST :", commands[1])
    print("LAST  :", commands[-2])
    print("FINISH:", commands[-1])

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
