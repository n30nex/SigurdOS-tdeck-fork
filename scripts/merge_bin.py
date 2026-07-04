#!/usr/bin/env python3
"""Post-build: merge bootloader + partitions + firmware into a single flashable image.
Patterned after MeshCore merge-bin.py — reads flash config from board JSON."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


REQUIRED_WEBFLASHER_ARTIFACTS = (
    "bootloader",
    "partitions",
    "boot_app0",
    "firmware",
    "full",
    "launcher",
)


def _truthy(value: str | None) -> bool:
    return value is not None and value.lower() not in ("", "0", "false", "no", "off")


def _webflasher_required(pioenv: str) -> bool:
    if _truthy(os.environ.get("SIGURDOS_ALLOW_PARTIAL_WEBFLASHER")):
        return False
    if _truthy(os.environ.get("SIGURDOS_REQUIRE_WEBFLASHER_ARTIFACTS")):
        return True
    return _truthy(os.environ.get("CI")) and pioenv == "SigurdOS_TDeck"


def _find_boot_app0(packages_dir: str, build_dir: str) -> str:
    """Find boot_app0.bin without assuming a specific PlatformIO package name."""
    build_path = Path(build_dir)
    candidates = [
        build_path / "boot_app0.bin",
        build_path.parent / "SigurdOS_TDeck" / "boot_app0.bin",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)

    packages_path = Path(packages_dir)
    if not packages_path.is_dir():
        return ""

    for candidate in packages_path.rglob("boot_app0.bin"):
        if candidate.is_file():
            return str(candidate)
    return ""


def _sha256(path: str | Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _copy_file(src: str | Path, dst: str | Path) -> None:
    Path(dst).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)


def _webflasher_dst_name(name: str) -> str:
    if name == "launcher":
        return "SigurdOS-tdeck-launcher.bin"
    return f"sigurdos-tdeck-{name}.bin"


def _copy_webflasher_artifacts(
    artifacts: dict[str, str],
    web_dir: str,
    offsets: dict[str, str],
    required_names: tuple[str, ...] = (),
) -> dict[str, dict[str, object]]:
    copied: dict[str, dict[str, object]] = {}
    missing: list[str] = []
    web_path = Path(web_dir)
    web_path.mkdir(parents=True, exist_ok=True)

    for name, src in artifacts.items():
        if src and Path(src).is_file():
            dst_name = _webflasher_dst_name(name)
            dst = web_path / dst_name
            _copy_file(src, dst)
            if name == "launcher":
                compat_dst = web_path / "sigurdos-tdeck-launcher.bin"
                _copy_file(src, compat_dst)
                print(f"SigurdOS webflasher: sigurdos-tdeck-launcher.bin ({compat_dst.stat().st_size:,} bytes)")
            size = dst.stat().st_size
            copied[name] = {
                "file": dst_name,
                "size": size,
                "sha256": _sha256(dst),
                "offset": offsets.get(name, "0x0"),
            }
            print(f"SigurdOS webflasher: {dst_name} ({size:,} bytes)")
        else:
            missing.append(name)
            print(f"SigurdOS webflasher: SKIP {name} - not found: {src}")

    required_missing = [name for name in required_names if name not in copied]
    if required_missing:
        raise RuntimeError(
            "SigurdOS webflasher: required artifact(s) missing: "
            + ", ".join(required_missing)
        )
    return copied


def _validate_manifest_artifacts(
    manifest: dict,
    web_dir: str,
    required_names: tuple[str, ...] = (),
) -> list[str]:
    errors: list[str] = []
    if manifest.get("flash_mode") != "keep":
        errors.append("flash_mode: expected keep")
    for name in required_names:
        if name not in manifest.get("artifacts", {}):
            errors.append(f"{name}: missing required manifest artifact")
    for name, meta in manifest.get("artifacts", {}).items():
        filename = meta.get("file")
        if not filename:
            errors.append(f"{name}: missing file field")
            continue
        path = Path(web_dir) / str(filename)
        if not path.is_file():
            errors.append(f"{name}: file does not exist: {filename}")
            continue
        size = path.stat().st_size
        if int(meta.get("size", -1)) != size:
            errors.append(f"{name}: size mismatch")
        if str(meta.get("sha256", "")) != _sha256(path):
            errors.append(f"{name}: sha256 mismatch")
    artifacts = manifest.get("artifacts", {})
    full = artifacts.get("full", {})
    launcher = artifacts.get("launcher", {})
    if full and launcher and full.get("sha256") != launcher.get("sha256"):
        errors.append("launcher: sha256 differs from full image")
    return errors


def _git(args, cwd):
    try:
        completed = subprocess.run(
            ["git", *args],
            cwd=cwd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    value = completed.stdout.strip()
    return value if value else "unknown"


def _git_dirty(cwd):
    try:
        completed = subprocess.run(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=cwd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return True
    return bool(completed.stdout.strip())

def merge_bin_action(target, source, env):
    board_config = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged_bin = env.subst("$BUILD_DIR/${PROGNAME}-merged.bin")
    firmware_bin = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    pioenv = env.subst("$PIOENV")
    webflasher_required = _webflasher_required(pioenv)

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")

    for f in [bootloader, partitions, firmware_bin]:
        if not os.path.isfile(f):
            msg = f"SigurdOS: skipping merge - missing: {f}"
            print(msg)
            if webflasher_required:
                raise RuntimeError(msg)
            return

    flash_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        "$ESP32_APP_OFFSET",
        firmware_bin,
    ]

    merge_cmd = " ".join([
        '"$PYTHONEXE"',
        '"$OBJCOPY"',
        "--chip",
        board_config.get("build.mcu", "esp32s3"),
        "merge_bin",
        "-o", merged_bin,
        "--flash_mode",
        "keep",  # Preserve bootloader's DIO mode — ROM requires DIO at boot
        "--flash_freq",
        "${__get_board_f_flash(__env__)}",
        "--flash_size",
        board_config.get("upload.flash_size", "16MB"),
        *flash_images,
    ])

    print(f"SigurdOS: merging firmware ({board_config.get('build.mcu', 'esp32s3')})...")
    merge_rc = env.Execute(merge_cmd)
    if merge_rc not in (None, 0):
        raise RuntimeError(f"SigurdOS: merge_bin failed with exit code {merge_rc}")

    launcher_bin = os.path.join(build_dir, "SigurdOS-tdeck-launcher.bin")
    if os.path.isfile(merged_bin):
        print(f"SigurdOS: merged -> firmware-merged.bin ({os.path.getsize(merged_bin):,} bytes)")
        # Launcher-compatible copy (same bytes as merged, Launcher-optimized name)
        _copy_file(merged_bin, launcher_bin)
        print(f"SigurdOS: launcher -> SigurdOS-tdeck-launcher.bin ({os.path.getsize(launcher_bin):,} bytes)")

    # Web flasher manifest

    proj_dir = env.subst("$PROJECT_DIR")
    web_dir = os.path.join(proj_dir, "webflasher")
    os.makedirs(web_dir, exist_ok=True)

    mcu = board_config.get("build.mcu", "esp32s3")
    flash_mode = "keep"  # Don't let esptool.js rewrite bootloader header — ESP32-S3 ROM boots DIO
    flash_size = board_config.get("upload.flash_size", "16MB")

    offsets = {
        "bootloader": "0x0000",
        "partitions": "0x8000",
        "boot_app0": "0xe000",
        "firmware":  "0x10000",
    }

    boot_app0_src = _find_boot_app0(env.subst("$PROJECT_PACKAGES_DIR"), build_dir)

    artifacts = {
        "bootloader": bootloader,
        "partitions": partitions,
        "boot_app0": boot_app0_src,
        "firmware":  firmware_bin,
        "full":      merged_bin,
        "launcher":  launcher_bin,
    }

    # Read version from tdeck_pins.h
    pins_h = os.path.join(proj_dir, "src", "hal", "tdeck_pins.h")
    version = "unknown"
    try:
        with open(pins_h, encoding="utf-8", errors="replace") as f:
            for line in f:
                if "SIGURDOS_VERSION" in line and '"' in line:
                    version = line.split('"')[1]
                    break
    except: pass

    git_sha = _git(["rev-parse", "--short=12", "HEAD"], proj_dir)
    meshcore_sha = _git(["rev-parse", "--short=12", "HEAD"], os.path.join(proj_dir, "lib", "meshcore"))

    manifest = {
        "name": "SigurdOS T-Deck",
        "board": "LilyGo T-Deck",
        "platformio_board": env.subst("$BOARD"),
        "mcu": mcu,
        "firmware_version": version,
        "git_sha": git_sha,
        "git_dirty": _git_dirty(proj_dir),
        "meshcore_sha": meshcore_sha,
        "build_environment": pioenv,
        "partition_table": env.GetProjectOption("board_build.partitions", "unknown"),
        "built_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "chip_family": "ESP32-S3",
        "flash_mode": flash_mode,
        "flash_size": flash_size,
        "artifacts": {},
        "flash_offsets": offsets,
    }

    required = REQUIRED_WEBFLASHER_ARTIFACTS if webflasher_required else ()
    manifest["artifacts"] = _copy_webflasher_artifacts(artifacts, web_dir, offsets, required)
    manifest_errors = _validate_manifest_artifacts(manifest, web_dir, required)
    if manifest_errors:
        raise RuntimeError("SigurdOS webflasher manifest invalid: " + "; ".join(manifest_errors))

    manifest_path = os.path.join(web_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"SigurdOS webflasher: manifest written")

if "Import" in globals():
    Import("env")
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
