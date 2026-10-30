#include "mosaico_ui.h"
#include <algorithm>
#include <cmath>
#include <cstring>

LV_IMAGE_DECLARE(mosaico_companion_body);
LV_FONT_DECLARE(mosaico_ui_small);
namespace {
constexpr uint32_t kBackground = 0xf7f6f9, kInk = 0x38393a, kMuted = 0x89848a;
constexpr uint32_t kAccent = 0xff4c01, kCamera = 0x1a1c20;
void Box(lv_obj_t* obj, int x, int y, int w, int h, uint32_t color, int radius = 18) {
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
void Visible(lv_obj_t* obj, bool visible) {
    if (visible)
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t* Label(lv_obj_t* parent, const char* text, int x, int y, int w) {
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}
void Ink(lv_obj_t* obj, uint32_t color) {
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}
void LineBox(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius) {
    auto* obj = lv_obj_create(parent);
    Box(obj, x, y, w, h, color, radius);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}
// Small vector controls, independent of font/icon package versions.
void Icon(lv_obj_t* button, bool camera) {
    auto* icon = lv_obj_create(button);
    Box(icon, 14, 14, 24, 24, kBackground, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    LineBox(icon, 2, 5, 20, 17, kMuted, camera ? 3 : 7);
    if (camera) {
        LineBox(icon, 8, 9, 8, 8, kMuted, 4);
        LineBox(icon, 8, 2, 8, 5, kMuted, 2);
    } else {
        LineBox(icon, 7, 10, 11, 2, kMuted, 0);
        LineBox(icon, 7, 15, 7, 2, kMuted, 0);
    }
}
lv_obj_t* Arc(lv_obj_t* parent, int start, int end, uint32_t color) {
    auto* obj = lv_arc_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_arc_color(obj, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(obj, true, LV_PART_INDICATOR);
    lv_arc_set_angles(obj, start, end);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}
}  // namespace

MosaicoUi::MosaicoUi(lv_obj_t* screen, const lv_font_t* font) {
    root_ = lv_obj_create(screen);
    Box(root_, 0, 0, 480, 480, kBackground, 0);
    small_font_ = mosaico_ui_small;
    SetFont(font);
    Ink(root_, kInk);
    auto* dot = lv_obj_create(root_);
    Box(dot, 38, 39, 6, 6, kAccent, 3);
    auto* title = Label(root_, "小智", 53, 24, 110);
    lv_obj_set_style_text_font(title, &small_font_, 0);
    // Pull down anywhere in the header (or tap its title) for quick settings.
    auto* header_touch = lv_obj_create(root_);
    Box(header_touch, 0, 0, 480, 70, kBackground, 0);
    lv_obj_set_style_bg_opa(header_touch, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(
        header_touch,
        [](lv_event_t* e) {
            auto* self = static_cast<MosaicoUi*>(lv_event_get_user_data(e));
            auto* input = lv_indev_active();
            if (!input)
                return;
            lv_point_t point;
            lv_indev_get_point(input, &point);
            if (lv_event_get_code(e) == LV_EVENT_PRESSED)
                self->press_point_ = point;
            else if (lv_event_get_code(e) == LV_EVENT_RELEASED &&
                     (point.y - self->press_point_.y > 35 || point.x < 180))
                self->OpenSettings();
        },
        LV_EVENT_ALL, this);

    face_ = lv_obj_create(root_);
    Box(face_, 70, 66, 340, 290, kBackground, 0);
    lv_obj_set_style_bg_opa(face_, LV_OPA_TRANSP, 0);
    body_ = lv_image_create(face_);
    lv_image_set_src(body_, &mosaico_companion_body);
    lv_image_set_pivot(body_, 0, 0);
    left_eye_ = lv_obj_create(face_);
    right_eye_ = lv_obj_create(face_);
    for (auto* eye : {left_eye_, right_eye_}) {
        Box(eye, 0, 0, 24, 38, 0x503c33, LV_RADIUS_CIRCLE);
        lv_obj_remove_flag(eye, LV_OBJ_FLAG_CLICKABLE);
        auto* glint = lv_obj_create(eye);
        Box(glint, 5, 6, 6, 10, 0xfff9f1, LV_RADIUS_CIRCLE);
        lv_obj_remove_flag(glint, LV_OBJ_FLAG_CLICKABLE);
    }
    left_smile_ = Arc(face_, 200, 340, 0x503c33);
    right_smile_ = Arc(face_, 200, 340, 0x503c33);
    mouth_ = Arc(face_, 20, 160, 0x684839);
    lv_obj_add_event_cb(
        face_,
        [](lv_event_t* e) {
            static_cast<MosaicoUi*>(lv_event_get_user_data(e))->Emit(Action::Chat);
        },
        LV_EVENT_CLICKED, this);
    greeting_ = Label(root_, "我在，慢慢说。", 30, 320, 420);
    lv_obj_set_style_text_align(greeting_, LV_TEXT_ALIGN_CENTER, 0);
    question_ = Label(root_, "", 38, 248, 404);
    Ink(question_, kMuted);
    lv_label_set_long_mode(question_, LV_LABEL_LONG_DOT);
    answer_box_ = lv_obj_create(root_);
    Box(answer_box_, 36, 290, 408, 110, kBackground, 0);
    lv_obj_add_flag(answer_box_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(answer_box_, LV_DIR_VER);
    answer_ = Label(answer_box_, "", 0, 0, 398);
    lv_obj_set_style_text_line_space(answer_, 8, 0);

    auto* camera = Button(root_, "", 62, 408, 52, Action::OpenCamera);
    Icon(camera, true);
    chat_button_ = Button(root_, "聊一聊", 168, 408, 144, Action::Chat);
    chat_label_ = lv_obj_get_child(chat_button_, 0);
    lv_obj_set_style_bg_color(chat_button_, lv_color_hex(kAccent), 0);
    Ink(chat_button_, 0xffffff);
    lv_obj_align(chat_label_, LV_ALIGN_CENTER, 12, 0);
    LineBox(chat_button_, 25, 14, 8, 15, 0xffffff, 5);
    auto* mic = Arc(chat_button_, 0, 180, 0xffffff);
    lv_obj_set_pos(mic, 21, 15);
    lv_obj_set_size(mic, 16, 20);
    lv_obj_set_style_arc_width(mic, 2, LV_PART_INDICATOR);
    LineBox(chat_button_, 28, 34, 2, 5, 0xffffff, 0);
    auto* history_button = Button(root_, "", 366, 408, 52, Action::Feedback);
    Icon(history_button, false);
    lv_obj_remove_event_cb(history_button, ButtonEvent);
    lv_obj_add_event_cb(
        history_button,
        [](lv_event_t* e) {
            auto* self = static_cast<MosaicoUi*>(lv_event_get_user_data(e));
            Visible(self->history_page_, true);
            self->Emit(Action::Feedback);
        },
        LV_EVENT_CLICKED, this);

    history_page_ = lv_obj_create(root_);
    Box(history_page_, 0, 0, 480, 480, kBackground, 0);
    Label(history_page_, "我们的对话", 32, 24, 416);
    history_ = lv_obj_create(history_page_);
    Box(history_, 24, 78, 432, 318, kBackground, 0);
    lv_obj_add_flag(history_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(history_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(history_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(history_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(history_, 18, 0);
    lv_obj_add_event_cb(history_, ScrollEvent, LV_EVENT_SCROLL_BEGIN, this);
    lv_obj_add_event_cb(history_, ScrollEvent, LV_EVENT_SCROLL_END, this);
    auto* back = Button(history_page_, "返回", 32, 412, 416, Action::Feedback);
    lv_obj_remove_event_cb(back, ButtonEvent);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* e) {
            Visible(static_cast<MosaicoUi*>(lv_event_get_user_data(e))->history_page_, false);
        },
        LV_EVENT_CLICKED, this);
    latest_ = Button(history_page_, "最新 ↓", 310, 344, 130, Action::Feedback);
    lv_obj_remove_event_cb(latest_, ButtonEvent);
    lv_obj_add_event_cb(
        latest_,
        [](lv_event_t* e) {
            auto* self = static_cast<MosaicoUi*>(lv_event_get_user_data(e));
            self->follow_ = true;
            lv_obj_scroll_to_y(self->history_, LV_COORD_MAX, LV_ANIM_OFF);
            Visible(self->latest_, false);
        },
        LV_EVENT_CLICKED, this);
    Visible(latest_, false);
    Visible(history_page_, false);

    settings_ = lv_obj_create(root_);
    Box(settings_, 12, 12, 456, 456, 0xffffff, 32);
    Label(settings_, "让自己舒服一点", 24, 24, 408);
    auto* label = Label(settings_, "音量", 24, 90, 360);
    lv_obj_set_style_text_font(label, &small_font_, 0);
    volume_ = lv_slider_create(settings_);
    lv_obj_set_pos(volume_, 36, 143);
    lv_obj_set_size(volume_, 384, 12);
    lv_slider_set_range(volume_, 0, 100);
    label = Label(settings_, "亮度", 24, 193, 360);
    lv_obj_set_style_text_font(label, &small_font_, 0);
    brightness_ = lv_slider_create(settings_);
    lv_obj_set_pos(brightness_, 36, 246);
    lv_obj_set_size(brightness_, 384, 12);
    lv_slider_set_range(brightness_, 10, 100);
    for (auto* slider : {volume_, brightness_}) {
        lv_obj_set_style_bg_color(slider, lv_color_hex(0xf0e9e5), 0);
        lv_obj_set_style_bg_color(slider, lv_color_hex(kAccent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(kAccent), LV_PART_KNOB);
        lv_obj_add_event_cb(
            slider,
            [](lv_event_t* e) {
                auto* self = static_cast<MosaicoUi*>(lv_event_get_user_data(e));
                auto* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
                self->Emit(obj == self->volume_ ? Action::Volume : Action::Brightness,
                           lv_slider_get_value(obj));
            },
            LV_EVENT_RELEASED, this);
    }
    auto* clear = Button(settings_, "清空对话", 24, 294, 196, Action::Feedback);
    lv_obj_add_event_cb(
        clear,
        [](lv_event_t* e) { static_cast<MosaicoUi*>(lv_event_get_user_data(e))->ClearMessages(); },
        LV_EVENT_CLICKED, this);
    auto* wifi = Button(settings_, "网络配网", 236, 294, 196, Action::ConfigureWifi);
    lv_obj_add_event_cb(
        wifi,
        [](lv_event_t* e) {
            Visible(static_cast<MosaicoUi*>(lv_event_get_user_data(e))->settings_, false);
        },
        LV_EVENT_CLICKED, this);
    auto* done = Button(settings_, "完成", 24, 370, 408, Action::Feedback);
    lv_obj_set_style_bg_color(done, lv_color_hex(kAccent), 0);
    Ink(done, 0xffffff);
    lv_obj_add_event_cb(
        done,
        [](lv_event_t* e) {
            Visible(static_cast<MosaicoUi*>(lv_event_get_user_data(e))->settings_, false);
        },
        LV_EVENT_CLICKED, this);
    Visible(settings_, false);

    camera_page_ = lv_obj_create(root_);
    Box(camera_page_, 0, 0, 480, 480, kCamera, 0);
    Ink(camera_page_, 0xffffff);
    label = Label(camera_page_, "一起看看", 38, 26, 400);
    lv_obj_set_style_text_font(label, &small_font_, 0);
    auto* frame = lv_obj_create(camera_page_);
    Box(frame, 24, 76, 432, 288, 0x292b2e, 28);
    lv_obj_set_style_clip_corner(frame, true, 0);
    camera_image_ = lv_image_create(frame);
    lv_obj_set_size(camera_image_, 432, 288);
    lv_image_set_inner_align(camera_image_, LV_IMAGE_ALIGN_CONTAIN);
    camera_hint_ = Label(camera_page_, "正在打开相机…", 24, 370, 432);
    lv_obj_set_height(camera_hint_, 28);
    lv_label_set_long_mode(camera_hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(camera_hint_, &small_font_, 0);
    lv_obj_set_style_text_align(camera_hint_, LV_TEXT_ALIGN_CENTER, 0);
    Ink(camera_hint_, 0xd1c9c5);
    back = Button(camera_page_, "返回", 32, 412, 80, Action::CloseCamera);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2a2c30), 0);
    Ink(back, 0xffffff);
    inspect_ = Button(camera_page_, "", 209, 406, 62, Action::Inspect);
    lv_obj_set_height(inspect_, 62);
    lv_obj_set_style_bg_color(inspect_, lv_color_hex(kAccent), 0);
    lv_obj_set_style_border_width(inspect_, 3, 0);
    lv_obj_set_style_border_color(inspect_, lv_color_hex(0xffba97), 0);
    ask_ = Button(camera_page_, "提问", 366, 412, 82, Action::Ask);
    lv_obj_set_style_bg_color(ask_, lv_color_hex(kCamera), 0);
    Ink(ask_, 0xffffff);
    Visible(camera_page_, false);
    Layout();
    animation_ = lv_timer_create(
        [](lv_timer_t* timer) {
            static_cast<MosaicoUi*>(lv_timer_get_user_data(timer))->Animate();
        },
        100, this);
}

MosaicoUi::~MosaicoUi() {
    if (animation_)
        lv_timer_delete(animation_);
    lv_obj_delete(root_);
}
void MosaicoUi::SetFont(const lv_font_t* font) {
    small_font_.fallback = font;
    lv_obj_set_style_text_font(root_, font, 0);
}
lv_obj_t* MosaicoUi::Button(lv_obj_t* parent, const char* text, int x, int y, int w,
                            Action action) {
    auto* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    Box(button, x, y, w, 52, 0xffffff, LV_RADIUS_CIRCLE);
    Ink(button, kInk);
    lv_obj_set_style_text_font(button, &small_font_, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xf4d5c6), LV_STATE_PRESSED);
    lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
    auto* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(action)));
    lv_obj_add_event_cb(button, ButtonEvent, LV_EVENT_CLICKED, this);
    return button;
}
void MosaicoUi::ButtonEvent(lv_event_t* event) {
    auto* obj = static_cast<lv_obj_t*>(lv_event_get_target(event));
    static_cast<MosaicoUi*>(lv_event_get_user_data(event))
        ->Emit(static_cast<Action>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj))));
}
void MosaicoUi::Emit(Action action, int value) {
    if (handler_)
        handler_(action, value);
}
void MosaicoUi::BindHeader(lv_obj_t* bar) {
    header_bar_ = bar;
    lv_obj_set_parent(bar, root_);
    lv_obj_set_align(bar, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(bar, 330, 26);
    lv_obj_set_size(bar, 112, 30);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    for (uint32_t i = 0; i < lv_obj_get_child_count(bar); ++i) {
        auto* child = lv_obj_get_child(bar, i);
        Ink(child, kMuted);
        for (uint32_t j = 0; j < lv_obj_get_child_count(child); ++j)
            Ink(lv_obj_get_child(child, j), kMuted);
    }
    lv_obj_move_to_index(bar, 2);  // Under the header's touch target, above background/title.
}
void MosaicoUi::BindStatus(lv_obj_t* bar) {
    status_bar_ = bar;
    lv_obj_set_parent(bar, root_);
    lv_obj_set_align(bar, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(bar, 432, 32);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    for (uint32_t i = 0; i < lv_obj_get_child_count(bar); ++i) {
        auto* child = lv_obj_get_child(bar, i);
        lv_obj_set_style_text_font(child, &small_font_, 0);
        Ink(child, kMuted);
        lv_obj_set_width(child, 432);
        lv_obj_set_style_text_align(child, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(child, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_long_mode(child, LV_LABEL_LONG_DOT);
    }
    lv_obj_move_to_index(history_page_, -1);
    lv_obj_move_to_index(settings_, -1);
    lv_obj_move_to_index(camera_page_, -1);
    Layout();
}
void MosaicoUi::OpenSettings() {
    Visible(settings_, true);
    Emit(Action::Feedback);
}
void MosaicoUi::SetState(State state) {
    state_ = state;
    lv_label_set_text(chat_label_, state == State::Speaking    ? "我想说"
                                   : state == State::Listening ? "结束对话"
                                   : state == State::Busy      ? "连接中…"
                                                               : "聊一聊");
    Layout();
}
void MosaicoUi::Layout() {
    compact_ = !messages_.messages().empty() && !subtitles_hidden_;
    Visible(question_, compact_);
    Visible(answer_box_, compact_);
    Visible(greeting_, !compact_);
    lv_label_set_text(greeting_, state_ == State::Listening ? "嗯，我在听。"
                                 : state_ == State::Busy    ? "等我一下。"
                                                            : "我在，慢慢说。");
    face_y_ = compact_ ? 49 : 66;
    lv_obj_set_pos(face_, compact_ ? 135 : 70, face_y_);
    lv_obj_set_size(face_, compact_ ? 210 : 340, compact_ ? 180 : 290);
    lv_image_set_scale(body_, compact_ ? 158 : 256);
    if (status_bar_)
        lv_obj_set_pos(status_bar_, 24, compact_ ? 202 : 363);
    Animate();
}
void MosaicoUi::SetEmotion(const char* emotion) {
    if (!emotion)
        return;
    happy_ =
        !strcmp(emotion, "happy") || !strcmp(emotion, "laughing") || !strcmp(emotion, "loving");
    sleepy_ = !strcmp(emotion, "sad") || !strcmp(emotion, "crying") || !strcmp(emotion, "sleepy");
    surprised_ = !strcmp(emotion, "surprised") || !strcmp(emotion, "shocked");
    Animate();
}
void MosaicoUi::SetTilt(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    tilt_x_ = .8f * tilt_x_ + .2f * std::clamp(x, -1.0f, 1.0f);
    tilt_y_ = .8f * tilt_y_ + .2f * std::clamp(y, -1.0f, 1.0f);
}
void MosaicoUi::Animate() {
    if (camera_open_ || !lv_obj_has_flag(settings_, LV_OBJ_FLAG_HIDDEN) ||
        !lv_obj_has_flag(history_page_, LV_OBJ_FLAG_HIDDEN))
        return;
    const auto tick = lv_tick_get();
    const float s = compact_ ? 210.0f / 340 : 1.0f;
    const bool blink = tick % 4700 < 160;
    const int eye_h = blink ? 4 : sleepy_ ? 10 : happy_ ? 12 : 38;
    lv_obj_set_y(face_, face_y_ + static_cast<int>(std::sin(tick * .0018f) * 2));
    int smile_x = 114;
    for (auto* smile : {left_smile_, right_smile_}) {
        lv_obj_set_pos(smile, smile_x * s, 130 * s);
        lv_obj_set_size(smile, 24 * s, 26 * s);
        lv_obj_set_style_arc_width(smile, std::max(3, static_cast<int>(7 * s)), LV_PART_INDICATOR);
        Visible(smile, happy_ && !blink);
        smile_x = 202;
    }
    int x = 114;
    for (auto* eye : {left_eye_, right_eye_}) {
        Visible(eye, !happy_ || blink);
        lv_obj_set_pos(eye, (x + tilt_x_ * 8) * s, (143 - eye_h / 2 + tilt_y_ * 5) * s);
        lv_obj_set_size(eye, 24 * s, std::max(2, static_cast<int>(eye_h * s)));
        auto* glint = lv_obj_get_child(eye, 0);
        Visible(glint, !blink && !happy_ && !sleepy_);
        lv_obj_set_pos(glint, 5 * s, 6 * s);
        lv_obj_set_size(glint, std::max(2, static_cast<int>(6 * s)),
                        std::max(3, static_cast<int>(10 * s)));
        x = 202;
    }
    const bool talk = state_ == State::Speaking && tick % 600 < 300;
    lv_obj_set_pos(mouth_, 152 * s, (talk || surprised_ ? 171 : 162) * s);
    lv_obj_set_size(mouth_, 36 * s, (talk || surprised_ ? 30 : 34) * s);
    lv_obj_set_style_arc_width(mouth_,
                               std::max(3, static_cast<int>((talk || surprised_ ? 10 : 6) * s)),
                               LV_PART_INDICATOR);
    lv_arc_set_angles(mouth_, talk || surprised_ ? 0 : 20, talk || surprised_ ? 360 : 160);
}
void MosaicoUi::ScrollEvent(lv_event_t* event) {
    auto* self = static_cast<MosaicoUi*>(lv_event_get_user_data(event));
    if (self->rebuilding_)
        return;
    if (lv_event_get_code(event) == LV_EVENT_SCROLL_BEGIN && lv_indev_active())
        self->follow_ = false;
    if (lv_event_get_code(event) == LV_EVENT_SCROLL_END)
        self->follow_ = lv_obj_get_scroll_bottom(self->history_) <= 12;
    Visible(self->latest_, !self->follow_);
}
void MosaicoUi::RebuildHistory() {
    rebuilding_ = true;
    const int old_y = lv_obj_get_scroll_y(history_);
    lv_obj_clean(history_);
    const char* question = "";
    const char* answer = "";
    for (const auto& message : messages_.messages()) {
        const bool user = message.role == "user";
        if (user) {
            question = message.text.c_str();
            answer = "";
        } else
            answer = message.text.c_str();
        auto* bubble = lv_obj_create(history_);
        Box(bubble, 0, 0, 416, LV_SIZE_CONTENT, user ? 0xede8e6 : 0xffffff, 16);
        lv_obj_set_style_pad_all(bubble, 12, 0);
        auto* label = lv_label_create(bubble);
        lv_obj_set_width(label, 392);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(label, message.text.c_str());
        Ink(label, user ? kMuted : kInk);
    }
    lv_label_set_text(question_, question);
    lv_label_set_text(answer_, answer);
    lv_obj_scroll_to_y(answer_box_, 0, LV_ANIM_OFF);
    lv_obj_update_layout(history_);
    lv_obj_scroll_to_y(history_, follow_ ? LV_COORD_MAX : old_y, LV_ANIM_OFF);
    rebuilding_ = false;
    Layout();
    Visible(latest_, !follow_);
}
void MosaicoUi::AddMessage(const char* role, const char* text) {
    if (role && !strcmp(role, "system")) {
        const bool removed = messages_.ClearSystem();
        const bool added = messages_.Add(role, text);
        if (removed || added)
            RebuildHistory();
        return;
    }
    if (messages_.Add(role, text))
        RebuildHistory();
}
void MosaicoUi::RemoveMessage(const char* role, const char* text) {
    if (messages_.Remove(role, text))
        RebuildHistory();
}
void MosaicoUi::ClearMessages() {
    messages_.Clear();
    follow_ = true;
    RebuildHistory();
}
void MosaicoUi::SetSubtitlesHidden(bool hidden) {
    subtitles_hidden_ = hidden;
    Layout();
}
void MosaicoUi::OpenCamera(bool available) {
    camera_open_ = true;
    camera_available_ = available;
    camera_busy_ = false;
    Visible(settings_, false);
    Visible(history_page_, false);
    Visible(camera_page_, true);
    SetCameraHint(!available              ? "未检测到相机，请关机后安装左侧子板"
                  : state_ == State::Idle ? "正在打开相机…"
                                          : "对话中，取景暂停；可直接拍照");
    SetCameraBusy(false);
}
void MosaicoUi::CloseCamera() {
    camera_open_ = false;
    Visible(camera_page_, false);
}
void MosaicoUi::SetCameraBusy(bool busy) {
    camera_busy_ = busy;
    for (auto* obj : {inspect_, ask_}) {
        if (busy || !camera_available_)
            lv_obj_add_state(obj, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
}
void MosaicoUi::SetCameraHint(const char* hint) { lv_label_set_text(camera_hint_, hint); }
void MosaicoUi::SetCameraImage(const lv_image_dsc_t* image) {
    lv_image_set_src(camera_image_, image);
    if (image && !camera_busy_)
        SetCameraHint("拍张照，问问小智这是什么");
}
void MosaicoUi::SetControls(int volume, int brightness) {
    lv_slider_set_value(volume_, volume, LV_ANIM_OFF);
    lv_slider_set_value(brightness_, brightness, LV_ANIM_OFF);
}
void MosaicoUi::ApplyTheme(bool) {
    // The companion surface has a fixed product palette; camera remains dark.
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    Ink(root_, kInk);
    if (status_bar_)
        BindStatus(status_bar_);
    if (header_bar_)
        BindHeader(header_bar_);
}
