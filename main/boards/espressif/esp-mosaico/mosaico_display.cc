#include "mosaico_display.h"
#include "mosaico_display_alignment.h"
#include <esp_iris.h>
#include <esp_iris_service_profiles.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <material_symbols.h>
#include <noto_emoji.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <cstring>
#include "application.h"
#include "config.h"
#include "lvgl_theme.h"
#include "settings.h"

void MosaicoDisplay::SetupUI() {
    if (ui_)
        return;
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    lv_display_add_event_cb(display_, MosaicoRoundDisplayArea, LV_EVENT_INVALIDATE_AREA, nullptr);
    SpiLcdDisplay::SetupUI();
    Settings settings("display", false);
    if (settings.GetString("theme").empty()) {
        auto* theme = LvglThemeManager::GetInstance().GetTheme("dark");
        if (theme)
            SetTheme(theme);
    }
    auto* theme = static_cast<LvglTheme*>(current_theme_);
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    ui_ = std::make_unique<MosaicoUi>(lv_screen_active(), theme->text_font()->font());
    ui_->BindStatus(status_bar_);
    ui_->SetHandler(handler_);
    ui_->ApplyTheme(theme->name() == "dark");
    ui_->BindHeader(top_bar_);
    // Existing low-battery notifications remain above the custom content.
    lv_obj_move_to_index(low_battery_popup_, -1);
}
void MosaicoDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    SpiLcdDisplay::SetTheme(theme);
    if (ui_) {
        ui_->SetFont(static_cast<LvglTheme*>(theme)->text_font()->font());
        ui_->ApplyTheme(theme->name() == "dark");
    }
}
void MosaicoDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    LvglDisplay::SetStatus(status);
    if (!ui_)
        return;
    const auto state = Application::GetInstance().GetDeviceState();
    if (state == kDeviceStateIdle) {
        ui_->AddMessage("system", "");
        lv_label_set_text(status_label_, "轻触我，或说「你好小智」");
    }
    ui_->SetState(state == kDeviceStateListening ? MosaicoUi::State::Listening
                  : state == kDeviceStateSpeaking || state == kDeviceStateNotifying
                      ? MosaicoUi::State::Speaking
                  : state == kDeviceStateIdle ? MosaicoUi::State::Idle
                                              : MosaicoUi::State::Busy);
}
bool MosaicoDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    if (!lock || !ui_ || !emotion || !*emotion ||
        (!noto_emoji_get_utf8(emotion) && !material_symbols_get_utf8(emotion)))
        return false;
    ui_->SetEmotion(emotion);
    return true;
}
void MosaicoDisplay::SetChatMessage(const char* role, const char* text) {
    if (role && !std::strcmp(role, "system") &&
        Application::GetInstance().GetDeviceState() == kDeviceStateStarting)
        return;
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->AddMessage(role, text);
}
void MosaicoDisplay::RemoveChatMessage(const char* role, const char* text) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->RemoveMessage(role, text);
}
void MosaicoDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->ClearMessages();
}
void MosaicoDisplay::SetHideSubtitle(bool hide) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->SetSubtitlesHidden(hide);
}
void MosaicoDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (!lock || !ui_ || !ui_->CameraOpen())
        return;
    // Detach before releasing a descriptor which LVGL may have cached.
    ui_->SetCameraImage(nullptr);
    if (camera_frame_)
        lv_image_cache_drop(camera_frame_->image_dsc());
    camera_frame_ = std::move(image);
    if (camera_frame_)
        ui_->SetCameraImage(camera_frame_->image_dsc());
}
void MosaicoDisplay::SetHandler(MosaicoUi::Handler handler) {
    DisplayLockGuard lock(this);
    if (!lock)
        return;
    handler_ = std::move(handler);
    if (ui_)
        ui_->SetHandler(handler_);
}
void MosaicoDisplay::OpenCamera(bool available) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->OpenCamera(available);
}
void MosaicoDisplay::CloseCamera() {
    DisplayLockGuard lock(this);
    if (lock && ui_) {
        ui_->SetCameraImage(nullptr);
        if (camera_frame_)
            lv_image_cache_drop(camera_frame_->image_dsc());
        camera_frame_.reset();
        ui_->CloseCamera();
    }
}
void MosaicoDisplay::CameraBusy(bool busy) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->SetCameraBusy(busy);
}
void MosaicoDisplay::CameraHint(const char* text) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->SetCameraHint(text);
}
void MosaicoDisplay::Motion(float x, float y) {
    // Sensor sampling must not wait behind a screenshot or a long render.
    if (!Lock(1))
        return;
    if (ui_)
        ui_->SetTilt(x, y);
    Unlock();
}
void MosaicoDisplay::SetControls(int volume, int brightness) {
    DisplayLockGuard lock(this);
    if (lock && ui_)
        ui_->SetControls(volume, brightness);
}

