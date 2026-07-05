#!/usr/bin/env python3
"""Guided release-firmware UI journey validator for physical T-Deck testing.

This harness is intentionally conservative: it opens only the explicit serial
port, leaves DTR/RTS deasserted by default, and monitors serial output while a
human performs physical touch/trackball/keyboard actions on the device.

It does not switch the device into remote-test mode and it does not inject UI
input. Optional serial screenshot commands are available only when explicitly
requested, because release firmware normally keeps serial commands disabled.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from safe_serial_monitor import DEFAULT_BAUD, DEFAULT_TIMEOUT_S, READ_SIZE, open_safe_serial
from ui_crash_monitor import detect_event


DEFAULT_FORBIDDEN_PORTS = ("COM11", "COM12", "COM16", "COM29")


try:
    from PIL import Image
except ImportError:  # pragma: no cover - optional runtime dependency
    Image = None


@dataclass(frozen=True)
class JourneyStep:
    name: str
    prompt: str
    duration_s: float
    allow_reset: bool = False


PROFILES: dict[str, list[JourneyStep]] = {
    "chat-public-login": [
        JourneyStep(
            "home-ready",
            "Start on Home. Verify the device is awake and responsive.",
            20.0,
        ),
        JourneyStep(
            "open-chat",
            "Open Chat from Home using touch or trackball.",
            45.0,
        ),
        JourneyStep(
            "tap-public",
            "Tap the Public channel row. Confirm it opens without reboot.",
            45.0,
        ),
        JourneyStep(
            "trackball-public",
            "Return to Chat, focus Public with the trackball, then click it.",
            45.0,
        ),
        JourneyStep(
            "krabs-lagoon-login",
            "Open Krabs Lagoon and attempt login. Record visible success, pending, or failure.",
            75.0,
        ),
        JourneyStep(
            "repeater-password-login",
            "Open the local repeater and attempt password login.",
            75.0,
        ),
        JourneyStep(
            "repeater-detail-back",
            "Open repeater detail, then go back using the global back affordance or Backspace.",
            45.0,
        ),
    ],
    "core-navigation": [
        JourneyStep("home-ready", "Start on Home. Verify the device is awake.", 20.0),
        JourneyStep("open-chat", "Open Chat, then return Home.", 45.0),
        JourneyStep("open-contacts", "Open Contacts, then return Home.", 45.0),
        JourneyStep("open-repeaters", "Open Repeaters, then return Home.", 45.0),
        JourneyStep("open-map", "Open Map, press Use GPS/Finding Sats if visible, then return Home.", 60.0),
        JourneyStep("open-settings", "Open Settings and visit GPS/System status rows.", 60.0),
    ],
    "chat-dm-persistence": [
        JourneyStep(
            "home-ready",
            "Start on Home. Verify the device is awake and responsive.",
            20.0,
        ),
        JourneyStep(
            "open-chats-public",
            "Open CHATS, then open Public with touch and trackball. Confirm no freeze or reboot.",
            60.0,
        ),
        JourneyStep(
            "receive-public",
            "Send a Public message from another node. Confirm the unread/toast behavior and visible history.",
            75.0,
        ),
        JourneyStep(
            "receive-dm",
            "Send a DM from another node while this device is on the chat list. Confirm toast/unread and that a DM row appears immediately.",
            90.0,
        ),
        JourneyStep(
            "verify-dm-filter",
            "Open DMs and confirm the DM is there, then return to CHATS and confirm the DM row is not listed under CHATS.",
            75.0,
        ),
        JourneyStep(
            "reboot-for-persistence",
            "Reboot the T-Deck, wait for Home, then reopen CHATS and DMs to confirm Public history and DM history persisted.",
            120.0,
            allow_reset=True,
        ),
        JourneyStep(
            "post-reboot-public-click",
            "After the reboot check, open Public again using the trackball click. Confirm no reboot.",
            60.0,
        ),
    ],
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_out_dir() -> Path:
    stamp = datetime.now().strftime("%Y-%m-%d-%H%M%S")
    return repo_root() / ".pio" / "hardware_ui_journeys" / stamp


def normalize_port(port: str) -> str:
    return port.strip().upper().removeprefix("\\\\.\\")


def safe_name(name: str) -> str:
    lowered = name.lower()
    cleaned = re.sub(r"[^a-z0-9._-]+", "-", lowered).strip("-")
    return cleaned or "step"


def load_steps(args: argparse.Namespace) -> list[JourneyStep]:
    if args.journey_json:
        data = json.loads(args.journey_json.read_text(encoding="utf-8"))
        raw_steps = data["steps"] if isinstance(data, dict) else data
        steps = []
        for idx, raw in enumerate(raw_steps, start=1):
            steps.append(
                JourneyStep(
                    name=str(raw.get("name") or f"step-{idx}"),
                    prompt=str(raw["prompt"]),
                    duration_s=float(raw.get("duration_s", args.default_step_duration)),
                    allow_reset=bool(raw.get("allow_reset", False)),
                ),
            )
        return steps
    return list(PROFILES[args.profile])


def apply_duration_override(steps: list[JourneyStep], override: float | None) -> list[JourneyStep]:
    if override is None:
        return steps
    return [JourneyStep(step.name, step.prompt, override, step.allow_reset) for step in steps]


def split_lines(pending: str, text: str) -> tuple[list[str], str]:
    pending += text
    lines: list[str] = []
    while "\n" in pending:
        line, pending = pending.split("\n", 1)
        lines.append(line.rstrip("\r"))
    return lines, pending


def record_line(
    line: str,
    start: float,
    events: list[dict[str, object]],
    echo: bool,
) -> None:
    kind = detect_event(line)
    if echo:
        print(f"[serial {time.monotonic() - start:8.3f}] {line}")
    if kind:
        events.append(
            {
                "t": round(time.monotonic() - start, 3),
                "kind": kind,
                "line": line,
            },
        )


def monitor_step(ser, step: JourneyStep, raw_path: Path, echo: bool) -> dict[str, object]:
    raw_count = 0
    events: list[dict[str, object]] = []
    pending = ""
    started = datetime.now().isoformat(timespec="seconds")
    start = time.monotonic()
    deadline = start + step.duration_s

    with raw_path.open("wb") as raw:
        while time.monotonic() < deadline:
            data = ser.read(READ_SIZE)
            if not data:
                continue
            raw_count += len(data)
            raw.write(data)
            raw.flush()
            text = data.decode("utf-8", errors="replace")
            lines, pending = split_lines(pending, text)
            for line in lines:
                record_line(line, start, events, echo)

    if pending:
        record_line(pending, start, events, echo)

    ended = datetime.now().isoformat(timespec="seconds")
    has_crash = any(event["kind"] == "crash" for event in events)
    has_reset = any(event["kind"] == "reset" for event in events)
    return {
        "name": step.name,
        "prompt": step.prompt,
        "duration_s": step.duration_s,
        "allow_reset": step.allow_reset,
        "started": started,
        "ended": ended,
        "raw_bytes": raw_count,
        "raw_log": str(raw_path),
        "event_count": len(events),
        "events": events,
        "passed": not has_crash and (step.allow_reset or not has_reset),
    }


def rgb565_hex_to_png(hex_data: str, width: int, height: int, output_path: Path) -> bool:
    if Image is None:
        return False
    import struct

    raw = bytes.fromhex(hex_data)
    expected = width * height * 2
    if len(raw) < expected:
        raw += b"\x00" * (expected - len(raw))
    elif len(raw) > expected:
        raw = raw[:expected]

    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for y in range(height):
        row_offset = y * width * 2
        for x in range(width):
            rgb565 = struct.unpack_from("<H", raw, row_offset + x * 2)[0]
            r = ((rgb565 >> 11) & 0x1F) << 3
            g = ((rgb565 >> 5) & 0x3F) << 2
            b = (rgb565 & 0x1F) << 3
            pixels[x, y] = (r, g, b)
    image.save(output_path)
    return True


def capture_screenshot(ser, step_name: str, out_dir: Path, command: str, timeout_s: float) -> dict[str, object]:
    started = datetime.now().isoformat(timespec="seconds")
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()

    width = 320
    height = 240
    stride = 640
    header_found = False
    done = False
    hex_data = ""
    lines: list[str] = []
    pending = ""
    start = time.monotonic()
    deadline = start + timeout_s

    while time.monotonic() < deadline and not done:
        data = ser.read(READ_SIZE)
        if not data:
            continue
        chunk = data.decode("utf-8", errors="replace")
        parsed, pending = split_lines(pending, chunk)
        for line in parsed:
            lines.append(line)
            if line.startswith("[capture] W="):
                header_found = True
                for part in line.replace("[capture] ", "").split():
                    if part.startswith("W="):
                        width = int(part[2:])
                    elif part.startswith("H="):
                        height = int(part[2:])
                    elif part.startswith("S="):
                        stride = int(part[2:])
            elif line.startswith("[cdata] "):
                hex_data += line[8:]
            elif line == "[capture] END":
                done = True
                break

    if pending:
        lines.append(pending)

    raw_path = out_dir / f"{safe_name(step_name)}.capture.txt"
    raw_path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")

    result: dict[str, object] = {
        "step": step_name,
        "command": command,
        "started": started,
        "timeout_s": timeout_s,
        "supported": bool(header_found),
        "complete": bool(done),
        "raw_capture": str(raw_path),
        "width": width if header_found else None,
        "height": height if header_found else None,
        "stride": stride if header_found else None,
        "png": None,
        "png_written": False,
    }
    if header_found and done and hex_data:
        png_path = out_dir / f"{safe_name(step_name)}.png"
        result["png"] = str(png_path)
        result["png_written"] = rgb565_hex_to_png(hex_data, width, height, png_path)
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=False, help="explicit serial port, for example COM8")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--profile", choices=sorted(PROFILES), default="chat-public-login")
    parser.add_argument("--journey-json", type=Path, help="custom journey JSON file")
    parser.add_argument("--out-dir", type=Path, default=default_out_dir())
    parser.add_argument("--default-step-duration", type=float, default=45.0)
    parser.add_argument("--step-duration", type=float, help="override every step duration")
    parser.add_argument("--screenshot-mode", choices=("none", "attempt", "require"), default="none")
    parser.add_argument("--screenshot-command", default="SCREENSHOT")
    parser.add_argument("--screenshot-timeout", type=float, default=30.0)
    parser.add_argument("--yes", action="store_true", help="do not wait for Enter before each step")
    parser.add_argument("--echo-serial", action="store_true", help="print decoded serial lines while monitoring")
    parser.add_argument("--allow-reset", action="store_true", help="do not fail the journey on reset/crash signatures")
    parser.add_argument("--dtr", action="store_true", help="assert DTR explicitly; default leaves DTR deasserted")
    parser.add_argument("--rts", action="store_true", help="assert RTS explicitly; default leaves RTS deasserted")
    parser.add_argument(
        "--forbid-port",
        action="append",
        default=list(DEFAULT_FORBIDDEN_PORTS),
        help=(
            "port that must not be opened; defaults to "
            + ", ".join(DEFAULT_FORBIDDEN_PORTS)
        ),
    )
    parser.add_argument("--dry-run", action="store_true", help="print the plan without opening serial")
    parser.add_argument("--list-profiles", action="store_true", help="list built-in profiles and exit")
    return parser


def print_profile_list() -> None:
    for name, steps in PROFILES.items():
        print(f"{name}:")
        for step in steps:
            reset_note = " (reset allowed)" if step.allow_reset else ""
            print(f"  - {step.name}: {step.duration_s:g}s{reset_note}")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.list_profiles:
        print_profile_list()
        return 0
    if not args.port:
        print("ERROR: --port is required unless --list-profiles is used", file=sys.stderr)
        return 2
    if args.default_step_duration <= 0 or (args.step_duration is not None and args.step_duration <= 0):
        print("ERROR: step durations must be > 0", file=sys.stderr)
        return 2

    normalized = normalize_port(args.port)
    forbidden = {normalize_port(port) for port in args.forbid_port}
    if normalized in forbidden:
        print(f"ERROR: refusing to open forbidden port {args.port}", file=sys.stderr)
        return 2

    steps = apply_duration_override(load_steps(args), args.step_duration)
    print(f"hardware UI journey profile={args.profile} steps={len(steps)}")
    for idx, step in enumerate(steps, start=1):
        reset_note = ", reset allowed" if step.allow_reset else ""
        print(f"{idx}. {step.name} ({step.duration_s:g}s{reset_note}): {step.prompt}")

    if args.dry_run:
        return 0

    try:
        import serial
    except ImportError:
        print("ERROR: pyserial is required. Install with: python -m pip install pyserial", file=sys.stderr)
        return 2

    args.out_dir.mkdir(parents=True, exist_ok=True)
    raw_dir = args.out_dir / "raw"
    raw_dir.mkdir(exist_ok=True)
    capture_dir = args.out_dir / "screenshots"
    if args.screenshot_mode != "none":
        capture_dir.mkdir(exist_ok=True)

    started = datetime.now().isoformat(timespec="seconds")
    results: list[dict[str, object]] = []
    screenshots: list[dict[str, object]] = []
    exit_code = 0

    try:
        with open_safe_serial(
            serial,
            args.port,
            args.baud,
            timeout=DEFAULT_TIMEOUT_S,
            dtr=args.dtr,
            rts=args.rts,
        ) as ser:
            print(
                "hardware UI journey open: "
                f"port={args.port} baud={args.baud} "
                f"dtr={'on' if args.dtr else 'off'} rts={'on' if args.rts else 'off'}",
                file=sys.stderr,
            )
            for idx, step in enumerate(steps, start=1):
                print(f"\n[{idx}/{len(steps)}] {step.prompt}")
                print(f"Monitor duration: {step.duration_s:g}s")
                if not args.yes:
                    input("Press Enter to start this step...")
                raw_path = raw_dir / f"{idx:02d}-{safe_name(step.name)}.raw.log"
                result = monitor_step(ser, step, raw_path, args.echo_serial)
                results.append(result)
                status = "PASS" if result["passed"] else "FAIL"
                print(
                    f"{status} {step.name}: raw_bytes={result['raw_bytes']} "
                    f"events={result['event_count']}"
                )
                if args.screenshot_mode != "none":
                    shot = capture_screenshot(
                        ser,
                        f"{idx:02d}-{step.name}",
                        capture_dir,
                        args.screenshot_command,
                        args.screenshot_timeout,
                    )
                    screenshots.append(shot)
                    if shot["complete"]:
                        print(f"screenshot captured: {shot.get('png') or shot['raw_capture']}")
                    elif args.screenshot_mode == "require":
                        print(f"required screenshot failed for {step.name}", file=sys.stderr)
    except KeyboardInterrupt:
        exit_code = 130
    except serial.SerialException as exc:
        print(f"ERROR: serial open/read failed: {exc}", file=sys.stderr)
        exit_code = 1

    ended = datetime.now().isoformat(timespec="seconds")
    reset_or_crash = any(not bool(result["passed"]) for result in results)
    required_screenshot_failed = (
        args.screenshot_mode == "require"
        and any(not bool(shot["complete"]) for shot in screenshots)
    )
    passed = exit_code == 0 and (args.allow_reset or not reset_or_crash) and not required_screenshot_failed
    if exit_code == 0 and not passed:
        exit_code = 4 if required_screenshot_failed else 3

    report = {
        "started": started,
        "ended": ended,
        "port": args.port,
        "baud": args.baud,
        "dtr": bool(args.dtr),
        "rts": bool(args.rts),
        "profile": args.profile,
        "journey_json": str(args.journey_json) if args.journey_json else None,
        "screenshot_mode": args.screenshot_mode,
        "passed": passed,
        "reset_or_crash_detected": reset_or_crash,
        "step_count": len(results),
        "results": results,
        "screenshots": screenshots,
    }
    report_path = args.out_dir / "summary.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"\nhardware UI journey {'PASS' if passed else 'FAIL'}")
    print(f"summary: {report_path}")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
