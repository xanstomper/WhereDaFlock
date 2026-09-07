// WhereDaFlock - minimal hardware-abstraction layer (hal.h)
//
// Lets the same detector firmware run on ANY ESP32 board:
//   * M5Stack devices (M5StickC, M5StickC Plus 1.1/1.2, Core*, Atom, StampS3,
//     Dinometer, etc.) when WDF_USE_M5UNIFIED is defined, via the M5Unified
//     library's auto-detected LED / buzzer / button / display.
//   * Generic ESP32 boards (XIAO, DevKit, custom) via raw GPIO when not.
//
// Explicitly guarded so it compiles whether or not M5Unified is installed.
//
// Board-neutral call sites in the firmware should use only these helpers.

#ifndef WHERE_DA_FLOCK_HAL_H
#define WHERE_DA_FLOCK_HAL_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Configuration (override via platformio build_flags or -D)
// ---------------------------------------------------------------------------
#ifndef BUZZER_PIN
#define BUZZER_PIN        3      // generic ESP32 default (piezo on GPIO 3)
#endif
#ifndef LED_PIN
#define LED_PIN          21      // generic ESP32 default (onboard LED, active-low)
#endif
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH   0
#endif

// Set WDF_USE_M5UNIFIED to use M5Unified on M5Stack boards. Derived from the
// board ID unless overridden.
#if !defined(WDF_USE_M5UNIFIED) && (defined(M5STACK) || defined(ARDUINO_M5STACK_DEVKIT) || \
    defined(ARDUINO_M5STICK_C) || defined(ARDUINO_M5STACK_CORE2) || defined(ARDUINO_M5STACK_CORE))
#define WDF_USE_M5UNIFIED 1
#endif

#if WDF_USE_M5UNIFIED
#include <M5Unified.h>
#endif

namespace wdf_hal {

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
inline void ledInit() {
#if WDF_USE_M5UNIFIED
  // M5Unified managed on-board LEDs (including M5StickC's red LED on GPIO 10).
  M5.begin();
  M5.Display.setBrightness(64);
#else
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? LOW : HIGH);
#endif
}

inline void ledSet(bool on) {
#if WDF_USE_M5UNIFIED
  M5.Display.setBrightness(on ? 200 : 8);   // visual flash on M5 LCD-backed LED
  // Many M5Stick boards expose an RGB/base LED; also blink common stick LED pin.
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? LOW : HIGH);    // most M5Stick LEDs are active-low
#endif
#else
  digitalWrite(LED_PIN, on ? (LED_ACTIVE_HIGH ? HIGH : LOW)
                          : (LED_ACTIVE_HIGH ? LOW : HIGH));
#endif
}

// ---------------------------------------------------------------------------
// Buzzer / tone
// ---------------------------------------------------------------------------
inline void toneStart(uint32_t freq) {
#if WDF_USE_M5UNIFIED
  M5.Speaker.setVolume(64);
  M5.Speaker.tone(freq);
#else
  tone(BUZZER_PIN, freq);
#endif
}
inline void toneStop() {
#if WDF_USE_M5UNIFIED
  M5.Speaker.stop();
#else
  noTone(BUZZER_PIN);
#endif
}

// Play a single frequency for ms (blocking like the existing beep()).
inline void beepHz(uint32_t freq, uint32_t ms) {
  toneStart(freq);
  delay(ms);
  toneStop();
}

inline void buzzerInit() {
#if WDF_USE_M5UNIFIED
  M5.Speaker.begin();
#else
  pinMode(BUZZER_PIN, OUTPUT);
#endif
}

// ---------------------------------------------------------------------------
// Display: tiny status line (M5 shields have screens; generic boards none)
// ---------------------------------------------------------------------------
inline void displayStatus(const char* line) {
#if WDF_USE_M5UNIFIED
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(2, 4);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.println(line);
  M5.Display.endWrite();
#endif
  (void)line;
}

// ---------------------------------------------------------------------------
// Button (M5 sticks have a tactile button; generic boards optional)
// ---------------------------------------------------------------------------
inline bool anyButtonPressed() {
#if WDF_USE_M5UNIFIED
  M5.update();
  return M5.BtnA.wasPressed();
#else
  return false;
#endif
}

} // namespace wdf_hal

#endif // WHERE_DA_FLOCK_HAL_H