bool MosaicoDisplay::RegisterPointer() {
    DisplayLockGuard lock(this);
    if (!lock)
        return false;
    pointer_queue_ = xQueueCreate(16, sizeof(Pointer));
    if (!pointer_queue_)
        return false;
    pointer_input_ = lv_indev_create();
    if (!pointer_input_) {
        vQueueDelete(pointer_queue_);
        pointer_queue_ = nullptr;
        return false;
    }
    lv_indev_set_type(pointer_input_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(pointer_input_, display_);
    lv_indev_set_user_data(pointer_input_, this);
    lv_indev_set_read_cb(pointer_input_, [](lv_indev_t* indev, lv_indev_data_t* data) {
        auto* self = static_cast<MosaicoDisplay*>(lv_indev_get_user_data(indev));
        if (xQueueReceive(self->pointer_queue_, &self->pointer_, 0) == pdTRUE)
            self->pointer_tick_ = lv_tick_get();
        if (lv_tick_elaps(self->pointer_tick_) > 2000)
            self->pointer_.down = false;
        data->point = {self->pointer_.x, self->pointer_.y};
        data->state = self->pointer_.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        data->continue_reading = uxQueueMessagesWaiting(self->pointer_queue_) > 0;
    });
    const auto err = esp_iris_rpc_register(
        ESP_IRIS_POINTER_SERVICE_ID, ESP_IRIS_POINTER_METHOD_ID,
        [](const esp_iris_rpc_request_t* request, uint8_t* response, size_t capacity, size_t* size,
           void* context) -> esp_err_t {
            if (request->payload_size != ESP_IRIS_POINTER_MESSAGE_SIZE ||
                capacity < ESP_IRIS_POINTER_MESSAGE_SIZE)
                return ESP_ERR_INVALID_SIZE;
            const auto* p = request->payload;
            const int16_t x = static_cast<int16_t>(p[2] | (p[3] << 8));
            const int16_t y = static_cast<int16_t>(p[4] | (p[5] << 8));
            if (p[0] > 3 || p[1] != 0 || p[6] != 0 || p[7] != 0 || x < 0 || y < 0 || x >= 480 ||
                y >= 480)
                return ESP_ERR_INVALID_ARG;
            Pointer point{x, y, p[0] < 2};
            auto* self = static_cast<MosaicoDisplay*>(context);
            if (xQueueSend(self->pointer_queue_, &point, 0) != pdTRUE)
                return ESP_ERR_NO_MEM;
            std::memcpy(response, p, ESP_IRIS_POINTER_MESSAGE_SIZE);
            *size = ESP_IRIS_POINTER_MESSAGE_SIZE;
            return ESP_OK;
        },
        this);
    if (err != ESP_OK) {
        lv_indev_delete(pointer_input_);
        pointer_input_ = nullptr;
        vQueueDelete(pointer_queue_);
        pointer_queue_ = nullptr;
        return false;
    }
    return true;
}

bool MosaicoDisplay::SnapshotToJpeg(std::string& data, int quality) {
    // LVGL rendering cannot run on the small ESP-Iris protocol stack.
    struct Request {
        MosaicoDisplay* display;
        std::string* data;
        int quality;
        SemaphoreHandle_t done;
        bool success;
    } request{this, &data, quality, xSemaphoreCreateBinary(), false};
    if (!request.done)
        return false;
    if (xTaskCreate(
            [](void* arg) {
                auto* r = static_cast<Request*>(arg);
                r->success = r->display->SpiLcdDisplay::SnapshotToJpeg(*r->data, r->quality);
                const auto done = r->done;
                xSemaphoreGive(done);
                vTaskDelete(nullptr);
            },
            "ui_snapshot", 16 * 1024, &request, 1, nullptr) != pdPASS) {
        vSemaphoreDelete(request.done);
        return false;
    }
    xSemaphoreTake(request.done, portMAX_DELAY);
    vSemaphoreDelete(request.done);
    return request.success;
}
