#include "audio_resampler.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdlib>

namespace {
constexpr const char* TAG = "AudioResampler";
constexpr int64_t kLogIntervalUs = 5000000;
constexpr size_t kGuardSamples = 8;  // 16-byte guards preserve the allocator's alignment.
constexpr int16_t kGuard = 0x5a3d;
constexpr int kMaxRate = 192000;
constexpr int kMaxChannels = 8;
constexpr uint32_t kFilterMarginFrames = 4096;

}  // namespace

AudioResampler::~AudioResampler() {
    Close();
    free(scratch_);
}

void AudioResampler::Close() {
    if (handle_) {
        esp_ae_rate_cvt_close(handle_);
        handle_ = nullptr;
    }
}

bool AudioResampler::Configure(int source_rate, int destination_rate, int channels) {
    if (source_rate <= 0 || source_rate > kMaxRate || destination_rate <= 0 ||
        destination_rate > kMaxRate || channels <= 0 || channels > kMaxChannels) {
        ESP_LOGE(TAG, "%s invalid format %d -> %d ch=%d", name_, source_rate, destination_rate,
                 channels);
        Close();
        return false;
    }
    if (handle_ && source_rate_ == source_rate && destination_rate_ == destination_rate &&
        channels_ == channels) {
        return true;
    }
    Close();
    source_rate_ = source_rate;
    destination_rate_ = destination_rate;
    channels_ = channels;
    esp_ae_rate_cvt_cfg_t config = {};
    config.src_rate = source_rate;
    config.dest_rate = destination_rate;
    config.channel = channels;
    config.bits_per_sample = 16;
    config.complexity = 2;
    config.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
    auto err = esp_ae_rate_cvt_open(&config, &handle_);
    if (err != ESP_AE_ERR_OK || !handle_) {
        ESP_LOGE(TAG, "%s open failed err=%d %d -> %d ch=%d", name_, err, source_rate,
                 destination_rate, channels);
        Close();
        return false;
    }
    sequence_ = 0;
    ESP_LOGI(TAG, "%s opened handle=%p %d -> %d ch=%d", name_, handle_, source_rate,
             destination_rate, channels);
    return true;
}

bool AudioResampler::Reset() {
    if (!handle_) {
        return true;
    }
    auto err = esp_ae_rate_cvt_reset(handle_);
    if (err != ESP_AE_ERR_OK) {
        ESP_LOGE(TAG, "%s reset failed err=%d", name_, err);
        Close();
        return false;
    }
    return true;
}

bool AudioResampler::Process(std::vector<int16_t>& pcm) {
    // AudioService feeds <=120 ms packets. A one-second ceiling also bounds any
    // size returned by a damaged handle before allocating or doing arithmetic.
    if (!handle_ || channels_ <= 0 || pcm.empty() || pcm.size() % channels_ != 0 ||
        pcm.size() / channels_ > static_cast<size_t>(source_rate_)) {
        ESP_LOGE(TAG, "%s invalid input handle=%p samples=%u ch=%d", name_, handle_,
                 static_cast<unsigned>(pcm.size()), channels_);
        return false;
    }
    const uint32_t frames = pcm.size() / channels_;
    const auto now = esp_timer_get_time();
    const bool trace = ++sequence_ <= 3 || now - last_log_us_ >= kLogIntervalUs;
    if (trace) {
        last_log_us_ = now;
        ESP_LOGI(TAG,
                 "%s begin seq=%lu handle=%p rate=%d/%d ch=%d in=%p frames=%lu "
                 "free_internal=%u free_psram=%u stack_free=%u",
                 name_, static_cast<unsigned long>(sequence_), handle_, source_rate_,
                 destination_rate_, channels_, pcm.data(), static_cast<unsigned long>(frames),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    uint32_t required = 0;
    auto err = esp_ae_rate_cvt_get_max_out_sample_num(handle_, frames, &required);
    if (err != ESP_AE_ERR_OK || required == 0 ||
        required > static_cast<uint32_t>(destination_rate_) + kFilterMarginFrames) {
        ESP_LOGE(TAG, "%s size failed seq=%lu err=%d in=%lu required=%lu", name_,
                 static_cast<unsigned long>(sequence_), err, static_cast<unsigned long>(frames),
                 static_cast<unsigned long>(required));
        Close();
        return false;
    }
    const size_t samples = static_cast<size_t>(required) * channels_;
    if (samples > capacity_) {
        auto* buffer =
            static_cast<int16_t*>(malloc((samples + 2 * kGuardSamples) * sizeof(int16_t)));
        if (!buffer) {
            ESP_LOGE(TAG, "%s allocation failed samples=%u", name_, static_cast<unsigned>(samples));
            return false;
        }
        free(scratch_);
        scratch_ = buffer;
        capacity_ = samples;
    }
    auto* output = scratch_ + kGuardSamples;
    std::fill_n(scratch_, kGuardSamples, kGuard);
    std::fill_n(output + samples, kGuardSamples, kGuard);
    uint32_t actual = required;
    if (trace) {
        ESP_LOGI(TAG, "%s process seq=%lu out=%p capacity_frames=%lu", name_,
                 static_cast<unsigned long>(sequence_), output,
                 static_cast<unsigned long>(required));
    }
    err = esp_ae_rate_cvt_process(handle_, reinterpret_cast<esp_ae_sample_t>(pcm.data()), frames,
                                  reinterpret_cast<esp_ae_sample_t>(output), &actual);
    if (!std::all_of(scratch_, output, [](int16_t v) { return v == kGuard; }) ||
        !std::all_of(output + samples, output + samples + kGuardSamples,
                     [](int16_t v) { return v == kGuard; })) {
        ESP_LOGE(TAG, "%s buffer overrun seq=%lu capacity=%lu actual=%lu err=%d", name_,
                 static_cast<unsigned long>(sequence_), static_cast<unsigned long>(required),
                 static_cast<unsigned long>(actual), err);
        abort();
    }
    if (trace) {
        ESP_LOGI(TAG, "%s end seq=%lu err=%d actual=%lu elapsed_us=%lld", name_,
                 static_cast<unsigned long>(sequence_), err, static_cast<unsigned long>(actual),
                 static_cast<long long>(esp_timer_get_time() - now));
    }
    if (err != ESP_AE_ERR_OK || actual > required) {
        ESP_LOGE(TAG, "%s process failed seq=%lu err=%d capacity=%lu actual=%lu", name_,
                 static_cast<unsigned long>(sequence_), err, static_cast<unsigned long>(required),
                 static_cast<unsigned long>(actual));
        Close();
        return false;
    }
    pcm.assign(output, output + static_cast<size_t>(actual) * channels_);
    return true;
}
