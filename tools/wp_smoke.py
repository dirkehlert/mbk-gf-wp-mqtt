#!/usr/bin/env python3
import argparse
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - only hit outside the PlatformIO venv
    print("pyserial is required. Run with PlatformIO's Python or install pyserial.", file=sys.stderr)
    raise


DEFAULT_COMMANDS = [
    "clock",
    "web.ap status",
    "web.view status",
    "mqtt status",
    "sp.list",
]


def send_command(port: str, baud: int, command: str, timeout: float) -> str:
    with serial.Serial(port, baud, timeout=timeout) as ser:
        time.sleep(0.25)
        ser.reset_input_buffer()
        ser.write((command + "\r\n").encode("utf-8"))
        ser.flush()
        time.sleep(timeout)
        return ser.read(8192).decode(errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser(description="Hardware smoke test for the MBK GF WP monitor CLI.")
    parser.add_argument("--port", default="/dev/cu.usbserial-0001")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("commands", nargs="*", default=DEFAULT_COMMANDS)
    args = parser.parse_args()

    failed = False
    for command in args.commands:
        response = send_command(args.port, args.baud, command, args.timeout)
        print(f"$ {command}")
        print(response.rstrip() or "<no response>")
        if "Err" in response or "Traceback" in response or not response.strip():
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
