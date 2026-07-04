#!/usr/bin/env python3
"""Passive serial crash monitor for physical T-Deck UI validation.

The monitor is intentionally read-only. It opens the serial port with the same
conservative DTR/RTS defaults as safe_serial_monitor.py, timestamps incoming
lines, and reports reset/crash signatures that appear while a human performs
touch or trackball actions on the device.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

from safe_serial_monitor import DEFAULT_BAUD, DEFAULT_TIMEOUT_S, READ_SIZE, open_safe_serial


RESET_PATTERNS = (
    "ESP-ROM:",
    "rst:",
    "Saved PC:",
    "boot:",
)

CRASH_PATTERNS = (
    "Guru Meditation",
    "panic",
    "PANIC",
    "Backtrace:",
    "assert failed",
    "abort()",
    "LoadProhibited",
    "StoreProhibited",
    "IllegalInstruction",
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="explicit port, for example COM8")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--duration", type=float, default=120.0)
    parser.add_argument("--label", default="manual-ui-test")
    parser.add_argument("--output", type=Path, help="raw byte capture path")
    parser.add_argument("--json-report", type=Path, help="summary JSON report path")
    parser.add_argument("--expect-no-reset", action="store_true")
    parser.add_argument("--dtr", action="store_true")
    parser.add_argument("--rts", action="store_true")
    parser.add_argument("--clear-buffer", action="store_true")
    return parser


def detect_event(line: str) -> str | None:
    if any(pattern in line for pattern in CRASH_PATTERNS):
        return "crash"
    if any(pattern in line for pattern in RESET_PATTERNS):
        return "reset"
    return None


def emit_line(start: float, line: str, events: list[dict[str, object]]) -> None:
    elapsed = time.monotonic() - start
    clean = line.rstrip("\r\n")
    kind = detect_event(clean)
    print(f"[{elapsed:8.3f}] {clean}")
    if kind:
        events.append({"t": round(elapsed, 3), "kind": kind, "line": clean})


def capture(args: argparse.Namespace) -> tuple[int, list[dict[str, object]], int]:
    try:
        import serial
    except ImportError:
        print("ERROR: pyserial is required. Install with: python -m pip install pyserial", file=sys.stderr)
        return 2, [], 0

    events: list[dict[str, object]] = []
    raw_count = 0
    output = None
    pending = ""
    start = time.monotonic()
    deadline = start + args.duration if args.duration > 0 else None

    try:
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            output = args.output.open("wb")

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
                "ui crash monitor open: "
                f"label={args.label} port={args.port} baud={args.baud} "
                f"dtr={'on' if args.dtr else 'off'} rts={'on' if args.rts else 'off'}",
                file=sys.stderr,
            )
            while deadline is None or time.monotonic() < deadline:
                data = ser.read(READ_SIZE)
                if not data:
                    continue
                raw_count += len(data)
                if output:
                    output.write(data)
                    output.flush()

                text = data.decode("utf-8", errors="replace")
                pending += text
                while "\n" in pending:
                    line, pending = pending.split("\n", 1)
                    emit_line(start, line + "\n", events)

        if pending:
            emit_line(start, pending, events)
    except KeyboardInterrupt:
        pass
    except serial.SerialException as exc:
        print(f"ERROR: serial open/read failed: {exc}", file=sys.stderr)
        return 1, events, raw_count
    finally:
        if output:
            output.close()

    has_reset_or_crash = any(e["kind"] in ("reset", "crash") for e in events)
    if args.expect_no_reset and has_reset_or_crash:
        return 3, events, raw_count
    return 0, events, raw_count


def write_report(args: argparse.Namespace, exit_code: int, events: list[dict[str, object]], raw_count: int) -> None:
    if not args.json_report:
        return
    args.json_report.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "label": args.label,
        "port": args.port,
        "baud": args.baud,
        "duration_s": args.duration,
        "dtr": bool(args.dtr),
        "rts": bool(args.rts),
        "raw_bytes": raw_count,
        "event_count": len(events),
        "events": events,
        "exit_code": exit_code,
    }
    args.json_report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.duration < 0:
        print("ERROR: --duration must be >= 0", file=sys.stderr)
        return 2
    exit_code, events, raw_count = capture(args)
    write_report(args, exit_code, events, raw_count)
    print(
        f"ui crash monitor summary: bytes={raw_count} events={len(events)} exit={exit_code}",
        file=sys.stderr,
    )
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
