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
#define HW_RED    0x001F
#define HW_DKRED  0x0010

static TFT_eSPI tft = TFT_eSPI();

static unsigned long alertUntilMs = 0;
static uint8_t idleCh = 1;
static int idleDetCount = 0;
static bool inAlert = false;
static uint8_t displayMode = 0; // 0:Bird 1:Radar 2:Spectrum 3:Info 4:Stealth
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


static void drawAlertHUD(unsigned long now) {
  tft.setTextColor(TFT_WHITE, HW_RED);
  tft.setTextDatum(MC_DATUM);
  tft.fillScreen(HW_RED);
  tft.drawString("ALERT!", TFT_WIDTH_PX/2, 25, 4);
  
  tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
  tft.drawString(lastMethod, TFT_WIDTH_PX/2, 60, 2);
  tft.drawString(lastMac, TFT_WIDTH_PX/2, 85, 2);
  tft.drawString(String(lastRssi) + " dBm | CH " + String(lastChannel), TFT_WIDTH_PX/2, 110, 2);
}

void m5stickDisplayShowAlert(const char* method, const char* mac, int8_t rssi,
                             uint8_t ch, unsigned long alertMs) {
  idleCh = ch;
  inAlert = true;
  if (alertMs == 0) alertMs = 5000;
  alertUntilMs = millis() + alertMs;
  
  strncpy(lastMethod, method ? method : "UNKNOWN", sizeof(lastMethod) - 1);
  strncpy(lastMac, mac ? mac : "00:00:00:00:00:00", sizeof(lastMac) - 1);
  lastRssi = rssi;
  lastChannel = ch;
  
  // Flash LED
  digitalWrite(M5_LED_PIN, LOW); // LED ON
  
  tft.fillScreen(TFT_BLACK);
  drawAlertHUD(millis());
}

bool m5stickDisplayInAlert(unsigned long now) {
  return inAlert && alertUntilMs != 0 && (long)(now - alertUntilMs) < 0;
}

void m5stickCycleDisplayMode() {
  displayMode = (displayMode + 1) % 5;
  tft.fillScreen(TFT_BLACK);
  if (displayMode == 4) {
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

// --- Main Tick ---
void m5stickDisplayTick(unsigned long now, uint8_t ch, int detCount) {
  static bool lastBtnA = HIGH;
  static bool lastBtnB = HIGH;
  bool btnA = digitalRead(M5_BUTTON_A_PIN);
  bool btnB = digitalRead(M5_BUTTON_B_PIN);

  if (lastBtnA == HIGH && btnA == LOW) {
    if (inAlert) {
      inAlert = false;
      alertUntilMs = 0;
      digitalWrite(M5_LED_PIN, HIGH);
      tft.fillScreen(TFT_BLACK);
    } else {
      m5stickCycleDisplayMode();
    }
  }
  if (lastBtnB == HIGH && btnB == LOW) {
    m5stickToggleMute();
    tone(M5_BUZZER_PIN, 1800, 60);
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
      if (now - lastFrameTick >= 200) {
        lastFrameTick = now;
        drawAlertHUD(now);
      }
      return;
    }
  }

  if (displayMode == 4) return; // Stealth

  switch (displayMode) {
    case 0: drawBirdTUI(now, ch, detCount); break;
    case 1: drawRadarHUD(now, ch, detCount); break;
    case 2: drawSpectrumHUD(now, ch, detCount); break;
    case 3: drawInfoHUD(now, ch, detCount); break;
  }
}
#endif // USE_M5STICKC_PLUS_DISPLAY
