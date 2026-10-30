#pragma once
#include <array>
#include <memory>
#include "driver/ppa.h"
#include "esp_video.h"

// Owned by the camera worker. LVGL holds a shared buffer lease until it replaces
// the image under its display lock; the worker only writes unleased buffers.
class MosaicoCameraPreview {
public:
    static constexpr int kWidth = 432;
    static constexpr int kHeight = 288;
    ~MosaicoCameraPreview();
    bool Present(const EspVideo::PreviewFrame& frame);

private:
    bool Initialize();
    ppa_client_handle_t ppa_ = nullptr;
    ppa_srm_oper_config_t operation_{};
    std::array<std::shared_ptr<uint8_t>, 2> buffers_;
};
