#ifdef USE_M5STICKC_PLUS_DISPLAY

#include "display_m5stick.h"
#include "m5stick_anim.h"
#include "session.h"
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <math.h>

#define M5_AXP192_I2C_ADDR 0x34
#define M5_SDA_PIN 21
#define M5_SCL_PIN 22
#define M5_BUTTON_A_PIN 37 // Front M5 button
#define M5_BUTTON_B_PIN 39 // Side button
#define M5_LED_PIN 10      // Active-low LED
#define M5_BUZZER_PIN 2    // Built-in buzzer

#define TFT_WIDTH_PX 240
#define TFT_HEIGHT_PX 135
#define RADAR_CX 67
#define RADAR_CY 76
#define RADAR_RADIUS 48

// M5Stick ST7789 panel is BGR. HW_RED (0xF800) renders blue.
// HW_RED is the byte-swapped value that actually displays as red.
#define HW_RED       0x001F
#define HW_DKRED     0x0010
#define HW_GREEN     0x07E0
#define HW_YELLOW    0x07FF
#define HW_ORANGE    0x041F
#define HW_CYAN      0xFFE0
#define HW_DARKGREY  0x39E7

static TFT_eSPI tft = TFT_eSPI();

static unsigned long alertUntilMs = 0;
static uint8_t idleCh = 1;
static int idleDetCount = 0;
static bool inAlert = false;
static uint8_t displayMode = 0; // 0:Bird 1:Radar 2:Spectrum 3:Info 4:Captures 5:Stealth
#define NUM_DISPLAY_MODES 6

enum CapturesSubView {
  CAPTURES_VIEW_LIST,
  CAPTURES_VIEW_DETAIL
};
static CapturesSubView capturesView = CAPTURES_VIEW_LIST;
static int selectedCaptureIdx = 0;
static int captureScrollOffset = 0;
static bool capturesNeedRedraw = true;
static unsigned long lastFrameTick = 0;
static float radarAngleDeg = 0.0f;
static float cachedBattVoltage = 4.10f;
static unsigned long lastBattRead = 0;

// Last alert details
static char lastMethod[32] = {0};
static char lastMac[24] = {0};
static int8_t lastRssi = -90;
static uint8_t lastChannel = 1;

// Channel hit counters for mini spectrum
static uint16_t chHits1 = 0;
static uint16_t chHits6 = 0;
static uint16_t chHits11 = 0;
static uint32_t totalDetections = 0;

// --- AXP192 Power Management Helpers ---

static void axpWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(M5_AXP192_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t axpRead(uint8_t reg) {
  Wire.beginTransmission(M5_AXP192_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)M5_AXP192_I2C_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

static void axpInit() {
  Wire.begin(M5_SDA_PIN, M5_SCL_PIN, 400000);
  
  // Register 0x12: Turn on DCDC1 (ESP32 3.3V), LDO2 (LCD backlight), LDO3 (LCD logic), EXTEN
  axpWrite(0x12, 0x4D);
  
  // Register 0x28: Set LDO2 (backlight) to ~3.0V (0xC) and LDO3 (LCD logic) to ~3.0V (0xC)
  axpWrite(0x28, 0xCC);
  
  // Register 0x82: Enable ADC for battery voltage & current measurement
  axpWrite(0x82, 0xFF);
  
  // Register 0x33: Set battery charging current to 100mA (for 120mAh LiPo)
  axpWrite(0x33, 0xC0);
}

float m5stickGetBatteryVoltage() {
  unsigned long now = millis();
  if (now - lastBattRead < 3000 && lastBattRead != 0) {
    return cachedBattVoltage;
  }
  lastBattRead = now;
  
  Wire.beginTransmission(M5_AXP192_I2C_ADDR);
  Wire.write(0x78);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)M5_AXP192_I2C_ADDR, (uint8_t)2);
  if (Wire.available() >= 2) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    uint16_t raw = (hi << 4) | (lo & 0x0F);
    cachedBattVoltage = raw * 1.1f / 1000.0f;
  }
  return cachedBattVoltage;
}

static void setBacklight(bool on) {
  uint8_t reg12 = axpRead(0x12);
  if (on) {
    reg12 |= (1 << 2); // LDO2 on
  } else {
    reg12 &= ~(1 << 2); // LDO2 off
  }
  axpWrite(0x12, reg12);
}

// --- Animation & Drawing Routines ---


