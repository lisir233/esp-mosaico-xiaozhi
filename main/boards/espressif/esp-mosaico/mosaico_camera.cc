#include "mosaico_camera.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>

MosaicoCamera::MosaicoCamera(const esp_video_init_config_t& config) : video_(config) {}
void MosaicoCamera::SetExplainUrl(const std::string& url, const std::string& token) {
    std::lock_guard lock(config_gate_);
    url_ = url;
    token_ = token;
}
void MosaicoCamera::ApplyCredentials() {
    std::lock_guard lock(config_gate_);
    video_.SetExplainUrl(url_, token_);
}
bool MosaicoCamera::Capture() {
    preview_ = false;
    std::lock_guard lock(gate_);
    return video_.Capture();
}
bool MosaicoCamera::SetHMirror(bool enabled) {
    std::lock_guard lock(gate_);
    return video_.SetHMirror(enabled);
}
bool MosaicoCamera::SetVFlip(bool enabled) {
    std::lock_guard lock(gate_);
    return video_.SetVFlip(enabled);
}
MosaicoCamera::Result MosaicoCamera::Explain(const std::string& question) {
    std::lock_guard lock(gate_);
    ApplyCredentials();
    return video_.Explain(question);
}
MosaicoCamera::Result MosaicoCamera::CaptureAndExplain(const std::string& question) {
    std::lock_guard lock(gate_);
    {
        std::lock_guard config_lock(config_gate_);
        if (url_.empty())
            return std::unexpected("请先开始一次语音对话，再拍照识图");
    }
    ApplyCredentials();
    if (!video_.Capture())
        return std::unexpected("拍照失败，请检查相机连接");
    return video_.Explain(question);
}
bool MosaicoCamera::Start(std::function<void(Result)> result, std::function<void()> failure,
                          std::function<bool()> preview_allowed) {
    if (worker_ready_)
        return true;
    result_ = std::move(result);
    failure_ = std::move(failure);
    preview_allowed_ = std::move(preview_allowed);
    worker_ready_ = xTaskCreate([](void* arg) { static_cast<MosaicoCamera*>(arg)->Run(); },
                                "camera_view", 16 * 1024, this, 1, nullptr) == pdPASS;
    return worker_ready_;
}
bool MosaicoCamera::Inspect() {
    bool expected = false;
    return Available() && inspect_.compare_exchange_strong(expected, true);
}
void MosaicoCamera::Run() {
    constexpr unsigned kPreviewPeriodMs = 100;
    constexpr int64_t kStatsPeriodUs = 5000000;
    int64_t stats_start = 0, busy_us = 0;
    unsigned frames = 0;
    for (;;) {
        const auto started = esp_timer_get_time();
        if (inspect_.load()) {
            preview_ = false;
            auto result = CaptureAndExplain("请用简洁的中文描述画面中的主要物体和可见文字。");
            if (result_)
                result_(std::move(result));
            inspect_ = false;
        } else if (preview_.load() && (!preview_allowed_ || preview_allowed_())) {
            bool failed = false;
            {
                std::lock_guard lock(gate_);
                if (preview_.load() &&
                    !video_.ReadPreview([this](const EspVideo::PreviewFrame& frame) {
                        return viewfinder_.Present(frame);
                    })) {
                    preview_ = false;
                    failed = true;
                }
            }
            if (failed && failure_)
                failure_();
            if (!failed) {
                const auto now = esp_timer_get_time();
                if (!stats_start)
                    stats_start = started;
                ++frames;
                busy_us += now - started;
                if (now - stats_start >= kStatsPeriodUs) {
                    ESP_LOGI("camera_preview", "fps=%.1f pipeline_ms=%.1f frames=%u",
                             frames * 1000000.0 / (now - stats_start), busy_us / (frames * 1000.0),
                             frames);
                    stats_start = now;
                    frames = 0;
                    busy_us = 0;
                }
            }
        } else {
            stats_start = 0;
            frames = 0;
            busy_us = 0;
        }
        // Count acquisition/conversion in the period, rather than adding a delay
        // after each frame. Always yield, including when a slow sensor misses it.
        const int64_t elapsed_ms = (esp_timer_get_time() - started) / 1000;
        vTaskDelay(
            pdMS_TO_TICKS(elapsed_ms < kPreviewPeriodMs ? kPreviewPeriodMs - elapsed_ms : 1) + 1);
    }
}
