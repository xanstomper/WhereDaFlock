#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef USE_M5STICKC_PLUS_DISPLAY

void m5stickDisplayInit();
void m5stickDisplayShowBoot();
void m5stickDisplayShowIdle(uint8_t ch, int detCount);
void m5stickDisplayShowAlert(const char* method, const char* mac, int8_t rssi,
                             uint8_t ch, unsigned long alertMs);
void m5stickDisplayTick(unsigned long now, uint8_t ch, int detCount);
bool m5stickDisplayInAlert(unsigned long now);
float m5stickGetBatteryVoltage();
void m5stickCycleDisplayMode();
void m5stickToggleMute();

#else

static inline void m5stickDisplayInit() {}
static inline void m5stickDisplayShowBoot() {}
static inline void m5stickDisplayShowIdle(uint8_t, int) {}
static inline void m5stickDisplayShowAlert(const char*, const char*, int8_t, uint8_t,
                                          unsigned long) {}
static inline void m5stickDisplayTick(unsigned long, uint8_t, int) {}
static inline bool m5stickDisplayInAlert(unsigned long) { return false; }
static inline float m5stickGetBatteryVoltage() { return 0.0f; }
static inline void m5stickCycleDisplayMode() {}
static inline void m5stickToggleMute() {}

#endif