void m5stickDisplayInit() {
  axpInit();
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  pinMode(M5_BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(M5_BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(M5_LED_PIN, OUTPUT);
  digitalWrite(M5_LED_PIN, HIGH);
}

void m5stickDisplayShowBoot() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(HW_RED, TFT_BLACK);
  
  // Tactical cyber sweep
  for (int y = 0; y < TFT_HEIGHT_PX; y += 5) {
    tft.drawFastHLine(0, y, TFT_WIDTH_PX, HW_RED);
    delay(10);
    tft.fillScreen(TFT_BLACK);
  }

  // Crosshair lock-on animation
  for (int r = 100; r > 10; r -= 15) {
    tft.fillScreen(TFT_BLACK);
    tft.drawCircle(TFT_WIDTH_PX/2, TFT_HEIGHT_PX/2, r, HW_RED);
    tft.drawFastHLine(0, TFT_HEIGHT_PX/2, TFT_WIDTH_PX, TFT_DARKGREY);
    tft.drawFastVLine(TFT_WIDTH_PX/2, 0, TFT_HEIGHT_PX, TFT_DARKGREY);
    delay(40);
  }
  
  // Console boot text
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("W.D.F. TACTICAL OS v2.0", 10, 10, 2);
  delay(200);
  tft.drawString("> BOOT_SEQ: INIT", 10, 35, 2);
  delay(200);
  tft.drawString("> RADIO: PROMISCUOUS", 10, 60, 2);
  delay(200);
  tft.drawString("> TARGETING: FLOCK", 10, 85, 2);
  delay(400);

  // Flash red to finish boot
  tft.fillScreen(HW_RED);
  delay(50);
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SYSTEM READY", TFT_WIDTH_PX/2, TFT_HEIGHT_PX/2, 4);
  delay(1000);
}

void m5stickDisplayShowIdle(uint8_t ch, int detCount) {
  idleCh = ch;
  idleDetCount = detCount;
  if (!inAlert) {
    if (ch == 1) chHits1++;
    else if (ch == 6) chHits6++;
    else if (ch == 11) chHits11++;
  }
}


// Extended alert details for rich tactical HUD
struct AlertState {
  char protocol[10];
  char name[32];
  char mac[24];
  char vendor[32];
  char method[24];
  char verdict[24];
  int8_t rssi;
  float distM;
  uint8_t confidence;
  uint8_t channel;
  uint16_t hits;
  unsigned long startedAt;
  unsigned long untilMs;
  int8_t lastRenderedRssi;
  uint16_t lastRenderedHits;
  int lastRenderedSecs;
  bool fullDrawn;
};
static AlertState alertData = {};

static void drawAlertHUDDynamic(unsigned long now);

static void drawAlertHUDFull() {
  tft.fillScreen(TFT_BLACK);

  // 1. Header Banner (Y=0..19)
  tft.fillRect(0, 0, TFT_WIDTH_PX, 19, HW_RED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_RED);
  tft.drawString("[!] TARGET: FLOCK DETECTED", 6, 9, 2);

  tft.setTextDatum(MR_DATUM);
  String badge = String("[") + alertData.protocol + "] " + String(alertData.confidence) + "%";
  tft.drawString(badge, TFT_WIDTH_PX - 6, 9, 2);

  // 2. Identity Card (Y=21..65, H=44)
  tft.drawRect(2, 21, 236, 44, HW_DKRED);
  tft.drawRect(3, 22, 234, 42, HW_DKRED);

  // Row 1 (Y=24): Device Name
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  tft.drawString("NAME:", 6, 24, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(alertData.name, 42, 24, 2);

  // Row 2 (Y=40): MAC address (Font 2) + Channel
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  tft.drawString("MAC: ", 6, 40, 2);
  tft.setTextColor(HW_CYAN, TFT_BLACK);
  tft.drawString(alertData.mac, 42, 40, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  if (alertData.channel > 0) {
    tft.drawString(String("CH ") + alertData.channel, 232, 40, 2);
  } else {
    tft.drawString("BLE-ADV", 232, 40, 2);
  }

  // Row 3 (Y=54): Vendor & Trigger Method
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  tft.drawString("MFR: ", 6, 54, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(alertData.vendor, 34, 54, 1);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  tft.drawString(String("MET: ") + alertData.method, 232, 54, 1);

  // 3. Telemetry & Proximity Card (Y=67..113, H=46)
  tft.drawRect(2, 67, 236, 46, HW_DKRED);
  tft.drawRect(3, 68, 234, 44, HW_DKRED);

  // Row 1 (Y=70): Static labels
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  tft.drawString("RSSI:", 6, 70, 1);
  tft.drawString("DIST:", 88, 70, 1);

  // Row 3 (Y=100): Verdict
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.drawString(alertData.verdict, 6, 100, 1);

  // 4. Footer Bar (Y=115..134, H=20)
  tft.fillRect(0, 115, TFT_WIDTH_PX, 20, HW_DKRED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_DKRED);
  tft.drawString("[A] DISMISS", 6, 124, 1);

  tft.setTextDatum(MR_DATUM);
  float vbat = m5stickGetBatteryVoltage();
  tft.drawString(String("BAT:") + String(vbat, 1) + "V", 234, 124, 1);

  alertData.fullDrawn = true;
  alertData.lastRenderedRssi = -128;
  alertData.lastRenderedHits = 0;
  alertData.lastRenderedSecs = -1;

  // Render initial dynamic values
  drawAlertHUDDynamic(millis());
}

static void drawAlertHUDDynamic(unsigned long now) {
  if (!alertData.fullDrawn) {
    drawAlertHUDFull();
    return;
  }

  // 1. Countdown timer (updated once per second)
  int remSecs = 0;
  if ((long)(alertData.untilMs - now) > 0) {
    remSecs = (int)((alertData.untilMs - now) / 1000);
  }
  if (remSecs != alertData.lastRenderedSecs) {
    alertData.lastRenderedSecs = remSecs;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, HW_DKRED);
    char timerBuf[20];
    snprintf(timerBuf, sizeof(timerBuf), "RESET: %ds  ", remSecs);
    tft.drawString(timerBuf, 120, 124, 1);
  }

  // 2. Check if RSSI or hits changed
  if (alertData.rssi != alertData.lastRenderedRssi || alertData.hits != alertData.lastRenderedHits) {
    alertData.lastRenderedRssi = alertData.rssi;
    alertData.lastRenderedHits = alertData.hits;

    // Draw RSSI value (Y=70)
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm  ", alertData.rssi);
    tft.drawString(rssiBuf, 38, 70, 1);

    // Draw Distance value (Y=70)
    char distBuf[16];
    if (alertData.distM < 0) {
      snprintf(distBuf, sizeof(distBuf), "? m   ");
    } else if (alertData.distM < 1.0f) {
      snprintf(distBuf, sizeof(distBuf), "%.2fm ", alertData.distM);
    } else {
      snprintf(distBuf, sizeof(distBuf), "%.1fm ", alertData.distM);
    }
    tft.setTextColor(HW_YELLOW, TFT_BLACK);
    tft.drawString(distBuf, 120, 70, 1);

    // Proximity indicator tag (Y=70, right side)
    tft.setTextDatum(TR_DATUM);
    if (alertData.distM >= 0 && alertData.distM < 1.5f) {
      tft.setTextColor(HW_RED, TFT_BLACK);
      tft.drawString("[IMMEDIATE] ", 232, 70, 1);
    } else if (alertData.distM >= 0 && alertData.distM < 4.0f) {
      tft.setTextColor(HW_ORANGE, TFT_BLACK);
      tft.drawString("[VERY CLOSE]", 232, 70, 1);
    } else if (alertData.distM >= 0 && alertData.distM < 10.0f) {
      tft.setTextColor(HW_YELLOW, TFT_BLACK);
      tft.drawString("[NEARBY]    ", 232, 70, 1);
    } else {
      tft.setTextColor(HW_GREEN, TFT_BLACK);
      tft.drawString("[IN RANGE]  ", 232, 70, 1);
    }

    // Draw 16-segment tactical signal bar (Y=82)
    int barX = 6;
    int barY = 82;
    int segW = 9;
    int segH = 8;
    int segGap = 2;
    int activeSegs = map(constrain((int)alertData.rssi, -95, -35), -95, -35, 0, 16);

    for (int i = 0; i < 16; i++) {
      int sx = barX + i * (segW + segGap);
      if (i < activeSegs) {
        uint16_t segColor;
        if (i < 5) segColor = HW_GREEN;
        else if (i < 11) segColor = HW_YELLOW;
        else segColor = HW_RED;
        tft.fillRect(sx, barY, segW, segH, segColor);
      } else {
        tft.fillRect(sx, barY, segW, segH, TFT_BLACK);
        tft.drawRect(sx, barY, segW, segH, HW_DKRED);
      }
    }

    // Signal percentage
    int sigPct = map(constrain((int)alertData.rssi, -95, -35), -95, -35, 0, 100);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char sigBuf[12];
    snprintf(sigBuf, sizeof(sigBuf), "%3d%%", sigPct);
    tft.drawString(sigBuf, 232, 82, 1);

    // Hits & Total update (Y=100)
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char hitsBuf[24];
    snprintf(hitsBuf, sizeof(hitsBuf), "HITS:%d  TOT:%d ", alertData.hits, idleDetCount);
    tft.drawString(hitsBuf, 232, 100, 1);
  }
}

void m5stickDisplayShowAlertRich(const char* protocol, const char* name, const char* mac,
                                 const char* vendor, const char* method, const char* verdict,
                                 int8_t rssi, float distM, uint8_t confidence,
                                 uint8_t ch, unsigned long alertMs) {
  idleCh = ch;
  inAlert = true;
  if (alertMs == 0) alertMs = 5000;
  alertUntilMs = millis() + alertMs;

  strncpy(alertData.protocol, protocol ? protocol : "BLE", sizeof(alertData.protocol) - 1);
  strncpy(alertData.name, (name && strlen(name) > 0) ? name : "Flock Device", sizeof(alertData.name) - 1);
  strncpy(alertData.mac, mac ? mac : "00:00:00:00:00:00", sizeof(alertData.mac) - 1);
  strncpy(alertData.vendor, vendor ? vendor : "Flock Safety", sizeof(alertData.vendor) - 1);
  strncpy(alertData.method, method ? method : "UNKNOWN", sizeof(alertData.method) - 1);
  strncpy(alertData.verdict, verdict ? verdict : "FLOCK_LIKELY", sizeof(alertData.verdict) - 1);
  alertData.rssi = rssi;
  alertData.distM = distM;
  alertData.confidence = confidence;
  alertData.channel = ch;
  alertData.hits = 1;
  alertData.startedAt = millis();
  alertData.untilMs = alertUntilMs;
  alertData.fullDrawn = false;
  alertData.lastRenderedRssi = -128;
  alertData.lastRenderedHits = 0;
  alertData.lastRenderedSecs = -1;

  strncpy(lastMac, alertData.mac, sizeof(lastMac) - 1);
  strncpy(lastMethod, alertData.method, sizeof(lastMethod) - 1);
  lastRssi = rssi;
  lastChannel = ch;

  digitalWrite(M5_LED_PIN, LOW); // LED ON (active-low)
  drawAlertHUDFull();
}

void m5stickDisplayUpdateAlertLive(int8_t rssi, float distM, uint16_t hits) {
  if (!inAlert) return;
  alertData.rssi = rssi;
  alertData.distM = distM;
  alertData.hits = hits;
  // Keep alert active while tracking live signal
  alertUntilMs = millis() + 5000;
  alertData.untilMs = alertUntilMs;
}

void m5stickDisplayShowAlert(const char* method, const char* mac, int8_t rssi,
                             uint8_t ch, unsigned long alertMs) {
  bool isBle = (method && strstr(method, "BLE") != nullptr);
  const char* proto = isBle ? "BLE" : "WIFI";
  const char* name = isBle ? "FS Ext Battery" : "Flock Cam ALPR";
  const char* vendor = isBle ? "Flock Safety (0x09C8)" : "Flock Safety (OUI)";
  const char* verdict = "FLOCK_LIKELY";
  float dist = (rssi == 0) ? -1.0f : powf(10.0f, (-40.0f - (float)rssi) / 20.0f);
  uint8_t conf = 100;

  m5stickDisplayShowAlertRich(proto, name, mac, vendor, method, verdict, rssi, dist, conf, ch, alertMs);
}

bool m5stickDisplayInAlert(unsigned long now) {
  return inAlert && alertUntilMs != 0 && (long)(now - alertUntilMs) < 0;
}

void m5stickCycleDisplayMode() {
  displayMode = (displayMode + 1) % NUM_DISPLAY_MODES;
  tft.fillScreen(TFT_BLACK);
  capturesView = CAPTURES_VIEW_LIST;
  capturesNeedRedraw = true;
  if (displayMode == 5) {
    setBacklight(false); // Stealth Mode
  } else {
    setBacklight(true);
  }
}

void m5stickToggleMute() {
  uint8_t mask = wdfBeepMask;
  uint8_t newMask = (mask == 0) ? 0x1F : 0x00;
  wdfBeepMask = newMask;
  WhereDaFlockSession::saveBeepMask();
}

// --- Mode 0: Bird Animation TUI ---
static void drawBirdTUI(unsigned long now, uint8_t ch, int detCount) {
  static unsigned long lastAnimFrameAt = 0;
  static int currentAnimFrame = 0;
  if (now - lastAnimFrameAt >= 40) {
    lastAnimFrameAt = now;
    tft.setSwapBytes(true);
    tft.pushImage(0, 0, WhereDaFlockAnimation::DISPLAY_WIDTH, WhereDaFlockAnimation::DISPLAY_HEIGHT, (const uint16_t*)WhereDaFlockAnimation::anim_frames[currentAnimFrame]);
    currentAnimFrame = (currentAnimFrame + 1) % WhereDaFlockAnimation::ANIM_FRAMES;

    float vbat = m5stickGetBatteryVoltage();
    bool isMuted = (wdfBeepMask == 0);

    tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("BAT:" + String(vbat, 1) + "V", 5, 5, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(isMuted ? "MUTED" : "SND:ON", TFT_WIDTH_PX - 5, 5, 1);

    tft.setTextDatum(BL_DATUM);
    tft.drawString("CH1:" + String(chHits1), 5, TFT_HEIGHT_PX - 2, 1);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("CH6:" + String(chHits6), TFT_WIDTH_PX / 2, TFT_HEIGHT_PX - 2, 1);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("CH11:" + String(chHits11), TFT_WIDTH_PX - 5, TFT_HEIGHT_PX - 2, 1);

    tft.setTextDatum(MC_DATUM);
    tft.drawString("SCANNING CH " + String(ch), TFT_WIDTH_PX / 2, 110, 2);
    if (detCount > 0) {
      tft.setTextColor(HW_RED, TFT_TRANSPARENT);
      tft.drawString("TARGET LOCK: " + String(detCount), TFT_WIDTH_PX / 2, 90, 2);
    }
  }
}

// --- Mode 1: Radar HUD ---
static void drawRadarHUD(unsigned long now, uint8_t ch, int detCount) {
  if (now - lastFrameTick < 50) return;
  lastFrameTick = now;

  tft.fillScreen(TFT_BLACK);

  int cx = 68;
  int cy = 68;
  int maxR = 58;

  // Concentric range rings
  for (int r = maxR; r > 0; r -= 19) {
    tft.drawCircle(cx, cy, r, TFT_DARKGREY);
  }
  // Crosshairs
  tft.drawFastHLine(cx - maxR, cy, maxR * 2, TFT_DARKGREY);
  tft.drawFastVLine(cx, cy - maxR, maxR * 2, TFT_DARKGREY);

  // Rotating sweep line
  radarAngleDeg += 8.0f;
  if (radarAngleDeg >= 360.0f) radarAngleDeg -= 360.0f;
  float rad = radarAngleDeg * PI / 180.0f;
  int sx = cx + (int)(maxR * cos(rad));
  int sy = cy - (int)(maxR * sin(rad));
  tft.drawLine(cx, cy, sx, sy, HW_RED);
  // Fading trail
  for (int t = 1; t <= 3; t++) {
    float trad = (radarAngleDeg - t * 8.0f) * PI / 180.0f;
    int tx = cx + (int)(maxR * cos(trad));
    int ty = cy - (int)(maxR * sin(trad));
    tft.drawLine(cx, cy, tx, ty, HW_DKRED);
  }

  // Simulated blips based on detection count
  if (detCount > 0) {
    for (int i = 0; i < min(detCount, 5); i++) {
      float bAngle = (radarAngleDeg - 20.0f - i * 35.0f) * PI / 180.0f;
      int bDist = 15 + (i * 11) % maxR;
      if (bDist > maxR - 5) bDist = maxR - 10;
      int bx = cx + (int)(bDist * cos(bAngle));
      int by = cy - (int)(bDist * sin(bAngle));
      tft.fillCircle(bx, by, 3, HW_RED);
    }
  }

  // Center dot
  tft.fillCircle(cx, cy, 2, HW_RED);

  // Right side info panel
  int rx = 145;
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("RADAR", rx, 5, 2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("CH: " + String(ch), rx, 28, 2);
  tft.drawString("DET: " + String(detCount), rx, 48, 2);
  tft.drawString("TOT: " + String(totalDetections), rx, 68, 2);

  float vbat = m5stickGetBatteryVoltage();
  int pct = constrain((int)((vbat - 3.2f) / 0.9f * 100), 0, 100);
  tft.drawString("BAT: " + String(pct) + "%", rx, 88, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(String(radarAngleDeg, 0) + (char)0xF8, rx, 110, 1);
}

// --- Mode 2: Spectrum / Channel Monitor ---
static void drawSpectrumHUD(unsigned long now, uint8_t ch, int detCount) {
  if (now - lastFrameTick < 100) return;
  lastFrameTick = now;

  tft.fillScreen(TFT_BLACK);

  // Title
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("SPECTRUM MONITOR", TFT_WIDTH_PX / 2, 3, 2);

  // Bar graph for channels 1, 6, 11
  uint16_t maxHits = max(max(chHits1, chHits6), chHits11);
  if (maxHits == 0) maxHits = 1;

  int barW = 50;
  int barMaxH = 75;
  int baseY = 120;
  int gap = 15;
  int startX = (TFT_WIDTH_PX - (3 * barW + 2 * gap)) / 2;

  uint16_t hits[3] = {chHits1, chHits6, chHits11};
  const char* labels[3] = {"CH 1", "CH 6", "CH 11"};
  uint8_t chs[3] = {1, 6, 11};

  for (int i = 0; i < 3; i++) {
    int x = startX + i * (barW + gap);
    int h = (int)((float)hits[i] / maxHits * barMaxH);
    if (h < 2 && hits[i] > 0) h = 2;

    // Bar fill
    uint16_t color = (chs[i] == ch) ? HW_RED : TFT_DARKGREY;
    tft.fillRect(x, baseY - h, barW, h, color);
    // Bar outline
    tft.drawRect(x, baseY - barMaxH, barW, barMaxH, TFT_DARKGREY);

    // Label
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(labels[i], x + barW / 2, baseY + 3, 1);

    // Count above bar
    tft.setTextDatum(BC_DATUM);
    tft.drawString(String(hits[i]), x + barW / 2, baseY - h - 2, 1);
  }

  // Active channel indicator
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SCAN: CH" + String(ch), 5, 22, 1);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("TOTAL: " + String(totalDetections), TFT_WIDTH_PX - 5, 22, 1);
}

// --- Mode 3: System Info Panel ---
static void drawInfoHUD(unsigned long now, uint8_t ch, int detCount) {
  if (now - lastFrameTick < 250) return;
  lastFrameTick = now;

  tft.fillScreen(TFT_BLACK);

  // Title bar
  tft.fillRect(0, 0, TFT_WIDTH_PX, 20, HW_DKRED);
  tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WHEREDAFLOCK INFO", TFT_WIDTH_PX / 2, 10, 2);

  // Info rows
  int y = 28;
  int lineH = 16;
  tft.setTextDatum(TL_DATUM);

  // Battery
  float vbat = m5stickGetBatteryVoltage();
  int pct = constrain((int)((vbat - 3.2f) / 0.9f * 100), 0, 100);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("BATTERY", 8, y, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(vbat, 2) + "V (" + String(pct) + "%)", 80, y, 1);
  y += lineH;

  // Uptime
  unsigned long secs = now / 1000;
  int hh = secs / 3600;
  int mm = (secs % 3600) / 60;
  int ss = secs % 60;
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("UPTIME", 8, y, 1);
  char uptBuf[16];
  snprintf(uptBuf, sizeof(uptBuf), "%02d:%02d:%02d", hh, mm, ss);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(uptBuf, 80, y, 1);
  y += lineH;

  // Current channel
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("CHANNEL", 8, y, 1);
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.drawString(String(ch), 80, y, 1);
  y += lineH;

  // Detections
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("DETECTS", 8, y, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(totalDetections), 80, y, 1);
  y += lineH;

  // Free heap
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("FREE MEM", 8, y, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(ESP.getFreeHeap() / 1024) + " KB", 80, y, 1);
  y += lineH;

  // Sound status
  bool isMuted = (wdfBeepMask == 0);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("SOUND", 8, y, 1);
  tft.setTextColor(isMuted ? HW_RED : TFT_GREEN, TFT_BLACK);
  tft.drawString(isMuted ? "MUTED" : "ACTIVE", 80, y, 1);

  // Right column - last alert
  int rx = 140;
  y = 28;
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.drawString("LAST ALERT", rx, y, 1);
  y += lineH;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (lastMac[0] != 0) {
    tft.drawString(lastMac, rx, y, 1);
    y += lineH;
    tft.drawString(String(lastRssi) + " dBm", rx, y, 1);
    y += lineH;
    tft.drawString("CH " + String(lastChannel), rx, y, 1);
  } else {
    tft.drawString("NONE", rx, y, 1);
  }
}

// --- Mode 4: Captures Tab (List & Detail Inspector) ---
static void drawCapturesList(unsigned long now) {
  static unsigned long lastListTick = 0;
  if (now - lastListTick >= 1000) {
    lastListTick = now;
    capturesNeedRedraw = true;
  }

  if (!capturesNeedRedraw) return;
  capturesNeedRedraw = false;

  tft.fillScreen(TFT_BLACK);

  // Top Header Bar
  tft.fillRect(0, 0, TFT_WIDTH_PX, 19, HW_DKRED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_DKRED);
  tft.drawString("CAPTURES LOG", 6, 9, 2);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(HW_YELLOW, HW_DKRED);
  char countBuf[24];
  snprintf(countBuf, sizeof(countBuf), "TOTAL: %d", wdfDetCount);
  tft.drawString(countBuf, TFT_WIDTH_PX - 6, 9, 2);

  if (wdfDetCount == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(HW_RED, TFT_BLACK);
    tft.drawString("NO CAPTURES RECORDED", TFT_WIDTH_PX / 2, 52, 2);
    tft.setTextColor(HW_DARKGREY, TFT_BLACK);
    tft.drawString("Monitoring 2.4GHz WiFi & BLE...", TFT_WIDTH_PX / 2, 75, 1);
    tft.drawString("Flock cameras will appear here.", TFT_WIDTH_PX / 2, 90, 1);

    tft.fillRect(0, 116, TFT_WIDTH_PX, 19, HW_DKRED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, HW_DKRED);
    tft.drawString("[A] NEXT TAB", TFT_WIDTH_PX / 2, 125, 1);
    return;
  }

  int totalItems = wdfDetCount + 1;
  if (selectedCaptureIdx >= totalItems) selectedCaptureIdx = totalItems - 1;
  if (selectedCaptureIdx < 0) selectedCaptureIdx = 0;

  if (selectedCaptureIdx < captureScrollOffset) {
    captureScrollOffset = selectedCaptureIdx;
  }
  if (selectedCaptureIdx >= captureScrollOffset + 3) {
    captureScrollOffset = selectedCaptureIdx - 2;
  }
  if (captureScrollOffset < 0) captureScrollOffset = 0;

  for (int slot = 0; slot < 3; slot++) {
    int itemIdx = captureScrollOffset + slot;
    if (itemIdx >= totalItems) break;

    int itemY = 22 + slot * 31;
    bool isSel = (itemIdx == selectedCaptureIdx);

    if (itemIdx < wdfDetCount) {
      WDFDetection& det = wdfDet[itemIdx];

      if (isSel) {
        tft.fillRect(2, itemY, 236, 29, HW_DKRED);
        tft.drawRect(2, itemY, 236, 29, HW_RED);
        tft.drawRect(3, itemY + 1, 234, 27, HW_RED);
      } else {
        tft.fillRect(2, itemY, 236, 29, TFT_BLACK);
        tft.drawRect(2, itemY, 236, 29, HW_DKRED);
      }

      tft.setTextDatum(TL_DATUM);
      uint16_t nameColor = isSel ? HW_YELLOW : TFT_WHITE;
      tft.setTextColor(nameColor, isSel ? HW_DKRED : TFT_BLACK);
      char titleBuf[48];
      snprintf(titleBuf, sizeof(titleBuf), "%s#%d [%s] %s", isSel ? "> " : "  ", itemIdx + 1, det.protocol, det.name);
      tft.drawString(titleBuf, 6, itemY + 3, 1);

      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(isSel ? TFT_WHITE : HW_RED, isSel ? HW_DKRED : TFT_BLACK);
      char rssiBuf[16];
      snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", det.rssi);
      tft.drawString(rssiBuf, 232, itemY + 3, 1);

      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(HW_CYAN, isSel ? HW_DKRED : TFT_BLACK);
      char macBuf[24];
      snprintf(macBuf, sizeof(macBuf), "   %s", det.mac);
      tft.drawString(macBuf, 6, itemY + 16, 1);

      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(HW_DARKGREY, isSel ? HW_DKRED : TFT_BLACK);
      unsigned long secsAgo = (now >= det.lastSeen) ? (now - det.lastSeen) / 1000 : 0;
      char statBuf[32];
      if (secsAgo < 60) snprintf(statBuf, sizeof(statBuf), "%u hits | %lus ago", det.count, secsAgo);
      else snprintf(statBuf, sizeof(statBuf), "%u hits | %lum ago", det.count, secsAgo / 60);
      tft.drawString(statBuf, 232, itemY + 16, 1);

    } else {
      if (isSel) {
        tft.fillRect(2, itemY, 236, 29, HW_DKRED);
        tft.drawRect(2, itemY, 236, 29, HW_YELLOW);
        tft.setTextColor(HW_YELLOW, HW_DKRED);
      } else {
        tft.fillRect(2, itemY, 236, 29, TFT_BLACK);
        tft.drawRect(2, itemY, 236, 29, HW_DKRED);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
      }
      tft.setTextDatum(MC_DATUM);
      tft.drawString(">> [ NEXT TAB: STEALTH ] >>", TFT_WIDTH_PX / 2, itemY + 14, 2);
    }
  }

  tft.fillRect(0, 116, TFT_WIDTH_PX, 19, HW_DKRED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_DKRED);
  tft.drawString("[B] SCROLL", 6, 125, 1);

  tft.setTextDatum(MC_DATUM);
  if (selectedCaptureIdx < wdfDetCount) {
    tft.drawString("[A] VIEW DETAILS", 120, 125, 1);
  } else {
    tft.drawString("[A] NEXT TAB", 120, 125, 1);
  }

  tft.setTextDatum(MR_DATUM);
  char posBuf[16];
  snprintf(posBuf, sizeof(posBuf), "%d/%d", selectedCaptureIdx + 1, totalItems);
  tft.drawString(posBuf, 234, 125, 1);
}

static void drawCaptureDetail(unsigned long now) {
  if (!capturesNeedRedraw) return;
  capturesNeedRedraw = false;

  if (selectedCaptureIdx >= wdfDetCount || wdfDetCount == 0) {
    capturesView = CAPTURES_VIEW_LIST;
    capturesNeedRedraw = true;
    return;
  }

  WDFDetection& det = wdfDet[selectedCaptureIdx];
  tft.fillScreen(TFT_BLACK);

  // 1. Header Banner (Y=0..19)
  tft.fillRect(0, 0, TFT_WIDTH_PX, 19, HW_RED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_RED);
  char titleBuf[32];
  snprintf(titleBuf, sizeof(titleBuf), "[!] CAPTURE #%d: FLOCK", selectedCaptureIdx + 1);
  tft.drawString(titleBuf, 6, 9, 2);

  tft.setTextDatum(MR_DATUM);
  char badgeBuf[24];
  snprintf(badgeBuf, sizeof(badgeBuf), "[%s] %u%%", det.protocol, det.confidence);
  tft.drawString(badgeBuf, TFT_WIDTH_PX - 6, 9, 2);

  // 2. Identity Card (Y=21..65, H=44)
  tft.drawRect(2, 21, 236, 44, HW_DKRED);
  tft.drawRect(3, 22, 234, 42, HW_DKRED);

  // Row 1 (Y=24): Device Name
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  tft.drawString("NAME:", 6, 24, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(det.name, 42, 24, 2);

  // Row 2 (Y=40): MAC address (Font 2) + Channel
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  tft.drawString("MAC: ", 6, 40, 2);
  tft.setTextColor(HW_CYAN, TFT_BLACK);
  tft.drawString(det.mac, 42, 40, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  if (det.channel > 0) {
    char chBuf[16];
    snprintf(chBuf, sizeof(chBuf), "CH %u", det.channel);
    tft.drawString(chBuf, 232, 40, 2);
  } else {
    tft.drawString("BLE-ADV", 232, 40, 2);
  }

  // Row 3 (Y=54): Vendor & Trigger Method
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  tft.drawString("MFR: ", 6, 54, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(det.vendor, 34, 54, 1);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(HW_DARKGREY, TFT_BLACK);
  char metBuf[32];
  snprintf(metBuf, sizeof(metBuf), "MET: %s", det.method);
  tft.drawString(metBuf, 232, 54, 1);

  // 3. Telemetry & Proximity Card (Y=67..113, H=46)
  tft.drawRect(2, 67, 236, 46, HW_DKRED);
  tft.drawRect(3, 68, 234, 44, HW_DKRED);

  // Row 1 (Y=70): RSSI, Distance, Proximity level
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  tft.drawString("RSSI:", 6, 70, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char rssiBuf[16];
  snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm  ", det.rssi);
  tft.drawString(rssiBuf, 38, 70, 1);

  tft.setTextColor(HW_YELLOW, TFT_BLACK);
  tft.drawString("DIST:", 88, 70, 1);
  char distBuf[16];
  if (det.distM < 0) snprintf(distBuf, sizeof(distBuf), "? m");
  else if (det.distM < 1.0f) snprintf(distBuf, sizeof(distBuf), "%.2fm", det.distM);
  else snprintf(distBuf, sizeof(distBuf), "%.1fm", det.distM);
  tft.drawString(distBuf, 120, 70, 1);

  tft.setTextDatum(TR_DATUM);
  if (det.distM >= 0 && det.distM < 1.5f) {
    tft.setTextColor(HW_RED, TFT_BLACK);
    tft.drawString("[IMMEDIATE]", 232, 70, 1);
  } else if (det.distM >= 0 && det.distM < 4.0f) {
    tft.setTextColor(HW_ORANGE, TFT_BLACK);
    tft.drawString("[VERY CLOSE]", 232, 70, 1);
  } else if (det.distM >= 0 && det.distM < 10.0f) {
    tft.setTextColor(HW_YELLOW, TFT_BLACK);
    tft.drawString("[NEARBY]", 232, 70, 1);
  } else {
    tft.setTextColor(HW_GREEN, TFT_BLACK);
    tft.drawString("[IN RANGE]", 232, 70, 1);
  }

  // Row 2 (Y=82): 16-Segment Tactical Signal Bar
  int barX = 6, barY = 82, segW = 9, segH = 8, segGap = 2;
  int activeSegs = map(constrain((int)det.rssi, -95, -35), -95, -35, 0, 16);
  for (int i = 0; i < 16; i++) {
    int sx = barX + i * (segW + segGap);
    if (i < activeSegs) {
      uint16_t segColor = (i < 5) ? HW_GREEN : ((i < 11) ? HW_YELLOW : HW_RED);
      tft.fillRect(sx, barY, segW, segH, segColor);
    } else {
      tft.fillRect(sx, barY, segW, segH, TFT_BLACK);
      tft.drawRect(sx, barY, segW, segH, HW_DKRED);
    }
  }
  int sigPct = map(constrain((int)det.rssi, -95, -35), -95, -35, 0, 100);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char pctBuf[12];
  snprintf(pctBuf, sizeof(pctBuf), "%3d%%", sigPct);
  tft.drawString(pctBuf, 232, 82, 1);

  // Row 3 (Y=100): Verdict & Hits & Timestamp
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(HW_RED, TFT_BLACK);
  tft.drawString(det.verdict, 6, 100, 1);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  unsigned long secsAgo = (now >= det.lastSeen) ? (now - det.lastSeen) / 1000 : 0;
  char statBuf[32];
  if (secsAgo < 60) snprintf(statBuf, sizeof(statBuf), "HITS:%u  SEEN:%lus", det.count, secsAgo);
  else snprintf(statBuf, sizeof(statBuf), "HITS:%u  SEEN:%lum", det.count, secsAgo / 60);
  tft.drawString(statBuf, 232, 100, 1);

  // 4. Footer Bar (Y=115..134, H=20)
  tft.fillRect(0, 115, TFT_WIDTH_PX, 20, HW_DKRED);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, HW_DKRED);
  tft.drawString("[A] BACK TO LIST", 6, 124, 1);

  tft.setTextDatum(MR_DATUM);
  char nextBuf[32];
  snprintf(nextBuf, sizeof(nextBuf), "[B] NEXT (%d/%d)", selectedCaptureIdx + 1, wdfDetCount);
  tft.drawString(nextBuf, 234, 124, 1);
}

static void drawCapturesTab(unsigned long now) {
  if (capturesView == CAPTURES_VIEW_LIST) {
    drawCapturesList(now);
  } else {
    drawCaptureDetail(now);
  }
}

// --- Main Tick ---
void m5stickDisplayTick(unsigned long now, uint8_t ch, int detCount) {
  static bool lastBtnA = HIGH;
  static bool lastBtnB = HIGH;
  static unsigned long btnAPressedAt = 0;
  bool btnA = digitalRead(M5_BUTTON_A_PIN);
  bool btnB = digitalRead(M5_BUTTON_B_PIN);

  // Button A (Front M5 Button)
  if (lastBtnA == HIGH && btnA == LOW) {
    btnAPressedAt = now;
  }
  if (lastBtnA == LOW && btnA == HIGH) {
    unsigned long duration = now - btnAPressedAt;
    if (inAlert) {
      inAlert = false;
      alertUntilMs = 0;
      digitalWrite(M5_LED_PIN, HIGH);
      tft.fillScreen(TFT_BLACK);
    } else if (duration >= 450) {
      // Long press: advance to next tab
      m5stickCycleDisplayMode();
    } else {
      // Short click:
      if (displayMode == 4) { // Captures Tab
        if (capturesView == CAPTURES_VIEW_DETAIL) {
          // In detail view: return to list
          capturesView = CAPTURES_VIEW_LIST;
          capturesNeedRedraw = true;
          tone(M5_BUZZER_PIN, 1800, 30);
        } else {
          // In list view:
          int totalItems = (wdfDetCount > 0) ? (wdfDetCount + 1) : 0;
          if (totalItems == 0 || selectedCaptureIdx >= wdfDetCount) {
            // No captures or [NEXT TAB] selected -> advance to next tab
            m5stickCycleDisplayMode();
          } else {
            // Open full tactical detail view for selected capture!
            capturesView = CAPTURES_VIEW_DETAIL;
            capturesNeedRedraw = true;
            tone(M5_BUZZER_PIN, 2400, 40);
          }
        }
      } else {
        // Normal modes: cycle to next tab
        m5stickCycleDisplayMode();
      }
    }
  }

  // Button B (Side Button)
  if (lastBtnB == HIGH && btnB == LOW) {
    if (displayMode == 4) { // Captures Tab
      if (capturesView == CAPTURES_VIEW_DETAIL) {
        // In detail view: flip to NEXT capture's detailed HUD!
        if (wdfDetCount > 0) {
          selectedCaptureIdx = (selectedCaptureIdx + 1) % wdfDetCount;
          capturesNeedRedraw = true;
          tone(M5_BUZZER_PIN, 2000, 25);
        }
      } else {
        // In list view: scroll down to next item!
        int totalItems = (wdfDetCount > 0) ? (wdfDetCount + 1) : 0;
        if (totalItems > 0) {
          selectedCaptureIdx = (selectedCaptureIdx + 1) % totalItems;
          capturesNeedRedraw = true;
          tone(M5_BUZZER_PIN, 1800, 20);
        }
      }
    } else {
      // Normal modes: toggle mute
      m5stickToggleMute();
      tone(M5_BUZZER_PIN, 1800, 60);
    }
  }
  lastBtnA = btnA;
  lastBtnB = btnB;

  // Track total detections
  if (detCount > 0) {
    static int lastDetCount = 0;
    if (detCount != lastDetCount) {
      totalDetections += (detCount > lastDetCount) ? (detCount - lastDetCount) : detCount;
      lastDetCount = detCount;
    }
  }

  if (inAlert) {
    if ((long)(now - alertUntilMs) >= 0) {
      inAlert = false;
      alertUntilMs = 0;
      digitalWrite(M5_LED_PIN, HIGH);
      tft.fillScreen(TFT_BLACK);
    } else {
      drawAlertHUDDynamic(now);
      return;
    }
  }

  if (displayMode == 5) return; // Stealth

  switch (displayMode) {
    case 0: drawBirdTUI(now, ch, detCount); break;
    case 1: drawRadarHUD(now, ch, detCount); break;
    case 2: drawSpectrumHUD(now, ch, detCount); break;
    case 3: drawInfoHUD(now, ch, detCount); break;
    case 4: drawCapturesTab(now); break;
  }
}
#endif // USE_M5STICKC_PLUS_DISPLAY
