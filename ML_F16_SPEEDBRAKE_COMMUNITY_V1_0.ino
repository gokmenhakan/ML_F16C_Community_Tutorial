/*********************************************************************
 * ML_F16_SPEEDBRAKE_COMMUNITY
 * COMMUNITY_EDITION_V1_0_NANO_SSD1306_DCS_BIOS
 *
 * F-16C Speedbrake Position Indicator - Community Edition
 *
 * Purpose:
 *   A simple, buildable DCS-BIOS driven speedbrake indicator for hobby use.
 *   This version is intentionally kept small and readable for an Arduino
 *   Nano plus a 0.96 inch 128x64 I2C SSD1306 OLED.
 *
 * Hardware:
 *   - Arduino Nano or compatible ATmega328P board
 *   - 0.96 inch 128x64 I2C SSD1306 OLED
 *   - DCS-BIOS serial connection over USB
 *
 * Display behaviour:
 *   - CLOSED  = diagonal striped flag
 *   - TRANSIT = simple vertical wipe between stripes and dots
 *   - OPEN    = 3 x 3 dot flag
 *
 * DCS-BIOS source:
 *   F-16C_50/SPEEDBRAKE_INDICATOR
 *   Address: 0x44d4
 *   Mask: 0xffff
 *   Max Value: 65535
 *
 * Community thresholds:
 *   - 0 to 5000       = CLOSED
 *   - 5001 to 59999   = TRANSIT
 *   - 60000 to 65535  = OPEN
 *
 * Notes:
 *   - This is the Community release. It deliberately omits Pro framework
 *     features such as extended service screens, advanced diagnostics,
 *     hidden modes, commercial support tooling, and protected build logic.
 *   - Serial diagnostics are disabled when DCS-BIOS is enabled because both
 *     use the Nano serial port.
 *   - For bench testing without DCS, set USE_DCS_BIOS to 0 and
 *     USE_TEST_VALUES to 1.
 *********************************************************************/

// -------------------------------------------------------------------
// USER CONFIGURATION
// -------------------------------------------------------------------

#define USE_DCS_BIOS          1
#define USE_TEST_VALUES       0

#define SHOW_STARTUP_SPLASH   1
#define SHOW_STANDBY_TEXT     1
#define SHOW_RAW_FOOTER       0
#define SHOW_STALE_MARKER     0

// Enable only when USE_DCS_BIOS is 0.
// The Arduino Nano cannot use the same USB serial link for both
// DCS-BIOS IRQ serial and human-readable diagnostics.
#define ENABLE_SERIAL_DEBUG   0

// -------------------------------------------------------------------
// BUILD INFO
// -------------------------------------------------------------------

#define BUILD_NAME            "ML_F16_SPEEDBRAKE_COMMUNITY"
#define BUILD_VERSION         "COMMUNITY_V1_0"
#define BUILD_DATE            "2026-06-06"
#define PANEL_ID              "F16_SPEEDBRAKE"

// -------------------------------------------------------------------
// DISPLAY CONFIGURATION
// -------------------------------------------------------------------

#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT          64
#define OLED_RESET             -1
#define OLED_ADDR            0x3C

#define IND_X                  34
#define IND_Y                  10
#define IND_W                  60
#define IND_H                  44

// -------------------------------------------------------------------
// TIMING
// -------------------------------------------------------------------

#define STARTUP_SPLASH_MS     1500
#define DCS_TIMEOUT_MS        3000
#define TEST_FRAME_MS           80
#define TEST_ENDPOINT_HOLD_MS 1200
#define SERIAL_DEBUG_MS       1000

// -------------------------------------------------------------------
// SPEEDBRAKE RAW THRESHOLDS
// -------------------------------------------------------------------

#define SB_CLOSED_MAX_RAW      5000
#define SB_OPEN_MIN_RAW       60000

// Bench animation only. These approximate the observed DCS movement:
// open around 3 seconds, close around 5 seconds.
#define TEST_OPEN_STEP_RAW     3500
#define TEST_CLOSE_STEP_RAW    2100

// -------------------------------------------------------------------
// LIBRARIES
// -------------------------------------------------------------------

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if USE_DCS_BIOS
#define DCSBIOS_IRQ_SERIAL
#include "DcsBios.h"
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------------------------------------------------
// STATE MODEL
// -------------------------------------------------------------------

enum SystemState {
  SYS_BOOT,
  SYS_STANDBY,
  SYS_LIVE,
  SYS_STALE,
  SYS_TEST
};

enum SpeedbrakeState {
  SB_CLOSED,
  SB_TRANSIT,
  SB_OPEN
};

struct DcsChannel {
  uint16_t raw;
  unsigned long lastUpdateMs;
  uint32_t updateCount;
  bool live;
};

