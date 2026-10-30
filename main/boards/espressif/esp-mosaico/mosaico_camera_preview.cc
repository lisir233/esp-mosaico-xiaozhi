#include "mosaico_camera_preview.h"
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include "board.h"
#include "display.h"
#include "lvgl_image.h"

namespace {
constexpr char kTag[] = "camera_preview";
constexpr size_t kAlignment = 128;
constexpr size_t kBufferSize = MosaicoCameraPreview::kWidth * MosaicoCameraPreview::kHeight * 2;
// Exact 12/16 hardware scale: avoid PPA's fractional rounding changing geometry.
constexpr float kScale = 0.75f;
constexpr unsigned kCropWidth = MosaicoCameraPreview::kHeight * 4 / 3;
constexpr unsigned kCropHeight = MosaicoCameraPreview::kWidth * 4 / 3;

class PreviewImage final : public LvglImage {
public:
    explicit PreviewImage(std::shared_ptr<uint8_t> pixels) : pixels_(std::move(pixels)) {
        descriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
        descriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
        descriptor_.header.w = MosaicoCameraPreview::kWidth;
        descriptor_.header.h = MosaicoCameraPreview::kHeight;
        descriptor_.header.stride = MosaicoCameraPreview::kWidth * 2;
        descriptor_.data_size = kBufferSize;
        descriptor_.data = pixels_.get();
    }
    const lv_image_dsc_t* image_dsc() const override { return &descriptor_; }

private:
    std::shared_ptr<uint8_t> pixels_;
    lv_image_dsc_t descriptor_{};
};
}  // namespace

MosaicoCameraPreview::~MosaicoCameraPreview() {
    if (ppa_)
        ppa_unregister_client(ppa_);
}

bool MosaicoCameraPreview::Initialize() {
    if (ppa_)
        return true;
    for (auto& buffer : buffers_) {
        buffer.reset(
            static_cast<uint8_t*>(heap_caps_aligned_calloc(
                kAlignment, 1, kBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)),
            heap_caps_free);
        if (!buffer) {
            buffers_ = {};
            return false;
        }
    }
    ppa_client_config_t config{};
    config.oper_type = PPA_OPERATION_SRM;
    config.max_pending_trans_num = 1;
    const auto err = ppa_register_client(&config, &ppa_);
    if (err != ESP_OK) {
        buffers_ = {};
        ESP_LOGE(kTag, "PPA initialization: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(kTag, "preview=%dx%d RGB565, left-slot rotation, two reusable buffers", kWidth,
             kHeight);
    return true;
}

bool MosaicoCameraPreview::Present(const EspVideo::PreviewFrame& frame) {
    const size_t stride = frame.stride ? frame.stride : frame.width * 2;
    if (!frame.data || frame.width < kCropWidth || frame.height < kCropHeight ||
        stride < frame.width * 2U || stride % 2 || frame.size < stride * frame.height)
        return false;
    ppa_srm_color_mode_t format;
    switch (frame.format) {
        case V4L2_PIX_FMT_UYVY:
            format = PPA_SRM_COLOR_MODE_YUV422_UYVY;
            break;
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YUV422P:
            format = PPA_SRM_COLOR_MODE_YUV422_YUYV;
            break;
        case V4L2_PIX_FMT_RGB565:
            format = PPA_SRM_COLOR_MODE_RGB565;
            break;
        default:
            ESP_LOGE(kTag, "unsupported preview format 0x%08lx", frame.format);
            return false;
    }
    if (!Initialize())
        return false;
    for (auto& buffer : buffers_) {
        if (buffer.use_count() != 1)
            continue;
        auto err = esp_cache_msync(const_cast<uint8_t*>(frame.data), frame.size,
                                   ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
        if (err == ESP_OK)
            err = esp_cache_msync(buffer.get(), kBufferSize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        if (err != ESP_OK)
            return false;
        // Same physical left-slot transform as BSP camera_lcd_preview. Crop in
        // sensor coordinates before rotating; scale both axes equally.
        operation_ = {};
        auto& operation = operation_;
        operation.in.buffer = frame.data;
        operation.in.pic_w = stride / 2;
        operation.in.pic_h = frame.height;
        operation.in.block_w = kCropWidth;
        operation.in.block_h = kCropHeight;
        operation.in.block_offset_x = ((frame.width - kCropWidth) / 2) & ~1U;
        operation.in.block_offset_y = ((frame.height - kCropHeight) / 2) & ~1U;
        operation.in.srm_cm = format;
        operation.in.yuv_range = PPA_COLOR_RANGE_LIMIT;
        operation.in.yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601;
        operation.out.buffer = buffer.get();
        operation.out.buffer_size = kBufferSize;
        operation.out.pic_w = kWidth;
        operation.out.pic_h = kHeight;
        operation.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
        operation.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
        operation.scale_x = kScale;
        operation.scale_y = kScale;
        operation.mirror_y = true;
        operation.mode = PPA_TRANS_MODE_BLOCKING;
        err = ppa_do_scale_rotate_mirror(ppa_, &operation);
        if (err == ESP_OK)
            err = esp_cache_msync(buffer.get(), kBufferSize,
                                  ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "preview conversion: %s", esp_err_to_name(err));
            return false;
        }
        auto* display = Board::GetInstance().GetDisplay();
        if (display)
            display->SetPreviewImage(std::make_unique<PreviewImage>(buffer));
        return true;
    }
    // Keep latency bounded if presentation still holds both buffers.
    return true;
}
