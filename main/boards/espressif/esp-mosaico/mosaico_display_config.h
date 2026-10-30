#pragma once

// At 480 pixels in RGB565, four full rows occupy 3840 bytes. Keep every
// partial refresh below the SPI transaction limit and one DMA node's capacity,
// including narrow dirty rectangles, whose height can exceed four rows.
// Shared with the native simulator so it exercises the device's buffer budget.
inline constexpr int kMosaicoDrawBufferLines = 4;
