#include <sys/resource.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include "audio_resampler.h"

bool fail_alloc = false;
extern "C" void* __real_malloc(size_t);
extern "C" void* __wrap_malloc(size_t n) { return fail_alloc ? nullptr : __real_malloc(n); }
int open_error = 0, query_error = 0, process_error = 0, reset_error = 0;
int opens = 0, closes = 0, calls = 0;
uint32_t required_override = 0;
bool overflow_count = false, overrun = false, underrun = false;
struct Handle {
    esp_ae_rate_cvt_cfg_t config;
};
int esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t* cfg, void** handle) {
    ++opens;
    *handle = open_error ? nullptr : new Handle{*cfg};
    return open_error;
}
void esp_ae_rate_cvt_close(void* handle) {
    ++closes;
    delete static_cast<Handle*>(handle);
}
int esp_ae_rate_cvt_reset(void*) { return reset_error; }
int esp_ae_rate_cvt_get_max_out_sample_num(void* h, uint32_t frames, uint32_t* out) {
    const auto& cfg = static_cast<Handle*>(h)->config;
    *out = required_override ? required_override : frames * cfg.dest_rate / cfg.src_rate + 128;
    return query_error;
}
int esp_ae_rate_cvt_process(void* h, void*, uint32_t frames, void* out, uint32_t* count) {
    ++calls;
    const auto& cfg = static_cast<Handle*>(h)->config;
    if (overrun)
        static_cast<int16_t*>(out)[*count * cfg.channel] = 123;
    if (underrun)
        static_cast<int16_t*>(out)[-1] = 123;
    if (process_error)
        return process_error;
    if (overflow_count) {
        ++*count;
        return 0;
    }
    *count = frames * cfg.dest_rate / cfg.src_rate;
    std::fill_n(static_cast<int16_t*>(out), *count * cfg.channel, 42);
    return 0;
}
int main(int argc, char** argv) {
    rlimit limit{0, 0};
    setrlimit(RLIMIT_CORE, &limit);
    std::string mode = argc > 1 ? argv[1] : "normal";
    AudioResampler converter("test");
    assert(converter.Configure(16000, 24000, 1));
    assert(converter.Configure(16000, 24000, 1) && opens == 1);
    std::vector<int16_t> pcm(960, 7);
    if (mode == "overrun") {
        overrun = true;
        converter.Process(pcm);
        return 10;
    }
    if (mode == "underrun") {
        underrun = true;
        converter.Process(pcm);
        return 11;
    }
    assert(converter.Process(pcm));
    assert(pcm.size() == 1440 && pcm.front() == 42 && pcm.back() == 42);
    for (int failure = 0; failure < 5; ++failure) {
        assert(converter.Configure(16000, 24000, 1));
        pcm.assign(960, 7);
        auto original = pcm;
        query_error = failure == 0 ? -1 : 0;
        process_error = failure == 1 ? -2 : 0;
        overflow_count = failure == 2;
        required_override = failure == 3 ? UINT32_MAX : 0;
        // Force a larger valid buffer to exercise allocation failure after reuse.
        if (failure == 4) {
            required_override = 2000;
            fail_alloc = true;
        }
        auto before_calls = calls;
        assert(!converter.Process(pcm) && pcm == original);
        if (failure == 0 || failure == 3 || failure == 4)
            assert(calls == before_calls);
        query_error = process_error = 0;
        overflow_count = false;
        required_override = 0;
        fail_alloc = false;
    }
    assert(converter.Configure(24000, 16000, 2));
    pcm.assign(480, 7);
    assert(converter.Process(pcm) && pcm.size() == 320);
    pcm.assign(3, 7);
    assert(!converter.Process(pcm));
    pcm.clear();
    assert(!converter.Process(pcm));
    pcm.assign(48002, 7);
    assert(!converter.Process(pcm));
    assert(!converter.Configure(0, 24000, 1));
    open_error = -1;
    assert(!converter.Configure(16000, 24000, 1));
    open_error = 0;
    assert(converter.Configure(16000, 24000, 1));
    reset_error = -1;
    assert(!converter.Reset());
    pcm.assign(960, 7);
    assert(!converter.Process(pcm));
    reset_error = 0;
    assert(converter.Configure(16000, 24000, 1));
    assert(converter.Reset() && converter.Process(pcm));
    assert(closes > 0);
}
