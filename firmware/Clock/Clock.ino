#ifdef ESP32_PREVIEW
#include "ArduinoPreview.h"
#include <ctime>
#else
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <esp_timer.h>

#if !ARDUINO_USB_CDC_ON_BOOT
#error Enable Tools > USB CDC On Boot to set the clock over USB-C.
#endif
#endif

#include <cstdint>

namespace clock_face {
constexpr int screenSize = 240;
constexpr int width = 160;
constexpr int height = 36;
constexpr int left = (screenSize - width) / 2;
constexpr int top = (screenSize - height) / 2;
constexpr int secondsPerDay = 86400;

inline void render(int secondsOfDay, uint16_t *pixels) {
  for (int i = 0; i < width * height; ++i) {
    pixels[i] = 0;
  }

  auto paint = [pixels](int x, int y, int w, int h) {
    for (int row = y; row < y + h; ++row) {
      for (int column = x; column < x + w; ++column) {
        pixels[row * width + column] = 0xFFFF;
      }
    }
  };

  constexpr uint8_t masks[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                               0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  // Segments: top, upper right, lower right, bottom, lower left, upper left, middle.
  constexpr int segments[][4] = {
      {4, 0, 12, 4}, {16, 4, 4, 12}, {16, 20, 4, 12}, {4, 32, 12, 4},
      {0, 20, 4, 12}, {0, 4, 4, 12}, {4, 16, 12, 4},
  };

  const int hours = secondsOfDay / 3600;
  const int minutes = (secondsOfDay / 60) % 60;
  const int seconds = secondsOfDay % 60;
  const int digits[] = {hours / 10, hours % 10, minutes / 10,
                        minutes % 10, seconds / 10, seconds % 10};

  int x = 0;
  for (int i = 0; i < 6; ++i) {
    if (i == 2 || i == 4) {
      paint(x + 1, 10, 4, 4);
      paint(x + 1, 22, 4, 4);
      x += 10;
    }
    const uint8_t mask = secondsOfDay < 0 ? 0x40 : masks[digits[i]];
    for (int segment = 0; segment < 7; ++segment) {
      if (mask & (1 << segment)) {
        const auto &rect = segments[segment];
        paint(x + rect[0], rect[1], rect[2], rect[3]);
      }
    }
    x += 24;
  }
}
}  // namespace clock_face

// Hardware SPI uses D8 (SCK) and D10 (MOSI). The display has no CS pin.
Adafruit_ST7789 screen(-1, D0, D1);
uint16_t timePixels[clock_face::width * clock_face::height];
int lastDisplayedSecond = -1;

void drawTime(int secondsOfDay) {
  clock_face::render(secondsOfDay, timePixels);
  screen.drawRGBBitmap(clock_face::left, clock_face::top, timePixels,
                       clock_face::width, clock_face::height);
}

#ifndef ESP32_PREVIEW
int secondsAtSync = 0;
int64_t syncTimeUs = -1;

bool setClock(const String &text) {
  if (text.length() != 8 || text[2] != ':' || text[5] != ':') {
    return false;
  }
  for (const int index : {0, 1, 3, 4, 6, 7}) {
    if (text[index] < '0' || text[index] > '9') {
      return false;
    }
  }
  const int hours = (text[0] - '0') * 10 + (text[1] - '0');
  const int minutes = (text[3] - '0') * 10 + (text[4] - '0');
  const int seconds = (text[6] - '0') * 10 + (text[7] - '0');
  if (hours >= 24 || minutes >= 60 || seconds >= 60) {
    return false;
  }

  secondsAtSync = hours * 3600 + minutes * 60 + seconds;
  syncTimeUs = esp_timer_get_time();
  return true;
}

int currentSecondsOfDay() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.println(setClock(input) ? "Time set" : "Send time as HH:MM:SS (24-hour format)");
  }
  if (syncTimeUs < 0) {
    return -1;
  }
  const int64_t elapsedSeconds = (esp_timer_get_time() - syncTimeUs) / 1000000;
  return (secondsAtSync + elapsedSeconds) % clock_face::secondsPerDay;
}
#else
int currentSecondsOfDay() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  return localtime_r(&now, &local) ? local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec : -1;
}
#endif

void setup() {
#ifndef ESP32_PREVIEW
  Serial.begin(115200);
  Serial.setTimeout(250);
#endif
  screen.init(clock_face::screenSize, clock_face::screenSize, SPI_MODE3);
  screen.setSPISpeed(10000000);
  screen.fillScreen(ST77XX_BLACK);
  drawTime(-1);
}

void loop() {
  const int seconds = currentSecondsOfDay();
  if (seconds != lastDisplayedSecond) {
    drawTime(seconds);
    lastDisplayedSecond = seconds;
  }
  delay(10);
}
