"""Exercise the board's real shared-default import without touching device NVS."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

BOARD = Path(__file__).resolve().parents[2] / "main/boards/espressif/esp-mosaico"


@unittest.skipUnless(shutil.which("c++"), "requires C++ compiler")
class MosaicoWifiTests(unittest.TestCase):
    def test_local_priority_missing_invalid_and_open_shared_networks(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            (base / "mosaico_shared.h").write_text(r'''
#pragma once
#include <cstddef>
using esp_err_t = int;
constexpr int ESP_OK = 0;
esp_err_t mosaico_shared_get(const char*, char*, size_t);
''')
            (base / "ssid_manager.h").write_text(r'''
#pragma once
#include <string>
#include <vector>
struct SsidItem { std::string ssid, password; };
class SsidManager {
public:
    std::vector<SsidItem> items;
    int writes = 0;
    static SsidManager& GetInstance() { static SsidManager s; return s; }
    const auto& GetSsidList() const { return items; }
    void AddSsid(const std::string& ssid, const std::string& password) {
        ++writes;
        items.push_back({ssid, password});
    }
};
''')
            (base / "test.cc").write_text(r'''
#include "mosaico_wifi.h"
#include "mosaico_shared.h"
#include "ssid_manager.h"
#include <cassert>
#include <cstring>
#include <map>
std::map<std::string, std::string> shared;
int reads = 0;
esp_err_t mosaico_shared_get(const char* key, char* out, size_t cap) {
    ++reads;
    auto it = shared.find(key);
    if (it == shared.end() || it->second.size() >= cap) return -1;
    std::memcpy(out, it->second.c_str(), it->second.size() + 1);
    return ESP_OK;
}
int main() {
    using mosaico::ImportSharedWifiIfEmpty;
    using R = mosaico::WifiDefaultsResult;
    auto& local = SsidManager::GetInstance();
    shared = {{"wifi_ssid", "shared-network"}, {"wifi_password", "test-password"}};
    local.items.push_back({"local-network", "local-password"});
    assert(ImportSharedWifiIfEmpty() == R::AlreadyConfigured);
    assert(reads == 0 && local.writes == 0 && local.items[0].password == "local-password");

    local.items.clear();
    assert(ImportSharedWifiIfEmpty() == R::Imported);
    assert(local.items.size() == 1 && local.items[0].ssid == "shared-network");
    assert(local.items[0].password == "test-password" && local.writes == 1);
    shared["wifi_password"] = "changed-password";
    assert(ImportSharedWifiIfEmpty() == R::AlreadyConfigured);
    assert(local.writes == 1 && local.items[0].password == "test-password");

    local.items.clear(); local.writes = 0;
    shared.clear();
    assert(ImportSharedWifiIfEmpty() == R::Unavailable && local.items.empty());
    shared["wifi_ssid"] = "network";
    assert(ImportSharedWifiIfEmpty() == R::Unavailable && local.items.empty());
    shared["wifi_password"] = "";
    shared["wifi_ssid"] = "";
    assert(ImportSharedWifiIfEmpty() == R::Invalid && local.items.empty());
    shared["wifi_ssid"] = std::string(33, 's');
    assert(ImportSharedWifiIfEmpty() == R::Unavailable && local.items.empty());
    shared["wifi_ssid"] = "network";
    for (auto invalid : {std::string("short"), std::string(64, 'z'), std::string(65, 'a')}) {
        shared["wifi_password"] = invalid;
        assert(ImportSharedWifiIfEmpty() != R::Imported && local.items.empty());
    }
    assert(local.writes == 0);

    for (auto password : {std::string(), std::string(8, 'p'), std::string(63, 'p'),
                          std::string(64, 'a')}) {
        shared = {{"wifi_ssid", std::string(32, 's')}, {"wifi_password", password}};
        const auto before = shared;
        assert(ImportSharedWifiIfEmpty() == R::Imported);
        assert(local.items[0].ssid.size() == 32 && local.items[0].password == password);
        assert(shared == before);
        local.items.clear();
    }
}
''')
            build = subprocess.run(
                ["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I", str(base),
                 "-I", str(BOARD), str(base / "test.cc"), str(BOARD / "mosaico_wifi.cc"),
                 "-o", str(base / "test")], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            subprocess.run([str(base / "test")], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
