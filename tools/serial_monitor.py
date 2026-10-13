import argparse
import sys
import time
from pathlib import Path

parser = argparse.ArgumentParser(description="Read ESP8266 Hello World serial output.")
parser.add_argument("--serial-library")
parser.add_argument("--port", required=True)
parser.add_argument("--seconds", type=int, default=15)
parser.add_argument("--log")
args = parser.parse_args()

if args.serial_library:
    sys.path.insert(0, args.serial_library)
import serial

received = []
port = serial.Serial(baudrate=115200, timeout=0.25)
port.dtr = False
port.rts = False
port.port = args.port
try:
    port.open()
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        raw = port.readline()
        if raw:
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            received.append(line)
            print(line, flush=True)
finally:
    port.close()
    if args.log:
        Path(args.log).write_text("\n".join(received) + "\n", encoding="utf-8")
