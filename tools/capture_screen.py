#!/usr/bin/env python3
import argparse
import struct
import time
import zlib
from pathlib import Path

import serial

SCREEN_NAMES = ["status", "paths", "heards", "heatstrip", "savepoints", "mqtt"]


def parse_pbm(text):
    lines = []
    capture = False
    for raw in text.splitlines():
        line = raw.strip()
        if line == "BEGIN_SCREEN_PBM":
            capture = True
            continue
        if line == "END_SCREEN_PBM":
            break
        if capture and line:
            lines.append(line)

    if len(lines) < 2 or lines[0] != "P1":
        raise RuntimeError("screen.dump did not return a P1 PBM frame")

    width, height = [int(v) for v in lines[1].split()]
    bits = []
    for line in lines[2:]:
        bits.extend(1 if v == "1" else 0 for v in line.split())

    if len(bits) != width * height:
        raise RuntimeError(f"PBM pixel count mismatch: got {len(bits)}, expected {width * height}")

    return width, height, bits


def png_chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, width, height, bits, scale):
    out_w = width * scale
    out_h = height * scale
    rows = bytearray()

    for y in range(height):
        scaled_row = bytearray([0])
        for x in range(width):
            value = 0 if bits[y * width + x] else 255
            for _ in range(scale):
                scaled_row.extend((value, value, value))
        for _ in range(scale):
            rows.extend(scaled_row)

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", out_w, out_h, 8, 2, 0, 0, 0)))
    png.extend(png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
    png.extend(png_chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def main():
    parser = argparse.ArgumentParser(description="Capture the Heltec Wireless Paper framebuffer as PNG")
    parser.add_argument("--port", default="/dev/cu.usbserial-0001")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", default="docs/screen-current.png")
    parser.add_argument("--output-dir", default="docs/screens")
    parser.add_argument("--scale", type=int, default=3)
    parser.add_argument("--screen", type=int, default=None, help="Render and capture a specific UI screen index")
    parser.add_argument("--all", action="store_true", help="Render and capture all UI screens")
    args = parser.parse_args()

    def capture_one(ser, command):
        ser.reset_input_buffer()
        ser.write(command.encode() + b"\r\n")
        ser.flush()

        data = bytearray()
        deadline = time.time() + 8
        while time.time() < deadline:
            chunk = ser.read(4096)
            if chunk:
                data.extend(chunk)
                if b"END_SCREEN_PBM" in data:
                    break

        text = data.decode(errors="replace")
        return parse_pbm(text)

    with serial.Serial(args.port, args.baud, timeout=2) as ser:
        time.sleep(0.5)
        if args.all:
            out_dir = Path(args.output_dir)
            for screen, name in enumerate(SCREEN_NAMES):
                width, height, bits = capture_one(ser, f"screen.dump {screen}")
                output = out_dir / f"{screen:02d}-{name}.png"
                write_png(output, width, height, bits, max(1, args.scale))
                print(f"wrote {output} ({width}x{height}, scale {max(1, args.scale)})")
        else:
            command = "screen.dump" if args.screen is None else f"screen.dump {args.screen}"
            width, height, bits = capture_one(ser, command)
            output = Path(args.output)
            write_png(output, width, height, bits, max(1, args.scale))
            print(f"wrote {output} ({width}x{height}, scale {max(1, args.scale)})")


if __name__ == "__main__":
    main()
