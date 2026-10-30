"""Execute portable production models and the camera arbitration with host drivers."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main/boards/espressif/esp-mosaico"


@unittest.skipUnless(shutil.which("c++"), "requires C++23 compiler")
class MosaicoInteractionTests(unittest.TestCase):
    def compile_run(self, source, stubs=None, extra=()):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            for name, text in (stubs or {}).items():
                path = base / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            (base / "test.cc").write_text(source)
            build = subprocess.run(["c++", "-std=c++23", "-pthread", "-Wall", "-Wextra", "-Werror",
                            "-Wno-unused-parameter",
                            "-I", str(base), "-I", str(BOARD),
                            "-I", str(ROOT / "main/boards/common"),
                            str(base / "test.cc"), *map(str, extra), "-o", str(base / "test")],
                           capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            subprocess.run([str(base / "test")], check=True, timeout=10)

    def test_history_utf8_bounds_and_system_messages(self):
        self.compile_run(r'''
#include "mosaico_ui_model.h"
#include <cassert>
int main() {
    mosaico::History history;
    assert(!history.Add(nullptr,"x") && !history.Add("user",nullptr));
    assert(!history.Add("assistant",""));
    history.Add("user","问题"); history.Add("assistant","回答");
    assert(!history.Add("assistant","回答"));
    history.Add("system","正在连接"); history.ClearSystem();
    assert(history.messages().size()==2);
    std::string huge;
    for(int i=0;i<1000;++i) huge+="中";
    history.Add("assistant",huge.c_str());
    const auto& value=history.messages().back().text;
    assert(value.size()==2049 && value.substr(value.size()-3)=="…");
    for(int i=0;i<30;++i) history.Add("user",std::to_string(i).c_str());
    assert(history.messages().size()==20 && history.messages().front().text=="10");
    assert(!history.Remove("assistant","29"));
    assert(history.Remove("user","29")); history.Clear(); assert(history.messages().empty());
}
''')

    def test_pickup_noise_cooldown_rearm_and_motor_suppression(self):
        self.compile_run(r'''
#include "mosaico_ui_model.h"
#include <cassert>
#include <limits>
int main() {
    mosaico::PickupDetector detector;
    for(int t=0;t<2000;t+=60) assert(!detector.Update(0,0,1,t));
    assert(!detector.Update(.01f,0,1,2000));
    assert(detector.Update(.4f,0,.9f,2060));
    assert(!detector.Update(-.4f,0,.9f,2120));
    for(int t=2200;t<7500;t+=60) assert(!detector.Update(0,0,1,t));
    assert(detector.Update(.4f,0,.9f,7600));
    detector.Suppress(7700);
    assert(!detector.Update(1,1,1,7720));
    assert(!detector.Update(-1,-1,-1,7780));
    assert(!detector.Update(std::numeric_limits<float>::quiet_NaN(),0,1,7800));
}
''')

    def test_camera_capture_explain_transaction_excludes_other_capture(self):
        self.compile_run(r'''
#include "mosaico_camera.h"
#include <cassert>
#include <future>
#include <chrono>
MosaicoCameraPreview::~MosaicoCameraPreview() = default;
bool MosaicoCameraPreview::Present(const EspVideo::PreviewFrame&) { return true; }
int main() {
    MosaicoCamera camera({});
    assert(!EspVideo::streaming && EspVideo::starts==0);
    auto no_credentials=camera.CaptureAndExplain("test");
    assert(!no_credentials && EspVideo::captures==0);
    camera.SetExplainUrl("https://example.invalid/vision","token");
    std::promise<void> entered, release;
    auto wait=release.get_future().share();
    EspVideo::on_explain=[&] { entered.set_value(); wait.wait(); };
    auto transaction=std::async(std::launch::async,[&] { return camera.CaptureAndExplain("test"); });
    entered.get_future().wait();
    auto competing=std::async(std::launch::async,[&] { return camera.Capture(); });
    assert(competing.wait_for(std::chrono::milliseconds(30))==std::future_status::timeout);
    release.set_value();
    auto result=transaction.get(); assert(result && *result=="1");
    assert(competing.get() && EspVideo::captures==2);
    assert(!EspVideo::streaming && EspVideo::starts==2 && EspVideo::stops==2);
    EspVideo::capture_ok=false;
    assert(!camera.CaptureAndExplain("test"));
    assert(!EspVideo::streaming && EspVideo::starts==3 && EspVideo::stops==3);
    EspVideo::start_ok=false;
    assert(!camera.Capture());
    assert(EspVideo::stops==3);
}
''', stubs={"esp_video.h": r'''
#pragma once
#include "camera.h"
#include <functional>
struct esp_video_init_config_t {};
class EspVideo {
public:
    struct PreviewFrame {};
    bool ReadPreview(const std::function<bool(const PreviewFrame&)>& consume) { return consume({}); }
    inline static int captures=0, starts=0, stops=0;
    inline static bool streaming=false, start_ok=true;
    inline static bool capture_ok=true;
    inline static std::function<void()> on_explain;
    explicit EspVideo(const esp_video_init_config_t&, bool start=true) { streaming=start; }
    bool IsInitialized() const { return true; }
    bool StartStreaming() { if(!start_ok) return false; ++starts; streaming=true; return true; }
    bool StopStreaming() { ++stops; streaming=false; return true; }
    void SetExplainUrl(const std::string&,const std::string&) {}
    bool SetHMirror(bool) { return true; }
    bool SetVFlip(bool) { return true; }
    bool Capture() { if(!streaming || !capture_ok) return false; ++captures; return true; }
    std::expected<std::string,std::string> Explain(const std::string&) {
        const int frame=captures; if(on_explain) on_explain();
        if(frame!=captures) return std::unexpected("frame overwritten");
        return std::to_string(frame);
    }
};
''', "freertos/task.h": r'''
#pragma once
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
inline int xTaskCreate(void(*)(void*),const char*,int,void*,int,void*) { return 1; }
inline void vTaskDelay(int) {}
''', "driver/ppa.h": r'''
#pragma once
using ppa_client_handle_t = void*;
struct ppa_srm_oper_config_t {};
''', "esp_timer.h": r'''
#pragma once
#include <cstdint>
inline int64_t esp_timer_get_time() { return 0; }
''', "esp_log.h": r'''
#pragma once
template<class... T> void ESP_LOGI(T...) {}
'''}, extra=[BOARD / "mosaico_camera.cc"])

    def test_default_camera_atomic_api_preserves_capture_failures(self):
        self.compile_run(r'''
#include "camera.h"
#include <cassert>
struct TestCamera : Camera {
    bool captured=false; int explains=0;
    void SetExplainUrl(const std::string&,const std::string&) override {}
    bool Capture() override { return captured; }
    bool SetHMirror(bool) override { return true; }
    bool SetVFlip(bool) override { return true; }
    std::expected<std::string,std::string> Explain(const std::string& q) override { ++explains; return q; }
};
int main() {
    TestCamera camera;
    assert(!camera.CaptureAndExplain("q") && camera.explains==0);
    camera.captured=true; assert(*camera.CaptureAndExplain("q")=="q" && camera.explains==1);
}
''')


if __name__ == "__main__":
    unittest.main()
