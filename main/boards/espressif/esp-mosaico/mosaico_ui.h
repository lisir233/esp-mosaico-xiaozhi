#pragma once
#include <lvgl.h>
#include <functional>
#include "mosaico_ui_model.h"

// Shared native view. Caller holds the LVGL lock; services and fonts outlive it.
class MosaicoUi {
public:
    enum class Action {
        Chat,
        OpenCamera,
        CloseCamera,
        Inspect,
        Ask,
        Volume,
        Brightness,
        Feedback,
        ConfigureWifi
    };
    enum class State { Idle, Listening, Speaking, Busy };
    using Handler = std::function<void(Action, int)>;
    MosaicoUi(lv_obj_t* screen, const lv_font_t* font);
    ~MosaicoUi();
    void SetHandler(Handler handler) { handler_ = std::move(handler); }
    void BindStatus(lv_obj_t* bar);
    void BindHeader(lv_obj_t* bar);
    void SetFont(const lv_font_t* font);
    void SetState(State state);
    void SetEmotion(const char* emotion);
    void SetTilt(float x, float y);
    void AddMessage(const char* role, const char* text);
    void RemoveMessage(const char* role, const char* text);
    void ClearMessages();
    void SetSubtitlesHidden(bool hidden);
    void OpenCamera(bool available);
    void CloseCamera();
    void SetCameraBusy(bool busy);
    void SetCameraHint(const char* hint);
    void SetCameraImage(const lv_image_dsc_t* image);
    bool CameraOpen() const { return camera_open_; }
    void SetControls(int volume, int brightness);
    void ApplyTheme(bool dark);
    lv_obj_t* root() const { return root_; }

private:
    lv_obj_t *root_, *face_, *body_, *left_eye_, *right_eye_, *mouth_, *greeting_;
    lv_obj_t *left_smile_, *right_smile_;
    lv_obj_t *question_, *answer_box_, *answer_, *history_page_, *history_;
    lv_obj_t *chat_button_, *chat_label_, *latest_, *settings_, *volume_, *brightness_;
    lv_obj_t *camera_page_, *camera_image_, *camera_hint_, *inspect_, *ask_;
    lv_obj_t *status_bar_ = nullptr, *header_bar_ = nullptr;
    lv_timer_t* animation_ = nullptr;
    lv_font_t small_font_{};
    mosaico::History messages_;
    Handler handler_;
    State state_ = State::Idle;
    bool follow_ = true, rebuilding_ = false, subtitles_hidden_ = false;
    bool camera_open_ = false, camera_available_ = false, camera_busy_ = false;
    bool compact_ = false, happy_ = false, sleepy_ = false, surprised_ = false;
    int face_y_ = 66;
    float tilt_x_ = 0, tilt_y_ = 0;
    lv_point_t press_point_{};
    lv_obj_t* Button(lv_obj_t* parent, const char* text, int x, int y, int w, Action action);
    void Emit(Action action, int value = 0);
    void RebuildHistory();
    void Layout();
    void Animate();
    void OpenSettings();
    static void ButtonEvent(lv_event_t* event);
    static void ScrollEvent(lv_event_t* event);
};
