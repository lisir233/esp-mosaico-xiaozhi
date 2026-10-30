#pragma once
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <atomic>
#include <functional>
#include <mutex>
#include "mosaico_ui_model.h"

// Board-lifetime worker owns BMI270; an independent one-shot timer turns GPIO8
// off even when an I2C read or LVGL is busy. Motor writes are serialized.
class MosaicoFeedback {
public:
    bool Start(i2c_master_bus_handle_t bus, std::function<void(float, float, bool)> motion);
    void Pulse(unsigned milliseconds = 30);

private:
    i2c_master_bus_handle_t bus_ = nullptr;
    std::function<void(float, float, bool)> motion_;
    std::atomic<int64_t> motor_until_{0};
    std::mutex motor_gate_;
    esp_timer_handle_t motor_timer_ = nullptr;
    bool started_ = false;
    void Run();
    void StopPulse();
};
