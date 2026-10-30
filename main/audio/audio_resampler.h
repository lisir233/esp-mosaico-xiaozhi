#pragma once

#include <cstdint>
#include <vector>

#include "esp_ae_rate_cvt.h"

// Single-owner converter. Callers serialize Configure/Process/Reset together.
// Process replaces PCM only on success; failure leaves input intact for disposal.
class AudioResampler {
public:
    explicit AudioResampler(const char* name) : name_(name) {}
    ~AudioResampler();
    AudioResampler(const AudioResampler&) = delete;
    AudioResampler& operator=(const AudioResampler&) = delete;

    bool Configure(int source_rate, int destination_rate, int channels);
    bool Process(std::vector<int16_t>& pcm);
    bool Reset();

private:
    void Close();
    const char* name_;
    esp_ae_rate_cvt_handle_t handle_ = nullptr;
    int source_rate_ = 0;
    int destination_rate_ = 0;
    int channels_ = 0;
    int16_t* scratch_ = nullptr;
    size_t capacity_ = 0;
    uint32_t sequence_ = 0;
    int64_t last_log_us_ = 0;
};
