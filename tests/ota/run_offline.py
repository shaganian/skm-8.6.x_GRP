#!/usr/bin/env python3
"""Compile only a host test harness, never an MSP430 image or release.

Extract the actual OTA C section and BootMeta/reset functions. Relocate hardware
addresses into mock FRAM and normalize MSP430 unsigned int/long to 16/32 bits.
No parser, writer, CRC, journal or finalization algorithm is reimplemented.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile


def function(text, name):
    match = re.search(r"^[\w ]+\b" + name + r"\([^;]*?\)\s*\{", text, re.M)
    if not match:
        raise AssertionError(f"Function missing: {name}")
    start = match.start()
    depth = 1
    pos = match.end()
    while depth:
        depth += (text[pos] == "{") - (text[pos] == "}")
        pos += 1
    return text[start:pos]


def replace_once(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


def abi(text):
    text = re.sub(r"\bunsigned long\b", "uint32_t", text)
    return re.sub(r"\bunsigned int\b", "uint16_t", text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--commands", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    image = args.image.read_bytes()
    assert hashlib.sha256(image).hexdigest() == (
        "d3cbc3d3cb9292e963c937fcb013709bb97a2c249f747e90a081a4a4f3124b35"
    ), "Use the audited 8.74 Slot B fixture"

    boot = (root / "boot_control.c").read_text()
    boot = re.sub(r"^#include.*$", "", boot, flags=re.M)
    boot = boot.replace("(volatile BootMeta *)(uintptr_t)BOOT_META_ADDR", "mock_meta()")
    boot = replace_once(boot, "(uintptr_t)&g_app_header", "test_header_address")
    util = (root / "drivers/utilites.c").read_text()
    begin = util.index("typedef struct ota_rx_state")
    end = util.index("\n}", util.index("static char ota_remote_command(void)\n{")) + 2
    ota = util[begin:end]
    ota = replace_once(ota, "(unsigned long)(uintptr_t)&g_app_header", "test_header_address")
    ota = replace_once(ota, "(const AppHeader *)(uintptr_t)header_addr", "mock_header(header_addr)")
    modem = (root / "drivers/modem_sim800c_33.c").read_text()
    reset_gate = function(modem, "ota_reset_after_response")

    # Check the hardware integration around the host-tested gate. SEND timeout
    # must exit, and status must be captured before request construction.
    snapshot = modem.index("ota_sent_status = ota_get_status();")
    request = modem.index("httpreq_smart();", snapshot)
    send = modem.index('cmp_str("SEND OK"', request)
    timeout = modem.index("if(time_w1 >= 20000u)", send)
    receive = modem.index('"CLOSED\\r\\n"', timeout)
    parse = modem.index("result_remote_command = parse_remote_command();", receive)
    gate = modem.index("ota_reset_after_response(\n", parse)
    assert snapshot < request < send < timeout < receive < parse < gate
    assert "return 0;" in modem[timeout:receive]
    assert modem.count("ota_software_reset();") == 1
    assert "response_ok == 1u" in reset_gate
    assert "ota_result = ota_remote_command();" in util
    assert "if (ota_result) {\n        return ota_result;" in util
    assert "ota_sent_status, result_remote_command" in modem[gate:gate + 140]

    header = (root / "app_header.h").read_text().replace(
        "typedef void (*app_code_ptr_t)(void);", "typedef uint32_t app_code_ptr_t;"
    )
    meta = (root / "boot_meta.h").read_text()
    harness = (root / "tests/ota/offline_test.c").read_text()
    source = harness.replace("/* GENERATED_HEADERS */", header + meta)
    source = source.replace("/* GENERATED_PRODUCTION_CODE */", "\n".join([
        abi((root / "drivers/ota_crc32.h").read_text()), abi(boot), function(util, "hex_nibble"), abi(ota), reset_gate,
    ]))
    with tempfile.TemporaryDirectory(prefix="skm-ota-offline-") as directory:
        test = Path(directory) / "test.c"
        binary = Path(directory) / "test"
        test.write_text(source)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fno-strict-aliasing", "-fsyntax-only", str(test)], check=True)
        subprocess.run([
            "cc", "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fno-strict-aliasing", "-fanalyzer", "-fsanitize=undefined", "-fno-sanitize-recover=all",
            str(test), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary), str(args.image.resolve()), str(args.commands.resolve())], check=True)
    print("PASS: modem request snapshot / SEND OK / response / reset integration")


if __name__ == "__main__":
    main()
