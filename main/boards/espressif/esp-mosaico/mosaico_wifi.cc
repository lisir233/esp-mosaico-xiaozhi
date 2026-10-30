#include "mosaico_wifi.h"
#include <mosaico_shared.h>
#include <ssid_manager.h>
#include <cstring>

namespace mosaico {
namespace {
constexpr size_t kSsidCapacity = 33;
constexpr size_t kPasswordCapacity = 65;

struct Credentials {
    char ssid[kSsidCapacity]{};
    char password[kPasswordCapacity]{};
    ~Credentials() {
        // Do not leave the temporary shared password on the startup task stack.
        volatile char* bytes = password;
        for (size_t i = 0; i < sizeof(password); ++i)
            bytes[i] = 0;
    }
};

bool ValidPassword(const char* password) {
    const size_t length = std::strlen(password);
    if (length == 0 || (length >= 8 && length <= 63))
        return true;
    return length == 64 && std::strspn(password, "0123456789abcdefABCDEF") == length;
}
}  // namespace

WifiDefaultsResult ImportSharedWifiIfEmpty() {
    auto& manager = SsidManager::GetInstance();
    if (!manager.GetSsidList().empty())
        return WifiDefaultsResult::AlreadyConfigured;

    Credentials credentials;
    if (mosaico_shared_get("wifi_ssid", credentials.ssid, sizeof(credentials.ssid)) != ESP_OK ||
        mosaico_shared_get("wifi_password", credentials.password, sizeof(credentials.password)) !=
            ESP_OK)
        return WifiDefaultsResult::Unavailable;
    if (!credentials.ssid[0] || !ValidPassword(credentials.password))
        return WifiDefaultsResult::Invalid;

    manager.AddSsid(credentials.ssid, credentials.password);
    return WifiDefaultsResult::Imported;
}
}  // namespace mosaico
