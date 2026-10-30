#include "mosaico_feedback.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bmi270_api.h"
#include "config.h"

bool MosaicoFeedback::Start(i2c_master_bus_handle_t bus,
                            std::function<void(float, float, bool)> motion) {
    if (started_)
        return true;
    bus_ = bus;
    motion_ = std::move(motion);
    const gpio_config_t config = {.pin_bit_mask = 1ULL << MOTOR_GPIO, .mode = GPIO_MODE_OUTPUT};
    if (gpio_config(&config) != ESP_OK || gpio_set_level(MOTOR_GPIO, 0) != ESP_OK)
        return false;
    const esp_timer_create_args_t timer = {
        .callback = [](void* arg) { static_cast<MosaicoFeedback*>(arg)->StopPulse(); },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ui_haptic_off"};
    if (esp_timer_create(&timer, &motor_timer_) != ESP_OK)
        return false;
    started_ = xTaskCreate([](void* arg) { static_cast<MosaicoFeedback*>(arg)->Run(); },
                           "ui_motion", 6144, this, 1, nullptr) == pdPASS;
    if (!started_) {
        esp_timer_delete(motor_timer_);
        motor_timer_ = nullptr;
    }
    return started_;
}
void MosaicoFeedback::Pulse(unsigned milliseconds) {
    if (!started_)
        return;
    std::lock_guard lock(motor_gate_);
    const uint64_t duration = std::clamp(milliseconds, 10U, 80U) * 1000;
    motor_until_ = esp_timer_get_time() + duration;
    esp_timer_stop(motor_timer_);
    if (gpio_set_level(MOTOR_GPIO, 1) != ESP_OK ||
        esp_timer_start_once(motor_timer_, duration) != ESP_OK) {
        gpio_set_level(MOTOR_GPIO, 0);
        motor_until_ = 0;
    }
}
void MosaicoFeedback::StopPulse() {
    std::lock_guard lock(motor_gate_);
    const int64_t remaining = motor_until_.load() - esp_timer_get_time();
    if (remaining > 0) {
        // A callback from the preceding pulse may already be queued while a new
        // pulse has armed its timer. Do not cut that new pulse short.
        if (esp_timer_is_active(motor_timer_) ||
            esp_timer_start_once(motor_timer_, remaining) == ESP_OK)
            return;
    }
    gpio_set_level(MOTOR_GPIO, 0);
    motor_until_ = 0;
}
void MosaicoFeedback::Run() {
    bmi270_handle_t imu = nullptr;
    esp_err_t err = bmi270_sensor_create_from_master_bus_with_address(
        bus_, 0x69, &imu, bmi270_config_file, BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE);
    bool ready = err == ESP_OK;
    if (ready) {
        bmi2_sens_config config = {.type = BMI2_ACCEL};
        ready = bmi2_get_sensor_config(&config, 1, imu) == BMI2_OK;
        if (ready) {
            config.cfg.acc.odr = BMI2_ACC_ODR_50HZ;
            config.cfg.acc.range = BMI2_ACC_RANGE_4G;
            config.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
            config.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
            const uint8_t sensor = BMI2_ACCEL;
            ready = bmi2_set_sensor_config(&config, 1, imu) == BMI2_OK &&
                    bmi2_sensor_enable(&sensor, 1, imu) == BMI2_OK;
        }
    }
    ESP_LOGI("MosaicoFeedback", "BMI270 %s; haptic worker ready", ready ? "ready" : "unavailable");
    mosaico::PickupDetector detector;
    int failures = 0;
    unsigned tick = 0;
    for (;;) {
        const int64_t now = esp_timer_get_time();
        const bool on = now < motor_until_.load();
        if (on)
            detector.Suppress(now / 1000);
        if (ready && ++tick % 3 == 0) {
            bmi2_sens_data sample = {};
            if (bmi2_get_sensor_data(&sample, imu) == BMI2_OK) {
                failures = 0;
                if (sample.status & BMI2_DRDY_ACC) {
                    constexpr float scale = 4.0f / 32768.0f;
                    const float x = sample.acc.x * scale, y = sample.acc.y * scale,
                                z = sample.acc.z * scale;
                    const bool pickup = !on && detector.Update(x, y, z, now / 1000);
                    if (motion_)
                        motion_(x, y, pickup);
                }
            } else if (++failures >= 10) {
                ESP_LOGW("MosaicoFeedback", "BMI270 read failures; disabling motion feedback");
                ready = false;
            }
        }
        if (!ready && imu) {
            // A failed create can retain a cleanup-only handle. Retry deletion.
            bmi270_sensor_del(&imu);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
