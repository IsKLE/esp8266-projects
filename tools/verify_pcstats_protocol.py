"""Exercise packet handling and stale-data behavior on the connected ESP8266."""
import argparse
import json
import sys
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--port", default="COM3")
parser.add_argument("--log", required=True, type=Path)
parser.add_argument("--serial-library")
args = parser.parse_args()
if args.serial_library:
    sys.path.insert(0, args.serial_library)
import serial

results = []
port = serial.Serial(baudrate=115200, timeout=0.2)
port.dtr = False
port.rts = False
port.port = args.port


def expect_line(expected, timeout=3):
    deadline = time.monotonic() + timeout
    received = []
    while time.monotonic() < deadline:
        raw = port.readline()
        if raw:
            line = raw.decode("ascii", errors="replace").strip()
            received.append(line)
            if line == expected:
                return received
    raise AssertionError("Expected {!r}; received {!r}".format(expected, received))


def send_case(name, payload, expected):
    port.write(payload)
    lines = expect_line(expected)
    results.append({"case": name, "result": "PASS", "response": lines})


try:
    port.open()
    port.reset_input_buffer()
    send_case("valid measurements", b"PC1,100,550,640,30,20,45\n", "ACK1,100,550,640,30,20,45")
    send_case("reject nonnumeric temperature", b"PC1,101,bad,640,30,20,45\n", "ERR1,INVALID")
    send_case("reject out of range load", b"PC1,102,550,640,101,20,45\n", "ERR1,INVALID")
    send_case("reject extra fields", b"PC1,103,550,640,30,20,45,99\n", "ERR1,INVALID")
    send_case("discard oversized line", b"X" * 200 + b"\n", "ERR1,OVERFLOW")
    send_case("recover after oversized line", b"PC1,104,550,640,30,20,45\n", "ACK1,104,550,640,30,20,45")
    results.append({"case": "show offline after stale input", "result": "PASS",
                    "response": expect_line("STATE1,OFFLINE", timeout=7)})
    send_case("missing sensor values remain unavailable", b"PC1,105,-999,640,-1,20,45\n", "ACK1,105,-999,640,-1,20,45")
    port.write(b"PC1,106,550,")
    time.sleep(0.2)
    port.write(b"640,30,20,45\n")
    results.append({"case": "fragmented serial packet", "result": "PASS",
                    "response": expect_line("ACK1,106,550,640,30,20,45")})
finally:
    port.close()
    args.log.write_text(json.dumps(results, indent=2), encoding="utf-8")

print("{} hardware protocol checks passed".format(len(results)))
