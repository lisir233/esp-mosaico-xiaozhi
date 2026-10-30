"""Fault-inject the actual converter wrapper; no hardware or vendor binary required."""
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
STUBS = Path(__file__).with_name("audio_resampler_stubs")


@unittest.skipUnless(shutil.which("c++"), "requires C++ compiler")
class AudioResamplerTests(unittest.TestCase):
    def test_conversion_failures_bounds_and_guards(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "test"
            build = subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DCONFIG_IDF_TARGET_ESP32S31=1", "-I", str(STUBS),
                "-I", str(ROOT / "main/audio"), str(STUBS / "test.cc"),
                str(ROOT / "main/audio/audio_resampler.cc"), "-Wl,--wrap=malloc",
                "-o", str(binary)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            for mode in ("normal", "overrun", "underrun"):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(binary), mode], capture_output=True,
                                            text=True, timeout=10)
                    self.assertEqual(result.returncode, 0 if mode == "normal" else -signal.SIGABRT,
                                     result.stderr)
                    if mode != "normal":
                        self.assertIn("buffer overrun", result.stderr)
