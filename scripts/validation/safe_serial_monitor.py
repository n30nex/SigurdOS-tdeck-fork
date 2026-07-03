#!/usr/bin/env python3
"""Read a serial port without asserting DTR or RTS by default.

This helper is intended for Windows-hosted T-Deck log capture where opening the
port with generic terminal defaults can reset the ESP32-S3 into ROM download
mode. It is read-only: it does not send input to the device.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path


DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 0.2
READ_SIZE = 4096


def open_safe_serial(
    serial_module,
    port: str,
    baud: int = DEFAULT_BAUD,
    timeout: float = DEFAULT_TIMEOUT_S,
    dtr: bool = False,
    rts: bool = False,
):
    """Open a serial port after applying conservative modem-control states."""
    ser = serial_module.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = timeout
    ser.write_timeout = 2
    ser.xonxoff = False
    ser.rtscts = False
    ser.dsrdtr = False

    # PySerial applies these pre-open states during open(). Re-apply after open
    # as a best-effort guard for drivers that ignore the cached state.
    ser.dtr = dtr
    ser.rts = rts
    ser.open()
    ser.dtr = dtr
    ser.rts = rts
    return ser


def state_name(enabled: bool) -> str:
    return "on" if enabled else "off"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="explicit port, for example COM8")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="seconds to capture; 0 means read until Ctrl+C",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="optional raw byte capture file; parent directories are created",
    )
    parser.add_argument(
        "--dtr",
        action="store_true",
        help="assert DTR explicitly; default leaves DTR deasserted",
    )
    parser.add_argument(
        "--rts",
        action="store_true",
        help="assert RTS explicitly; default leaves RTS deasserted",
    )
    parser.add_argument(
        "--clear-buffer",
        action="store_true",
        help="discard bytes already buffered after opening before capture starts",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the planned settings without opening the port",
    )
    return parser


def capture_bytes(ser, output: Path | None, duration: float) -> int:
    log = None
    try:
        if output is not None:
            output.parent.mkdir(parents=True, exist_ok=True)
            log = output.open("wb")

        deadline = None if duration <= 0 else time.monotonic() + duration
        while deadline is None or time.monotonic() < deadline:
            data = ser.read(READ_SIZE)
            if not data:
                continue
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
            if log is not None:
                log.write(data)
                log.flush()
    except KeyboardInterrupt:
        return 130
    finally:
        if log is not None:
            log.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.duration < 0:
        print("ERROR: --duration must be >= 0", file=sys.stderr)
        return 2

    if args.dry_run:
        print(
            "safe serial dry run: "
            f"port={args.port} baud={args.baud} "
            f"dtr={state_name(args.dtr)} rts={state_name(args.rts)}"
        )
        return 0

    try:
        import serial
    except ImportError:
        print(
            "ERROR: pyserial is required. Install with: python -m pip install pyserial",
            file=sys.stderr,
        )
        return 2

    try:
        with open_safe_serial(
            serial,
            args.port,
            args.baud,
            timeout=DEFAULT_TIMEOUT_S,
            dtr=args.dtr,
            rts=args.rts,
        ) as ser:
            if args.clear_buffer:
                ser.reset_input_buffer()
            print(
                "safe serial open: "
                f"port={args.port} baud={args.baud} "
                f"dtr={state_name(args.dtr)} rts={state_name(args.rts)}",
                file=sys.stderr,
            )
            return capture_bytes(ser, args.output, args.duration)
    except serial.SerialException as exc:
        print(f"ERROR: serial open failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
