#pragma once
#include <lvgl.h>
#include <algorithm>

// Match the BSP CO5300 QSPI area rounder. Round before LVGL renders, so the
// pixel buffer and the panel's address window describe the same rectangle.
// Changing coordinates only in flush would read past the rendered buffer.
inline void MosaicoRoundDisplayArea(lv_event_t* event) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    auto* display = static_cast<lv_display_t*>(lv_event_get_target(event));
    if (!area || !display)
        return;
    constexpr int kPixelsPerGroup = 4;
    const int last_x = lv_display_get_horizontal_resolution(display) - 1;
    if (last_x < 0)
        return;
    const int left = std::clamp(static_cast<int>(area->x1), 0, last_x);
    const int right = std::clamp(static_cast<int>(area->x2), 0, last_x);
    area->x1 = left / kPixelsPerGroup * kPixelsPerGroup;
    area->x2 = std::min((right / kPixelsPerGroup + 1) * kPixelsPerGroup - 1, last_x);
}