SystemState systemState = SYS_BOOT;
SpeedbrakeState currentSbState = SB_CLOSED;
SpeedbrakeState lastRenderedSbState = SB_TRANSIT;

DcsChannel chSpeedbrake = {
  0,
  0,
  0,
  false
};

bool dcsEverSeen = false;
unsigned long lastDcsPacketMs = 0;
unsigned long lastSerialDebugMs = 0;

bool lastRenderedStandby = false;
bool lastRenderedStale = false;
uint8_t lastRenderedTransitionPercent = 255;

// -------------------------------------------------------------------
// DECODING
// -------------------------------------------------------------------

SpeedbrakeState decodeSpeedbrakeState(uint16_t raw) {
  if (raw <= SB_CLOSED_MAX_RAW) {
    return SB_CLOSED;
  }

  if (raw >= SB_OPEN_MIN_RAW) {
    return SB_OPEN;
  }

  return SB_TRANSIT;
}

uint8_t speedbrakeTransitionPercent(uint16_t raw) {
  if (raw <= SB_CLOSED_MAX_RAW) {
    return 0;
  }

  if (raw >= SB_OPEN_MIN_RAW) {
    return 100;
  }

  unsigned long span = SB_OPEN_MIN_RAW - SB_CLOSED_MAX_RAW;
  unsigned long pos = raw - SB_CLOSED_MAX_RAW;

  return (uint8_t)((pos * 100UL) / span);
}

const char* speedbrakeStateName(SpeedbrakeState state) {
  switch (state) {
    case SB_CLOSED:
      return "CLOSED";
    case SB_TRANSIT:
      return "TRANSIT";
    case SB_OPEN:
      return "OPEN";
    default:
      return "UNKNOWN";
  }
}

const char* systemStateName(SystemState state) {
  switch (state) {
    case SYS_BOOT:
      return "BOOT";
    case SYS_STANDBY:
      return "STANDBY";
    case SYS_LIVE:
      return "LIVE";
    case SYS_STALE:
      return "STALE";
    case SYS_TEST:
      return "TEST";
    default:
      return "UNKNOWN";
  }
}

// -------------------------------------------------------------------
// DCS-BIOS
// -------------------------------------------------------------------

void updateDcsChannel(DcsChannel& ch, uint16_t newValue) {
  ch.raw = newValue;
  ch.lastUpdateMs = millis();
  ch.updateCount++;
  ch.live = true;

  lastDcsPacketMs = millis();
  dcsEverSeen = true;
}

void onSpeedbrakeIndicatorChange(unsigned int newValue) {
  updateDcsChannel(chSpeedbrake, (uint16_t)newValue);
  currentSbState = decodeSpeedbrakeState(chSpeedbrake.raw);
}

#if USE_DCS_BIOS
DcsBios::IntegerBuffer speedbrakeIndicatorBuffer(
  F_16C_50_SPEEDBRAKE_INDICATOR,
  onSpeedbrakeIndicatorChange
);
#endif

// -------------------------------------------------------------------
// DISPLAY HELPERS
// -------------------------------------------------------------------

void clearDisplayBlack() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

void drawCenteredText(const char* text, uint8_t textSize, int y) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(text);
}

bool pointInIndicatorWindow(int x, int y) {
  return (x >= IND_X &&
          x < IND_X + IND_W &&
          y >= IND_Y &&
          y < IND_Y + IND_H);
}

void drawRawFooter() {
#if SHOW_RAW_FOOTER
  display.fillRect(0, 54, 128, 10, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("RAW:");
  display.print(chSpeedbrake.raw);
#endif
}

void drawStaleMarker() {
#if SHOW_STALE_MARKER
  display.setTextSize(1);
  display.setCursor(104, 56);
  display.print("STL");
#endif
}

// -------------------------------------------------------------------
// FLAG DRAWING
// -------------------------------------------------------------------

void drawStripePatternInWindow(int yOffset = 0) {
  for (int x = IND_X - IND_H - 20;
       x < IND_X + IND_W + IND_H + 20;
       x += 12) {

    for (int t = 0; t < 4; t++) {
      int x0 = x + t;
      int y0 = IND_Y + IND_H + yOffset;

      for (int i = 0; i <= IND_H + 12; i++) {
        int px = x0 + i;
        int py = y0 - i;

        if (pointInIndicatorWindow(px, py)) {
          display.drawPixel(px, py, SSD1306_WHITE);
        }
      }
    }
  }
}

void drawDotPatternInWindow() {
  const int startX = IND_X + 8;
  const int startY = IND_Y + 6;
  const int gapX = 22;
  const int gapY = 16;
  const int radius = 4;

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int x = startX + (col * gapX);
      int y = startY + (row * gapY);
      display.fillCircle(x, y, radius, SSD1306_WHITE);
    }
  }
}

