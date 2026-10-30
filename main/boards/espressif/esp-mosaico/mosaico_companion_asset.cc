#include <lvgl.h>
// Generated native BGRA pixels: no runtime PNG decode/cache requirement.
static const uint8_t pixels[] = {
#include "assets/companion-body.inc"
};
extern const lv_image_dsc_t mosaico_companion_body = [] {
    lv_image_dsc_t image{};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = LV_COLOR_FORMAT_ARGB8888;
    image.header.w = 340;
    image.header.h = 290;
    image.header.stride = 340 * 4;
    image.data_size = sizeof(pixels);
    image.data = pixels;
    return image;
}();
