#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include "mosaico_display_alignment.h"
#include "mosaico_display_config.h"
#include "mosaico_ui.h"

static lv_point_t pointer;
static bool pressed = false;
static void Tick(int count = 4) {
    for (int i = 0; i < count; ++i) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
}
static lv_obj_t* Find(lv_obj_t* root, const char* text) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), text))
        return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* match = Find(lv_obj_get_child(root, i), text))
            return match;
    return nullptr;
}
static void Tap(int x, int y) {
    pointer = {x, y};
    pressed = true;
    Tick();
    pressed = false;
    Tick();
}
static void Click(lv_obj_t* screen, const char* label) {
    auto* obj = Find(screen, label);
    assert(obj);
    lv_obj_update_layout(screen);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    pointer = {(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    pressed = true;
    Tick();
    pressed = false;
    Tick();
}
static void Save(lv_obj_t* screen, const char* name) {
    Tick();
    auto* buffer = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    assert(buffer);
    FILE* out = fopen(name, "wb");
    assert(out);
    fprintf(out, "P6\n480 480\n255\n");
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 480; ++x) {
            const auto* pixel = buffer->data + y * buffer->header.stride + x * 3;
            const uint8_t rgb[] = {pixel[2], pixel[1], pixel[0]};
            fwrite(rgb, 1, 3, out);
        }
    fclose(out);
    lv_draw_buf_destroy(buffer);
}
int main(int argc, char** argv) {
    const bool interactive = argc > 1 && !strcmp(argv[1], "--interactive");
    lv_init();
    lv_display_t* display;
    if (interactive) {
        display = lv_sdl_window_create(480, 480);
        lv_sdl_mouse_create();
        lv_sdl_mousewheel_create();
    } else {
        display = lv_display_create(480, 480);
        static uint8_t pixels[480 * kMosaicoDrawBufferLines * 2];
        lv_display_set_buffers(display, pixels, nullptr, sizeof(pixels),
                               LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t* area, uint8_t*) {
            assert(area->x1 % 4 == 0 && (area->x2 + 1) % 4 == 0);
            assert(area->x1 >= 0 && area->x2 < 480);
            assert(lv_area_get_size(area) <= 480 * kMosaicoDrawBufferLines);
            lv_display_flush_ready(d);
        });
        auto* input = lv_indev_create();
        lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(input, [](lv_indev_t*, lv_indev_data_t* data) {
            data->point = pointer;
            data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        });
    }
    lv_display_add_event_cb(display, MosaicoRoundDisplayArea, LV_EVENT_INVALIDATE_AREA, nullptr);
    for (int left = 0; left < 480; ++left) {
        for (int right = left; right < 480; ++right) {
            lv_area_t area{left, 13, right, 31};
            lv_display_send_event(display, LV_EVENT_INVALIDATE_AREA, &area);
            assert(area.x1 <= left && area.x2 >= right);
            assert(area.x1 % 4 == 0 && (area.x2 + 1) % 4 == 0);
            assert(area.x1 >= 0 && area.x2 < 480);
            assert(area.y1 == 13 && area.y2 == 31);
        }
    }
    const char* font_path = getenv("MOSAICO_SIM_FONT");
    auto* font = lv_tiny_ttf_create_file(
        font_path ? font_path : "A:/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 30);
    assert(font);
    auto* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf7f6f9), 0);
    lv_obj_set_style_text_font(screen, font, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xeaf2f4), 0);
    auto* title = lv_label_create(screen);
    lv_label_set_text(title, "小智");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    MosaicoUi view(screen, font);
    auto* bar = lv_obj_create(screen);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(0xeaf2f4), 0);
    auto* status = lv_label_create(bar);
    lv_label_set_text(status, "轻触我，或说「你好小智」");
    lv_obj_center(status);
    view.BindStatus(bar);
    view.SetControls(70, 80);
    int chats = 0, opens = 0, inspects = 0, closes = 0;
    bool available = false;
    view.SetHandler([&](MosaicoUi::Action action, int) {
        using Action = MosaicoUi::Action;
        switch (action) {
            case Action::Chat:
                ++chats;
                view.SetState(MosaicoUi::State::Listening);
                lv_label_set_text(status, "正在听");
                break;
            case Action::OpenCamera:
                ++opens;
                view.OpenCamera(available);
                break;
            case Action::CloseCamera:
                ++closes;
                view.CloseCamera();
                break;
            case Action::Inspect:
                ++inspects;
                view.SetCameraBusy(true);
                view.SetCameraHint("正在拍照识图…");
                break;
            default:
                break;
        }
    });
    if (interactive) {
        view.AddMessage("user", "今天有什么值得期待的事情？");
        view.AddMessage("assistant", "也许是一杯热茶，或是和朋友聊聊天。你想先做哪一件？");
        for (;;) {
            Tick(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    Tick(12);  // Capture an open-eyed idle expression, outside the blink window.
    Save(screen, "idle.ppm");
    Click(screen, "聊一聊");
    assert(chats == 1);
    view.AddMessage("user", "今天有点累。");
    view.AddMessage("assistant", "辛苦啦。\n要不要先歇一会儿？");
    view.SetState(MosaicoUi::State::Speaking);
    view.SetEmotion("happy");
    lv_label_set_text(status, "正在陪你聊");
    Save(screen, "conversation.ppm");
    for (int i = 0; i < 6; ++i)
        view.AddMessage("assistant", ("这是一条可以回看的完整消息 " + std::to_string(i)).c_str());
    Tap(392, 434);  // History icon.
    Tick();
    pointer = {220, 140};
    pressed = true;
    Tick();
    for (int y = 140; y <= 350; y += 10) {
        pointer.y = y;
        Tick(1);
    }
    pressed = false;
    Tick(12);
    view.AddMessage("assistant", "你可以点“最新”回到最后一条消息。");
    auto* latest = Find(screen, "最新 ↓");
    assert(latest && !lv_obj_has_flag(lv_obj_get_parent(latest), LV_OBJ_FLAG_HIDDEN));
    // Idle status ticks must not rebuild/scroll a user's historical position.
    auto* history_page = lv_obj_get_parent(Find(screen, "我们的对话"));
    auto* history = lv_obj_get_parent(
        lv_obj_get_parent(Find(history_page, "你可以点“最新”回到最后一条消息。")));
    const int history_y = lv_obj_get_scroll_y(history);
    view.AddMessage("system", "");
    assert(lv_obj_get_scroll_y(history) == history_y);
    Save(screen, "history.ppm");
    Click(screen, "最新 ↓");
    assert(lv_obj_has_flag(lv_obj_get_parent(latest), LV_OBJ_FLAG_HIDDEN));
    Click(screen, "返回");
    Tap(88, 434);
    assert(opens == 1 && view.CameraOpen());
    Tap(240, 437);
    assert(inspects == 0);
    Save(screen, "camera-unavailable.ppm");
    Tap(72, 438);
    assert(closes == 1 && !view.CameraOpen());
    available = true;
    Tap(88, 434);
    Tap(240, 437);
    assert(inspects == 1);
    Tap(240, 437);
    assert(inspects == 1);
    view.SetCameraBusy(false);
    // Exercise the real LVGL image path at native preview resolution. Each
    // frame changes the grid; verify updates, closure, and reopening below.
    static uint16_t camera_pixels[432 * 288];
    lv_image_dsc_t camera_frame{};
    camera_frame.header.magic = LV_IMAGE_HEADER_MAGIC;
    camera_frame.header.cf = LV_COLOR_FORMAT_RGB565;
    camera_frame.header.w = 432;
    camera_frame.header.h = 288;
    camera_frame.header.stride = 432 * 2;
    camera_frame.data_size = sizeof(camera_pixels);
    camera_frame.data = reinterpret_cast<uint8_t*>(camera_pixels);
    for (int frame = 0; frame < 10; ++frame) {
        for (int y = 0; y < 288; ++y)
            for (int x = 0; x < 432; ++x)
                camera_pixels[y * 432 + x] = y < 12 ? 0xf800 :
                    ((x / 36 + y / 36 + frame) % 2 ? 0x07e0 : 0x001f);
        view.SetCameraImage(&camera_frame);
        Tick();
    }
    Save(screen, "camera-preview.ppm");
    view.SetCameraImage(nullptr);
    view.CloseCamera();
    view.SetState(MosaicoUi::State::Listening);
    view.OpenCamera(true);
    assert(Find(screen, "对话中，取景暂停；可直接拍照"));
    view.CloseCamera();
    view.OpenCamera(true);
    assert(Find(screen, "对话中，取景暂停；可直接拍照"));
    view.SetCameraHint("服务暂不可用，请重试");
    Save(screen, "camera-error.ppm");
    view.SetCameraHint(
        "An unexpectedly long server error must not cover the shutter or prevent going back to "
        "chat.");
    Tick();
    Tap(72, 438);
    pointer = {220, 35};
    pressed = true;
    Tick();
    for (int y = 35; y < 125; y += 10) {
        pointer.y = y;
        Tick(1);
    }
    pressed = false;
    Tick();
    auto* done = Find(screen, "完成");
    assert(done &&
           !lv_obj_has_flag(lv_obj_get_parent(lv_obj_get_parent(done)), LV_OBJ_FLAG_HIDDEN));
    Save(screen, "settings.ppm");
    Click(screen, "完成");
    for (int i = 0; i < 30; ++i)
        view.AddMessage("assistant", std::to_string(i).c_str());
    assert(!Find(screen, "0") && Find(screen, "29"));
    view.SetState(MosaicoUi::State::Idle);
    assert(Find(screen, "29"));  // Idle must preserve conversation.
    view.ClearMessages();
    Save(screen, "cleared.ppm");
    Tap(240, 220);
    assert(chats == 2);  // Character itself opens conversation.
    puts(
        "PASS: native companion, character touch, conversation, retained history, scroll-follow, "
        "camera absent/busy/error, settings swipe, CO5300 aligned flushes");
}