void drawStripePatternClippedAboveY(int visibleToY, int yOffset = 0) {
  for (int x = IND_X - IND_H - 20;
       x < IND_X + IND_W + IND_H + 20;
       x += 12) {

    for (int t = 0; t < 4; t++) {
      int x0 = x + t;
      int y0 = IND_Y + IND_H + yOffset;

      for (int i = 0; i <= IND_H + 12; i++) {
        int px = x0 + i;
        int py = y0 - i;

        if (pointInIndicatorWindow(px, py) && py <= visibleToY) {
          display.drawPixel(px, py, SSD1306_WHITE);
        }
      }
    }
  }
}

void drawDotPatternClippedBelowY(int visibleFromY) {
  const int startX = IND_X + 8;
  const int startY = IND_Y + 6;
  const int gapX = 22;
  const int gapY = 16;
  const int radius = 4;

  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int cx = startX + (col * gapX);
      int cy = startY + (row * gapY);

      for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
          int dx = x - cx;
          int dy = y - cy;

          if ((dx * dx + dy * dy) <= (radius * radius) &&
              pointInIndicatorWindow(x, y) &&
              y >= visibleFromY) {
            display.drawPixel(x, y, SSD1306_WHITE);
          }
        }
      }
    }
  }
}

// -------------------------------------------------------------------
// RENDERING
// -------------------------------------------------------------------

void renderStartupSplash() {
  clearDisplayBlack();

  display.setTextSize(1);
  display.setCursor(8, 6);
  display.print("ML F-16C SIM");

  display.setCursor(8, 19);
  display.print("SPEEDBRAKE");

  display.setCursor(8, 34);
  display.print("COMMUNITY V1.0");

  display.setCursor(8, 48);
  display.print("NANO SSD1306");

  display.display();
}

void renderStandby() {
  clearDisplayBlack();

#if SHOW_STANDBY_TEXT
  drawCenteredText("STBY", 2, 18);
  drawCenteredText("WAIT DCS", 1, 42);
#endif

  display.display();

  lastRenderedStandby = true;
  lastRenderedStale = false;
}

void renderClosedStripes(bool stale = false) {
  clearDisplayBlack();

  drawStripePatternInWindow();
  drawRawFooter();

  if (stale) {
    drawStaleMarker();
  }

  display.display();

  lastRenderedStandby = false;
  lastRenderedStale = stale;
  lastRenderedTransitionPercent = 0;
}

void renderOpenDots(bool stale = false) {
  clearDisplayBlack();

  drawDotPatternInWindow();
  drawRawFooter();

  if (stale) {
    drawStaleMarker();
  }

  display.display();

  lastRenderedStandby = false;
  lastRenderedStale = stale;
  lastRenderedTransitionPercent = 100;
}

void renderDrumTransition(uint8_t percent, bool stale = false) {
  clearDisplayBlack();

  int splitY = IND_Y + IND_H - ((IND_H * percent) / 100);
  int stripeYOffset = (percent / 8) % 12;

  drawStripePatternClippedAboveY(splitY, stripeYOffset);
  drawDotPatternClippedBelowY(splitY);

  drawRawFooter();

  if (stale) {
    drawStaleMarker();
  }

  display.display();

  lastRenderedStandby = false;
  lastRenderedStale = stale;
  lastRenderedTransitionPercent = percent;
}

void renderSpeedbrakeState(bool force = false, bool stale = false) {
  uint8_t transitionPercent = speedbrakeTransitionPercent(chSpeedbrake.raw);

  if (!force &&
      currentSbState == lastRenderedSbState &&
      lastRenderedStale == stale &&
      lastRenderedTransitionPercent == transitionPercent &&
      !lastRenderedStandby) {
    return;
  }

  lastRenderedSbState = currentSbState;

  switch (currentSbState) {
    case SB_CLOSED:
      renderClosedStripes(stale);
      break;

    case SB_TRANSIT:
      renderDrumTransition(transitionPercent, stale);
      break;

    case SB_OPEN:
      renderOpenDots(stale);
      break;

    default:
      renderClosedStripes(stale);
      break;
  }
}

// -------------------------------------------------------------------
// SYSTEM STATE
// -------------------------------------------------------------------

void updateSystemState() {
#if USE_TEST_VALUES
  systemState = SYS_TEST;
  return;
#endif

  if (!dcsEverSeen) {
    systemState = SYS_STANDBY;
    return;
  }

  if ((millis() - lastDcsPacketMs) > DCS_TIMEOUT_MS) {
    systemState = SYS_STALE;
    return;
  }

  systemState = SYS_LIVE;
}

