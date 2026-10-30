#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "display/lcd_display.h"
#include "mosaico_ui.h"

// Board-lifetime adapter. Serializes every shared view mutation with LVGL's lock.
class MosaicoDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    void SetupUI() override;
    void SetTheme(Theme* theme) override;
    void SetStatus(const char* status) override;
    bool SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* text) override;
    void RemoveChatMessage(const char* role, const char* text) override;
    void ClearChatMessages() override;
    bool PreserveChatOnIdle() const override { return true; }
    void SetHideSubtitle(bool hide) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    bool SnapshotToJpeg(std::string& data, int quality = 80) override;
    void SetHandler(MosaicoUi::Handler handler);
    void OpenCamera(bool available);
    void CloseCamera();
    void CameraBusy(bool busy);
    void CameraHint(const char* text);
    void Motion(float x, float y);
    void SetControls(int volume, int brightness);
    bool RegisterPointer();

private:
    struct Pointer {
        int16_t x;
        int16_t y;
        bool down;
    };
    QueueHandle_t pointer_queue_ = nullptr;
    lv_indev_t* pointer_input_ = nullptr;
    Pointer pointer_{};
    uint32_t pointer_tick_ = 0;
    std::unique_ptr<MosaicoUi> ui_;
    MosaicoUi::Handler handler_;
    std::unique_ptr<LvglImage> camera_frame_;
};
