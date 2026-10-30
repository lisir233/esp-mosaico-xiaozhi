#pragma once

namespace mosaico {
enum class WifiDefaultsResult { AlreadyConfigured, Imported, Unavailable, Invalid };

// Call before WifiBoard::StartNetwork(), before Wi-Fi tasks access SsidManager.
// Seeds an empty local list through its normal persistence API. Shared values
// are read-only; existing local credentials always take precedence.
WifiDefaultsResult ImportSharedWifiIfEmpty();
}  // namespace mosaico
