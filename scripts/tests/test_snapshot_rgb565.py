"""Check the real snapshot method's encoder contract without ESP hardware."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from test_jpeg_fit_scale import function

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main/display/lvgl_display/lvgl_display.cc"


@unittest.skipUnless(shutil.which("c++"), "requires a host C++ compiler")
class SnapshotRgb565Tests(unittest.TestCase):
    def test_native_colors_padded_rows_and_capture_failures(self):
        driver = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#define CONFIG_LV_USE_SNAPSHOT 1
#define ESP_LOGE(...) ((void)0)
constexpr int LV_COLOR_FORMAT_RGB565 = 1;
constexpr int V4L2_PIX_FMT_RGB565 = 2;
struct lv_obj_t {};
struct lv_draw_buf_t {
    uint8_t* data;
    size_t data_size;
    struct { uint32_t w, h, stride; } header;
};
static bool can_lock = true, can_capture = true, destroyed = false;
static uint8_t pixels[12];
static lv_draw_buf_t buffer{pixels, sizeof(pixels), {2, 2, 4}};
static lv_obj_t screen;
static lv_obj_t* lv_screen_active() { return &screen; }
static lv_draw_buf_t* lv_snapshot_take(lv_obj_t*, int) {
    assert(can_lock);
    return can_capture ? &buffer : nullptr;
}
static void lv_draw_buf_destroy(lv_draw_buf_t*) { destroyed = true; }
struct LvglDisplay { bool SnapshotToJpeg(std::string&, int); };
struct DisplayLockGuard {
    explicit DisplayLockGuard(LvglDisplay*) {}
    explicit operator bool() const { return can_lock; }
};
static bool image_to_jpeg_cb(uint8_t* data, size_t, uint16_t w, uint16_t h,
    int format, int quality, size_t (*callback)(void*, size_t, const void*, size_t), void* arg) {
    // RGB565 little-endian red, green, blue, white, in two scanlines.
    const uint8_t expected[] = {0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0xff, 0xff};
    assert(w == 2 && h == 2 && format == V4L2_PIX_FMT_RGB565 && quality == 75);
    assert(std::memcmp(data, expected, sizeof(expected)) == 0);
    const char encoded[] = "jpeg";
    return callback(arg, 0, encoded, 4) == 4;
}
''' + function(SOURCE.read_text(), "bool LvglDisplay::SnapshotToJpeg(") + r'''
int main() {
    LvglDisplay display;
    std::string output;
    for (uint32_t stride : {4U, 6U}) {
        buffer.header.stride = stride;
        std::memset(pixels, 0x55, sizeof(pixels));
        pixels[0] = 0; pixels[1] = 0xf8; pixels[2] = 0xe0; pixels[3] = 0x07;
        pixels[stride] = 0x1f; pixels[stride+1] = 0;
        pixels[stride+2] = 0xff; pixels[stride+3] = 0xff;
        destroyed = false;
        assert(display.SnapshotToJpeg(output, 75));
        assert(output == "jpeg" && destroyed);
    }
    can_capture = false;
    destroyed = false;
    assert(!display.SnapshotToJpeg(output, 75) && !destroyed);
    can_lock = false;
    can_capture = true;
    assert(!display.SnapshotToJpeg(output, 75) && !destroyed);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "snapshot.cc"
            binary = pathlib.Path(tmp) / "snapshot"
            source.write_text(driver)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Werror", str(source),
                            "-o", str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
