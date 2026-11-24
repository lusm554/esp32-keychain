#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace preview {
constexpr int size = 240;
using Pixels = std::array<uint16_t, size * size>;

inline Pixels framebuffer{};
inline std::mutex framebufferMutex;
inline uint64_t revision = 0;
inline bool spiDelayEnabled = false;

inline bool copyFrame(Pixels &destination, uint64_t &shownRevision) {
  const std::lock_guard lock(framebufferMutex);
  if (revision == shownRevision) {
    return false;
  }
  destination = framebuffer;
  shownRevision = revision;
  return true;
}
}  // namespace preview

constexpr int D0 = 1;
constexpr int D1 = 2;
constexpr int SPI_MODE3 = 3;
constexpr uint16_t ST77XX_BLACK = 0x0000;
constexpr uint16_t ST77XX_WHITE = 0xFFFF;
constexpr uint16_t ST77XX_RED = 0xF800;
constexpr uint16_t ST77XX_GREEN = 0x07E0;
constexpr uint16_t ST77XX_BLUE = 0x001F;

inline void delay(unsigned long milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

inline unsigned long millis() {
  using namespace std::chrono;
  static const auto started = steady_clock::now();
  const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - started).count();
  return static_cast<uint32_t>(elapsed);
}

// A small subset of the ST7789 API for drawing into the preview framebuffer.
class Adafruit_ST7789 {
public:
  Adafruit_ST7789(int, int, int) {}
  void init(int, int, int = SPI_MODE3) {}
  void setSPISpeed(uint32_t frequency) {
    spiFrequency = frequency ? frequency : defaultSPIFrequency;
  }
  int width() const { return preview::size; }
  int height() const { return preview::size; }

  void fillScreen(uint16_t color) {
    fillRect(0, 0, width(), height(), color);
  }

  void drawPixel(int x, int y, uint16_t color) {
    fillRect(x, y, 1, 1, color);
  }

  void fillRect(int x, int y, int w, int h, uint16_t color) {
    const int right = std::min(x + w, width());
    const int bottom = std::min(y + h, height());
    const int left = std::max(x, 0);
    const int top = std::max(y, 0);
    if (left >= right || top >= bottom) {
      return;
    }
    waitForPixels((right - left) * (bottom - top));
    const std::lock_guard lock(preview::framebufferMutex);
    for (int row = top; row < bottom; ++row) {
      auto start = preview::framebuffer.begin() + row * width();
      std::fill(start + left, start + right, color);
    }
    ++preview::revision;
  }

  void drawRGBBitmap(int x, int y, const uint16_t *pixels, int w, int h) {
    const int right = std::min(x + w, width());
    const int bottom = std::min(y + h, height());
    const int left = std::max(x, 0);
    const int top = std::max(y, 0);
    if (left >= right || top >= bottom) {
      return;
    }
    waitForPixels((right - left) * (bottom - top));
    const std::lock_guard lock(preview::framebufferMutex);
    for (int row = top; row < bottom; ++row) {
      const auto *source = pixels + (row - y) * w + (left - x);
      auto destination = preview::framebuffer.begin() + row * width() + left;
      std::copy_n(source, right - left, destination);
    }
    ++preview::revision;
  }

private:
  // Default data clock in Adafruit_ST77xx.
  static constexpr uint32_t defaultSPIFrequency = 32000000;
  uint32_t spiFrequency = defaultSPIFrequency;

  void waitForPixels(int count) const {
    if (preview::spiDelayEnabled) {
      const auto transferTime = std::chrono::duration<double>(count * 16.0 / spiFrequency);
      std::this_thread::sleep_for(transferTime);
    }
  }
};
