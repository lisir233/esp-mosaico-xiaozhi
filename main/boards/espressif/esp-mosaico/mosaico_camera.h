#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include "esp_video.h"
#include "mosaico_camera_preview.h"

// Board-lifetime service. The worker owns preview/inspection scheduling; gate_
// serializes all sensor/frame operations, including MCP's capture+explain pair.
class MosaicoCamera : public Camera {
public:
    using Result = std::expected<std::string, std::string>;
    explicit MosaicoCamera(const esp_video_init_config_t& config);
    void SetExplainUrl(const std::string& url, const std::string& token) override;
    bool Capture() override;
    bool SetHMirror(bool enabled) override;
    bool SetVFlip(bool enabled) override;
    Result Explain(const std::string& question) override;
    Result CaptureAndExplain(const std::string& question) override;
    bool Start(std::function<void(Result)> result, std::function<void()> failure,
               std::function<bool()> preview_allowed = {});
    void Preview(bool enabled) { preview_.store(enabled); }
    bool Inspect();
    bool Available() const { return video_.IsInitialized() && worker_ready_; }

private:
    EspVideo video_;
    MosaicoCameraPreview viewfinder_;
    std::mutex gate_, config_gate_;
    std::string url_, token_;
    std::atomic<bool> preview_{false}, inspect_{false};
    bool worker_ready_ = false;
    std::function<void(Result)> result_;
    std::function<void()> failure_;
    std::function<bool()> preview_allowed_;
    void Run();
    void ApplyCredentials();
    bool CaptureFrame();
};