void renderCurrentSystemState() {
  switch (systemState) {
    case SYS_BOOT:
      renderStartupSplash();
      break;

    case SYS_STANDBY:
      if (!lastRenderedStandby) {
        renderStandby();
      }
      break;

    case SYS_LIVE:
      renderSpeedbrakeState(false, false);
      break;

    case SYS_STALE:
#if SHOW_STALE_MARKER
      renderSpeedbrakeState(false, true);
#else
      // Community behaviour: freeze the last valid cockpit indication.
#endif
      break;

    case SYS_TEST:
      // Test mode is handled separately by runTestMode().
      break;

    default:
      renderStandby();
      break;
  }
}

// -------------------------------------------------------------------
// BENCH TEST MODE
// -------------------------------------------------------------------

#if USE_TEST_VALUES
void runTestMode() {
  static unsigned long lastTestFrameMs = 0;
  static uint16_t testRaw = 0;
  static int testDirection = 1;
  static bool holdingEndpoint = true;
  static unsigned long endpointHoldStartMs = 0;

  if ((millis() - lastTestFrameMs) < TEST_FRAME_MS) {
    return;
  }

  lastTestFrameMs = millis();

  if (holdingEndpoint) {
    if ((millis() - endpointHoldStartMs) < TEST_ENDPOINT_HOLD_MS) {
      chSpeedbrake.raw = testRaw;
      chSpeedbrake.lastUpdateMs = millis();
      chSpeedbrake.updateCount++;
      chSpeedbrake.live = true;

      dcsEverSeen = true;
      lastDcsPacketMs = millis();

      currentSbState = decodeSpeedbrakeState(chSpeedbrake.raw);
      renderSpeedbrakeState(true, false);
      return;
    }

    holdingEndpoint = false;
  }

  if (testDirection > 0) {
    if (testRaw >= (65535 - TEST_OPEN_STEP_RAW)) {
      testRaw = 65535;
      testDirection = -1;
      holdingEndpoint = true;
      endpointHoldStartMs = millis();
    } else {
      testRaw += TEST_OPEN_STEP_RAW;
    }
  } else {
    if (testRaw <= TEST_CLOSE_STEP_RAW) {
      testRaw = 0;
      testDirection = 1;
      holdingEndpoint = true;
      endpointHoldStartMs = millis();
    } else {
      testRaw -= TEST_CLOSE_STEP_RAW;
    }
  }

  chSpeedbrake.raw = testRaw;
  chSpeedbrake.lastUpdateMs = millis();
  chSpeedbrake.updateCount++;
  chSpeedbrake.live = true;

  dcsEverSeen = true;
  lastDcsPacketMs = millis();

  currentSbState = decodeSpeedbrakeState(chSpeedbrake.raw);
  renderSpeedbrakeState(true, false);
}
#endif

// -------------------------------------------------------------------
// OPTIONAL SERIAL DEBUG
// -------------------------------------------------------------------

void runSerialDebug() {
#if ENABLE_SERIAL_DEBUG
#if !USE_DCS_BIOS
  if ((millis() - lastSerialDebugMs) < SERIAL_DEBUG_MS) {
    return;
  }

  lastSerialDebugMs = millis();

  Serial.print("BUILD=");
  Serial.print(BUILD_NAME);

  Serial.print(" VERSION=");
  Serial.print(BUILD_VERSION);

  Serial.print(" SYS=");
  Serial.print(systemStateName(systemState));

  Serial.print(" SB=");
  Serial.print(speedbrakeStateName(currentSbState));

  Serial.print(" RAW=");
  Serial.print(chSpeedbrake.raw);

  Serial.print(" TRANSITION=");
  Serial.print(speedbrakeTransitionPercent(chSpeedbrake.raw));
  Serial.print("%");

  Serial.print(" UPDATES=");
  Serial.print(chSpeedbrake.updateCount);

  Serial.println();
#endif
#endif
}

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------

void setup() {
#if ENABLE_SERIAL_DEBUG
#if !USE_DCS_BIOS
  Serial.begin(115200);
#endif
#endif

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();

#if SHOW_STARTUP_SPLASH
  systemState = SYS_BOOT;
  renderStartupSplash();
  delay(STARTUP_SPLASH_MS);
#endif

#if USE_DCS_BIOS
  DcsBios::setup();
#endif

#if USE_TEST_VALUES
  systemState = SYS_TEST;
  chSpeedbrake.raw = 0;
  currentSbState = SB_CLOSED;
  renderSpeedbrakeState(true, false);
#else
  systemState = SYS_STANDBY;
  renderStandby();
#endif
}

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------

void loop() {
#if USE_DCS_BIOS
  DcsBios::loop();
#endif

#if USE_TEST_VALUES
  runTestMode();
#else
  updateSystemState();
  renderCurrentSystemState();
#endif

  runSerialDebug();
}
