#!/usr/bin/env python3
"""Fast validation for scripts/merge_bin.py helper behavior.

This does not build firmware. It imports the SCons post-action script as a
normal Python module and tests the pure helper functions used by release
artifact generation.
"""

from __future__ import annotations

import importlib.util
import os
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MERGE_BIN = REPO_ROOT / "scripts" / "merge_bin.py"


def load_merge_bin_module():
    spec = importlib.util.spec_from_file_location("sigurdos_merge_bin", MERGE_BIN)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to import {MERGE_BIN}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeBoardConfig:
    def get(self, key, default=None):
        values = {
            "build.mcu": "esp32s3",
            "upload.flash_size": "16MB",
        }
        return values.get(key, default)


class FakeEnv:
    def __init__(self, project_dir: Path, build_dir: Path, packages_dir: Path):
        self.project_dir = project_dir
        self.build_dir = build_dir
        self.packages_dir = packages_dir
        self.execute_calls = []

    def BoardConfig(self):
        return FakeBoardConfig()

    def subst(self, value: str) -> str:
        replacements = {
            "$BUILD_DIR": str(self.build_dir),
            "${PROGNAME}": "firmware",
            "$PROJECT_DIR": str(self.project_dir),
            "$PROJECT_PACKAGES_DIR": str(self.packages_dir),
            "$BOARD": "lilygo-t-deck",
            "$PIOENV": "SigurdOS_TDeck",
        }
        for needle, replacement in replacements.items():
            value = value.replace(needle, replacement)
        return value

    def Flatten(self, value):
        return list(value)

    def get(self, key, default=None):
        if key == "FLASH_EXTRA_IMAGES":
            return ["0x0000", str(self.build_dir / "bootloader.bin"), "0x8000", str(self.build_dir / "partitions.bin")]
        return default

    def Execute(self, command):
        self.execute_calls.append(command)
        (self.build_dir / "firmware-merged.bin").write_bytes(b"merged-image")
        return 0

    def GetProjectOption(self, key, default=None):
        if key == "board_build.partitions":
            return "default_16MB.csv"
        return default


class MergeBinHelpersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.merge_bin = load_merge_bin_module()

    def test_import_does_not_require_scons(self):
        self.assertTrue(callable(self.merge_bin.merge_bin_action))
        self.assertTrue(callable(self.merge_bin._find_boot_app0))

    def test_find_boot_app0_searches_any_platformio_package_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            packages = root / "packages"
            boot_app0 = packages / "renamed-framework-package" / "tools" / "boot_app0.bin"
            boot_app0.parent.mkdir(parents=True)
            boot_app0.write_bytes(b"boot")

            found = self.merge_bin._find_boot_app0(str(packages), str(root / ".pio" / "build" / "env"))
            self.assertEqual(Path(found), boot_app0)

    def test_copy_webflasher_artifacts_and_validate_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src_dir = root / "src"
            web_dir = root / "webflasher"
            src_dir.mkdir()

            artifacts = {}
            for name in self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS:
                src = src_dir / f"{name}.bin"
                payload_name = "full" if name == "launcher" else name
                src.write_bytes(f"{payload_name}-payload".encode("ascii"))
                artifacts[name] = str(src)

            copied = self.merge_bin._copy_webflasher_artifacts(
                artifacts,
                str(web_dir),
                {"bootloader": "0x0000", "partitions": "0x8000", "boot_app0": "0xe000", "firmware": "0x10000"},
                self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS,
            )
            manifest = {"artifacts": copied}

            self.assertEqual(set(copied), set(self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS))
            manifest["flash_mode"] = "keep"
            self.assertEqual(
                self.merge_bin._validate_manifest_artifacts(
                    manifest,
                    str(web_dir),
                    self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS,
                ),
                [],
            )
            self.assertTrue((web_dir / "SigurdOS-tdeck-launcher.bin").is_file())
            self.assertTrue((web_dir / "sigurdos-tdeck-launcher.bin").is_file())

    def test_required_missing_artifact_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src_dir = root / "src"
            src_dir.mkdir()
            bootloader = src_dir / "bootloader.bin"
            bootloader.write_bytes(b"bootloader")

            with self.assertRaises(RuntimeError):
                self.merge_bin._copy_webflasher_artifacts(
                    {"bootloader": str(bootloader), "boot_app0": ""},
                    str(root / "webflasher"),
                    {},
                    ("bootloader", "boot_app0"),
                )

    def test_merge_post_action_generates_required_manifest(self):
        with tempfile.TemporaryDirectory(prefix="sigurdos build ") as tmp:
            root = Path(tmp)
            project = root / "project"
            build = project / ".pio" / "build" / "SigurdOS_TDeck"
            packages = root / "packages"
            boot_app0 = packages / "framework-renamed" / "tools" / "partitions" / "boot_app0.bin"
            pins_h = project / "src" / "hal" / "tdeck_pins.h"

            build.mkdir(parents=True)
            boot_app0.parent.mkdir(parents=True)
            pins_h.parent.mkdir(parents=True)

            (build / "bootloader.bin").write_bytes(b"bootloader")
            (build / "partitions.bin").write_bytes(b"partitions")
            (build / "firmware.bin").write_bytes(b"firmware")
            boot_app0.write_bytes(b"boot-app0")
            pins_h.write_text('#define SIGURDOS_VERSION "test-version"\n', encoding="utf-8")

            env = FakeEnv(project, build, packages)
            old_ci = os.environ.get("CI")
            os.environ["CI"] = "1"
            try:
                self.merge_bin.merge_bin_action([], [], env)
            finally:
                if old_ci is None:
                    os.environ.pop("CI", None)
                else:
                    os.environ["CI"] = old_ci

            web_dir = project / "webflasher"
            manifest_path = web_dir / "manifest.json"
            self.assertTrue(manifest_path.is_file())

            import json
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["flash_mode"], "keep")
            self.assertEqual(set(manifest["artifacts"]), set(self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS))
            self.assertEqual(manifest["artifacts"]["launcher"]["file"], "SigurdOS-tdeck-launcher.bin")
            self.assertEqual(
                manifest["artifacts"]["full"]["sha256"],
                manifest["artifacts"]["launcher"]["sha256"],
            )
            self.assertEqual(
                self.merge_bin._validate_manifest_artifacts(
                    manifest,
                    str(web_dir),
                    self.merge_bin.REQUIRED_WEBFLASHER_ARTIFACTS,
                ),
                [],
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
