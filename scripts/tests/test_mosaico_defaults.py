"""Exercise the real CMake defaults chain used by direct Mosaico builds."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cmake"), "requires CMake")
class MosaicoDefaultsTests(unittest.TestCase):
    def defaults(self, supplied):
        # Run the project's actual pre-IDF configuration without loading a target
        # toolchain. The common profile defines the CBIN glyph ABI and PNG decoder.
        preamble = (ROOT / "CMakeLists.txt").read_text().split(
            "list(APPEND EXTRA_COMPONENT_DIRS", 1
        )[0]
        preamble = preamble.replace("${CMAKE_CURRENT_LIST_DIR}", str(ROOT))
        script = 'set(SDKCONFIG_DEFAULTS "' + ";".join(supplied) + '")\n'
        script += preamble + '\nmessage(STATUS "DEFAULTS=${SDKCONFIG_DEFAULTS}")\n'
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "defaults.cmake"
            path.write_text(script)
            result = subprocess.run(
                ["cmake", "-P", str(path)], capture_output=True, text=True, check=True
            )
        line = next(line for line in result.stdout.splitlines()
                    if line.startswith("-- DEFAULTS="))
        return line.removeprefix("-- DEFAULTS=").split(";")

    def test_direct_build_loads_common_before_target_and_product(self):
        self.assertEqual(self.defaults([]), [str(ROOT / name) for name in (
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s31", "sdkconfig.mosaico.defaults",
            "cmake/mosaico-retained.defaults"
        )])

    def test_variant_overrides_survive_without_duplicate_profiles(self):
        self.assertEqual(self.defaults([
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s31", "build/variant.defaults"
        ]), [str(ROOT / name) for name in (
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s31",
            "build/variant.defaults", "sdkconfig.mosaico.defaults", "cmake/mosaico-retained.defaults"
        )])

    def test_legacy_explicit_list_still_gets_missing_common_profile(self):
        self.assertEqual(self.defaults([
            str(ROOT / "sdkconfig.defaults.esp32s31"),
            str(ROOT / "sdkconfig.mosaico.defaults")
        ]), [str(ROOT / name) for name in (
            "sdkconfig.defaults", "sdkconfig.defaults.esp32s31", "sdkconfig.mosaico.defaults",
            "cmake/mosaico-retained.defaults"
        )])


if __name__ == "__main__":
    unittest.main()
