# SPDX-FileCopyrightText: 2026 FEmbed
# SPDX-License-Identifier: Apache-2.0
"""Compare production C register traces with esptool 5.3.1 power_on_flash."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True, type=Path)
parser.add_argument("--esptool-root", type=Path)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
if args.esptool_root:
    sys.path.insert(0, str(args.esptool_root))
from esptool.targets import esp32p4


class Reference(esp32p4.ESP32P4ROM):
    def __init__(self, eco, xpd):
        self.secure_download_mode = False
        self.revision = {5: 300, 6: 301, 7: 302, 8: 303}[eco]
        self.events = []
        self.registers = {
            0x501151BC: 0xA5A50200,
            0x501151B8: 0x12550122,
            0x501153FC: 0xDEADBEEF,
            0x5011010C: 0x89ABCDE1,
            0x5012D034: (1 << 16) if xpd else 0,
        }

    def get_chip_revision(self):
        return self.revision

    def read_reg(self, address):
        self.events.append(["read", address])
        return self.registers[address]

    def write_reg(self, address, value):
        self.events.append(["write", address, value])
        self.registers[address] = value


def native(eco, xpd, fail=0, p4=1, protocol=0, secure=0):
    return json.loads(
        subprocess.check_output(
            [
                str(args.executable),
                str(eco),
                str(xpd),
                str(fail),
                str(p4),
                str(protocol),
                str(secure),
            ]
        )
    )


report = []
for eco in (5, 6, 7, 8):
    for xpd in (0, 1):
        reference = Reference(eco, xpd)
        esp32p4.sleep = lambda seconds, reference=reference: reference.events.append(
            ["delay_ms", seconds * 1000]
        )
        reference.power_on_flash()
        result = native(eco, xpd)
        c_io = [event for event in result["events"] if event[0] in ("read", "write")]
        reference_io = [
            event for event in reference.events if event[0] in ("read", "write")
        ]
        assert result["result"] == 0 and c_io == reference_io, (
            eco,
            xpd,
            c_io,
            reference_io,
        )
        c_delays = [event[1] for event in result["events"] if event[0] == "delay_ms"]
        reference_delays = [
            event[1] for event in reference.events if event[0] == "delay_ms"
        ]
        assert len(c_delays) == len(reference_delays)
        assert all(c >= r for c, r in zip(c_delays, reference_delays))
        for failed_call in range(1, result["calls"] + 1):
            failed = native(eco, xpd, failed_call)
            assert failed["result"] == 2 and failed["calls"] == failed_call
        report.append(
            {
                "eco": eco,
                "xpd": xpd,
                "events": result["events"],
                "reference_matches": True,
                "failed_io_cases": result["calls"],
            }
        )
for p4, protocol, secure in ((1, 0, 1), (0, 0, 0), (1, 1, 0), (1, 2, 0)):
    result = native(7, 1, p4=p4, protocol=protocol, secure=secure)
    assert result["result"] == 0 and result["events"] == [["end"]]
    assert result["calls"] == (1 if secure else 0)
if args.output:
    args.output.write_text(
        json.dumps(
            {
                "reference_esptool": "5.3.1",
                "cases": report,
                "secure_other_target_and_protocol_unchanged": True,
            },
            indent=2,
        )
    )
print(
    "8 reference cases, all I/O failure points, secure/other-target/protocol skip: PASS"
)
