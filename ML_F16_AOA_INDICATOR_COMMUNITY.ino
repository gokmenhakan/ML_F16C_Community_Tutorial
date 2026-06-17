/*********************************************************************
 * ML_F16_AOA_INDICATOR
 * COMMUNITY_ESP32D_R1
 *
 * F-16C AOA Indicator
 * ESP32 + 2.4" 240x320 SPI TFT using TFT_eSPI
 *
 * Key features:
 * - Real AOA instrument style
 * - Static faceplate
 * - Moving AOA tape behind fixed datum
 * - Moving colour bands on tape
 * - Wider colour strip for improved green-band visibility
 * - OFF flag for no-data / standby
 * - DCS-BIOS live feed
 * - DCS stale handling
 * - Rear diagnostic button only
 * - Window-sized sprite to reduce live-mode flicker
 * - Diagnostics screen draws static labels once and refreshes values only
 * - Live calibrated against the actual DCS cockpit AOA indicator
 * - On-screen AOA / DEG / NEG labels removed for physical etched cover
 * - Standard ML_F16 10-15 second boot splash with progress bar
 * - Standard build identity, diagnostics and NVG proxy
 * - Cleaner maintenance diagnostics page
 * - Community edition defaults; Pro hooks disabled
 *
 * DCS-BIOS:
 * F-16C_50/AOA_VALUE
 * Output type: integer
 * Address: 0x4492
 * Max value: 65535
 *
 * Community Release Notes:
 * - This build is intentionally ESP32-D / TFT_eSPI based.
 * - PanelReporter and PFLD hooks are disabled by default.
 * - DCS-BIOS FP Fork / compatible F-16C_50 definitions required.
 *********************************************************************/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// -------------------------------------------------------------------
// EDITION DEFAULTS - defined before optional Pro includes
// -------------------------------------------------------------------

#ifndef ML_F16_EDITION_COMMUNITY
#define ML_F16_EDITION_COMMUNITY  1
#endif

#ifndef ML_F16_EDITION_PRO
#define ML_F16_EDITION_PRO        0
#endif

#ifndef ENABLE_PANEL_REPORTER
#define ENABLE_PANEL_REPORTER     0
#endif

#ifndef ENABLE_PFLD_REPORTING
#define ENABLE_PFLD_REPORTING     0
#endif

#define DCSBIOS_DEFAULT_SERIAL
#include "DcsBios.h"

#if ML_F16_EDITION_PRO && ENABLE_PANEL_REPORTER
#include "ML_F16_PanelReporter.h"
#endif

#if ML_F16_EDITION_PRO && ENABLE_PFLD_REPORTING
#include "ML_F16_PFLD.h"
#endif

// -------------------------------------------------------------------
// BUILD INFO
// -------------------------------------------------------------------

#define BUILD_NAME        "ML_F16_AOA_INDICATOR"
#define BUILD_PHASE       "COMMUNITY_ESP32D_R1"
#define BUILD_DATE        "2026-06-17"
#define PANEL_ID          "F16_AOA"
#define HARDWARE_TARGET   "ESP32-D / 2.4 TFT_eSPI"
#define BUILD_PHASE_SHORT "COMM ESP32D R1"
#define HARDWARE_TARGET_SHORT "ESP32 / 2.4 TFT"

// -------------------------------------------------------------------
// FRAMEWORK SWITCHES
// -------------------------------------------------------------------

#define USE_DCS_BIOS              1
#define USE_TEST_VALUES           0

// Edition defaults are defined before optional Pro includes

#define ENABLE_DIAGNOSTICS        1
#define ENABLE_REAR_DIAG_BTN      1
#define ENABLE_TOUCH_DIAGS        0

#define SHOW_STARTUP_SPLASH       1
#define SHOW_RAW_DEBUG            0

#define ENABLE_NVG_MODE           1
#define ENABLE_DCS_NVG_PROXY      1
#define START_IN_NVG_MODE         0
#define NVG_PROXY_HUD_NIGHT       1
#define NVG_PROXY_HUD_NIGHT_POS   2

// Pro-only future hooks are defined before optional Pro includes.

// -------------------------------------------------------------------
// HARDWARE CONFIG
// -------------------------------------------------------------------

#define SCREEN_W                  240
#define SCREEN_H                  320

// Rear diagnostic button.
// Wire button between GPIO27 and GND.
// Uses INPUT_PULLUP.
#define PIN_REAR_DIAG_BTN         27

// -------------------------------------------------------------------
// TIMING CONFIG
// -------------------------------------------------------------------

#define STANDARD_BOOT_TIME_MS     12000
#define STARTUP_SPLASH_MS         STANDARD_BOOT_TIME_MS
#define SELF_TEST_MS              0
#define DCS_TIMEOUT_MS            3000
#define SYNC_SWEEP_MS             1600
#define FRAME_MS                  33
#define DIAG_DEBOUNCE_MS          300
#define DIAG_REFRESH_MS           500

// -------------------------------------------------------------------
// DISPLAY GEOMETRY
// -------------------------------------------------------------------

#define FACE_X                    62
#define FACE_Y                    12
#define FACE_W                    116
#define FACE_H                    296

#define WINDOW_X                  88
#define WINDOW_Y                  50
#define WINDOW_W                  64
#define WINDOW_H                  220

#define WINDOW_RADIUS             8

#define DATUM_Y                   160

// Visual tape spacing.
// 14 px per degree gives useful movement inside the 220 px aperture.
#define UNIT_PER_DEG              14

#define AOA_MIN_DEG               -5.0f
#define AOA_MAX_DEG                18.0f

// -------------------------------------------------------------------
// AOA COLOUR BAND CONFIG
// -------------------------------------------------------------------

#define ENABLE_AOA_COLOUR_BANDS   1

// Visual tape bands only. These do not affect DCS calibration.
// Captured against the DCS cockpit AOA tape:
//
// Start green / end yellow raw 43589 = approx +11.52 deg
// Mid green band raw 44320          = approx +12.37 deg
// Start red band raw 45622          = approx +13.88 deg
// Top red band raw 48199            = visual top red reference

#define AOA_YELLOW_LOW_DEG         8.0f
#define AOA_YELLOW_HIGH_DEG       11.52f

#define AOA_GREEN_LOW_DEG         11.52f
#define AOA_GREEN_HIGH_DEG        13.88f

#define AOA_RED_LOW_DEG           13.88f
#define AOA_RED_HIGH_DEG          18.0f

#define AOA_RAW_START_GREEN_BAND   43589
#define AOA_DEG_START_GREEN_BAND   11.52f

#define AOA_RAW_MID_GREEN_BAND     44320
#define AOA_DEG_MID_GREEN_BAND     12.37f

#define AOA_RAW_START_RED_BAND     45622
#define AOA_DEG_START_RED_BAND     13.88f

#define AOA_RAW_TOP_RED_BAND       48199

// -------------------------------------------------------------------
// COLOURS
// -------------------------------------------------------------------

#define COL_BLACK                 TFT_BLACK
#define COL_WHITE                 TFT_WHITE
#define COL_GREEN                 TFT_GREEN
#define COL_BLUE                  TFT_BLUE
#define COL_RED                   TFT_RED
#define COL_YELLOW                TFT_YELLOW

#define COL_FACE                  0x18E3
#define COL_FACE_DARK             0x0841
#define COL_FACE_EDGE             0x39E7
#define COL_WINDOW_DARK           0x0000
#define COL_GLASS                 0x2104
#define COL_GREY                  0x8410
#define COL_DIM                   0x4208
#define COL_AMBER                 0xFD20
#define COL_OFF_ORANGE            0xFBE0
#define COL_OFF_RED               0xD2A0
#define COL_NVG_GREEN             0x07E0

#define COL_AOA_YELLOW            0xFFE0
#define COL_AOA_GREEN             0x07E0
#define COL_AOA_RED               0xF800

#define COL_AOA_YELLOW_DIM        0x7BE0
#define COL_AOA_GREEN_DIM         0x03E0
#define COL_AOA_RED_DIM           0x7800

// -------------------------------------------------------------------
// SYSTEM STATES
// -------------------------------------------------------------------

enum SystemState {
  SYS_OFF,
  SYS_BOOT,
  SYS_STANDBY,
  SYS_SYNC,
  SYS_LIVE,
  SYS_STALE,
  SYS_DIAG
};

SystemState currentState = SYS_BOOT;
SystemState lastRenderedState = SYS_OFF;

// -------------------------------------------------------------------
// DCS CHANNEL STRUCTURE
// -------------------------------------------------------------------

struct DcsChannel {
  const char* name;
  uint16_t raw;
  float decoded;
  unsigned long lastUpdateMs;
  uint32_t updateCount;
  bool live;
};

DcsChannel chAoa = {
  "AOA_VALUE",
  0,
  0.0f,
  0,
  0,
  false
};

DcsChannel chHudBrtSw = {
  "HUD_BRT_SW",
  0,
  0.0f,
  0,
  0,
  false
};

// -------------------------------------------------------------------
// CALIBRATION TABLE
// -------------------------------------------------------------------

struct CalPoint {
  float value;
  uint16_t raw;
};

/*
   Live cockpit-AOA-referenced calibration table.

   These values were captured against the actual DCS cockpit AOA indicator,
   not aircraft pitch angle, climb angle, or flight path angle.

   Captured points:
   -5   = 29216
    0   = 33112
   +2.5 = 35343
   +5   = 37599
   +10  = 42288
   +15  = 46581
   +18  = 49406

   Visual references:
   Start green / end yellow = 43589
   Mid green band           = 44320
   Start red band           = 45622
   Top red band             = 48199

   Notes:
   - F_16C_50_AOA_VALUE is a gauge-position output.
   - Lookup table interpolation is required.
   - The useful captured display range is currently -5 to +18.
*/
CalPoint aoaCalTable[] = {
  { -5.0f, 29216 },
  {  0.0f, 33112 },
  {  2.5f, 35343 },
  {  5.0f, 37599 },
  { 10.0f, 42288 },
  { 15.0f, 46581 },
  { 18.0f, 49406 }
};

const int AOA_CAL_COUNT = sizeof(aoaCalTable) / sizeof(aoaCalTable[0]);

// -------------------------------------------------------------------
// DISPLAY OBJECTS
// -------------------------------------------------------------------

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite tape = TFT_eSprite(&tft);

// -------------------------------------------------------------------
// LIVE VALUES
// -------------------------------------------------------------------

float aoaTargetDeg = 0.0f;
float aoaDisplayedDeg = 0.0f;

float testAoaDeg = -5.0f;
bool testDirectionUp = true;

bool dcsEverSeen = false;
bool diagnosticsMode = false;
bool nvgMode = false;
bool dcsNvgProxyValid = false;
bool splashComplete = false;
bool bootStaticDrawn = false;
bool tapeReady = false;
bool staticFaceDrawn = false;
bool diagnosticsDrawn = false;
bool offFlagDrawn = false;

unsigned long bootMs = 0;
unsigned long lastFrameMs = 0;
unsigned long lastDcsPacketMs = 0;
unsigned long syncStartMs = 0;
unsigned long lastDiagButtonMs = 0;
unsigned long lastDiagRefreshMs = 0;

uint32_t frameCounter = 0;
uint32_t staleCount = 0;

// -------------------------------------------------------------------
// BASIC HELPERS
// -------------------------------------------------------------------

float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float lerpFloat(float a, float b, float t) {
  return a + ((b - a) * t);
}

float interpolateTable(uint16_t raw, const CalPoint* table, int count) {
  if (count <= 0) return 0.0f;

  if (raw <= table[0].raw) {
    return table[0].value;
  }

  if (raw >= table[count - 1].raw) {
    return table[count - 1].value;
  }

  for (int i = 0; i < count - 1; i++) {
    if (raw >= table[i].raw && raw <= table[i + 1].raw) {
      float span = (float)(table[i + 1].raw - table[i].raw);
      if (span <= 0.0f) return table[i].value;

      float t = (float)(raw - table[i].raw) / span;
      return lerpFloat(table[i].value, table[i + 1].value, t);
    }
  }

  return table[0].value;
}

const char* stateName(SystemState s) {
  switch (s) {
    case SYS_OFF:     return "OFF";
    case SYS_BOOT:    return "BOOT";
    case SYS_STANDBY: return "STANDBY";
    case SYS_SYNC:    return "SYNC";
    case SYS_LIVE:    return "LIVE";
    case SYS_STALE:   return "STALE";
    case SYS_DIAG:    return "DIAG";
    default:          return "UNKNOWN";
  }
}

uint16_t cockpitWhite() {
  // STALE must override NVG so a failed DCS feed is visually obvious.
  if (currentState == SYS_STALE) return COL_RED;
#if ENABLE_NVG_MODE
  if (nvgMode) return COL_NVG_GREEN;
#endif
  return COL_WHITE;
}

uint16_t cockpitDim() {
  // STALE must override NVG so a failed DCS feed is visually obvious.
  if (currentState == SYS_STALE) return COL_AOA_RED_DIM;
#if ENABLE_NVG_MODE
  if (nvgMode) return 0x03E0;
#endif
  return COL_GREY;
}

uint16_t bandYellowColour(bool stale) {
  if (stale) return COL_AOA_YELLOW_DIM;
#if ENABLE_NVG_MODE
  if (nvgMode) return COL_NVG_GREEN;
#endif
  return COL_AOA_YELLOW;
}

uint16_t bandGreenColour(bool stale) {
  if (stale) return COL_AOA_GREEN_DIM;
#if ENABLE_NVG_MODE
  if (nvgMode) return COL_NVG_GREEN;
#endif
  return COL_AOA_GREEN;
}

uint16_t bandRedColour(bool stale) {
  if (stale) return COL_AOA_RED_DIM;
#if ENABLE_NVG_MODE
  if (nvgMode) return COL_NVG_GREEN;
#endif
  return COL_AOA_RED;
}

void invalidateStaticFace() {
  staticFaceDrawn = false;
  diagnosticsDrawn = false;
  offFlagDrawn = false;
}

// -------------------------------------------------------------------
// DCS CHANNEL UPDATE
// -------------------------------------------------------------------

void updateDcsChannel(DcsChannel& ch, uint16_t newValue) {
  ch.raw = newValue;
  ch.decoded = interpolateTable(newValue, aoaCalTable, AOA_CAL_COUNT);
  ch.decoded = clampFloat(ch.decoded, AOA_MIN_DEG, AOA_MAX_DEG);
  ch.lastUpdateMs = millis();
  ch.updateCount++;
  ch.live = true;

  lastDcsPacketMs = millis();

  if (!dcsEverSeen) {
    dcsEverSeen = true;
    syncStartMs = millis();
    invalidateStaticFace();
  }
}

// -------------------------------------------------------------------
// DCS-BIOS CALLBACKS
// -------------------------------------------------------------------

void onAoaValueChange(unsigned int newValue) {
  updateDcsChannel(chAoa, newValue);
  aoaTargetDeg = chAoa.decoded;
}

/*
   Correct IntegerBuffer form for this TFT display build.

   ServoOutput uses F_16C_50_AOA_VALUE_A.
   This TFT display uses F_16C_50_AOA_VALUE.
*/
#if USE_DCS_BIOS
DcsBios::IntegerBuffer aoaValueBuffer(
  F_16C_50_AOA_VALUE,
  onAoaValueChange
);
#endif


// -------------------------------------------------------------------
// DCS-BIOS NVG / DIM PROXY
// -------------------------------------------------------------------

void onHudBrightnessSwitchChange(unsigned int newValue) {
#if ENABLE_NVG_MODE && ENABLE_DCS_NVG_PROXY
  chHudBrtSw.raw = (uint16_t)newValue;
  chHudBrtSw.decoded = (float)newValue;
  chHudBrtSw.lastUpdateMs = millis();
  chHudBrtSw.updateCount++;
  chHudBrtSw.live = true;
  dcsNvgProxyValid = true;

#if NVG_PROXY_HUD_NIGHT
  bool newNvg = (newValue == NVG_PROXY_HUD_NIGHT_POS);
#else
  bool newNvg = false;
#endif

  if (nvgMode != newNvg) {
    nvgMode = newNvg;
    invalidateStaticFace();
  }
#else
  (void)newValue;
#endif
}

#if USE_DCS_BIOS && ENABLE_NVG_MODE && ENABLE_DCS_NVG_PROXY
DcsBios::IntegerBuffer hudBrightnessSwitchBuffer(
  F_16C_50_HUD_BRT_SW,
  onHudBrightnessSwitchChange
);
#endif

// -------------------------------------------------------------------
// PANEL REPORTER / PFLD HOOKS
// -------------------------------------------------------------------

void reportPanelStatus(const char* severity, const char* message) {
#if ML_F16_EDITION_PRO && ENABLE_PANEL_REPORTER
  // Header is compile-gated. Keep implementation here until the final
  // PanelReporter API is locked across all modules.
  (void)severity;
  (void)message;
#else
  (void)severity;
  (void)message;
#endif
}

void reportPfldStatus(const char* message) {
#if ML_F16_EDITION_PRO && ENABLE_PFLD_REPORTING
  // Header is compile-gated. Keep implementation here until the final
  // PFLD API is locked across all modules.
  (void)message;
#else
  (void)message;
#endif
}

// -------------------------------------------------------------------
// DISPLAY SETUP
// -------------------------------------------------------------------

void setupDisplay() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BLACK);

  tape.setColorDepth(16);
  tape.createSprite(WINDOW_W, WINDOW_H);
  tape.setTextDatum(MC_DATUM);
  tape.setTextFont(4);

  tapeReady = true;
}

// -------------------------------------------------------------------
// STANDARD ML_F16 STARTUP SPLASH
// -------------------------------------------------------------------

void drawStandardBootStatic() {
  tft.fillScreen(COL_BLACK);
  tft.setTextDatum(MC_DATUM);

  // 240 px wide TFT: use controlled short display strings derived from build identity.
  tft.setTextFont(2);
  tft.setTextColor(COL_AMBER, COL_BLACK);
  tft.drawString("ML_F16 AOA", SCREEN_W / 2, 54);

  tft.setTextFont(1);
  tft.setTextColor(COL_GREY, COL_BLACK);
  tft.drawString(BUILD_PHASE_SHORT, SCREEN_W / 2, 82);
  tft.drawString(HARDWARE_TARGET_SHORT, SCREEN_W / 2, 100);
  tft.drawString(BUILD_DATE, SCREEN_W / 2, 118);

  tft.setTextFont(2);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.drawString("INITIALISING", SCREEN_W / 2, 160);
  tft.drawString("AVIONICS", SCREEN_W / 2, 182);

  // Outer progress box. Keep this geometry matched to updateStandardBootProgress().
  const int barX = 30;
  const int barY = 214;
  const int barW = SCREEN_W - 60;
  const int barH = 14;

  tft.drawRoundRect(barX, barY, barW, barH, 4, COL_GREY);

  tft.setTextFont(1);
  tft.setTextColor(COL_DIM, COL_BLACK);
  tft.drawString(PANEL_ID, SCREEN_W / 2, 252);
  tft.drawString("ML_F16 MODULE", SCREEN_W / 2, 292);

  bootStaticDrawn = true;
}

void updateStandardBootProgress(float progress) {
  progress = clampFloat(progress, 0.0f, 1.0f);

  if (!bootStaticDrawn) {
    drawStandardBootStatic();
  }

  // Outer box from drawStandardBootStatic():
  // tft.drawRoundRect(30, 214, SCREEN_W - 60, 14, 4, COL_GREY);

  const int outerX = 30;
  const int outerY = 214;
  const int outerW = SCREEN_W - 60;
  const int outerH = 14;

  // Inset fill area so the ribbon sits cleanly inside the rounded border.
  const int inset = 3;
  const int barX = outerX + inset;
  const int barY = outerY + inset;
  const int barW = outerW - (inset * 2);
  const int barH = outerH - (inset * 2);

  const int fillW = (int)(barW * progress);

  // Dirty-region update only. Do not clear/redraw the whole screen.
  tft.fillRect(barX, barY, barW, barH, COL_BLACK);

  if (fillW > 0) {
    tft.fillRoundRect(barX, barY, fillW, barH, 2, COL_AMBER);
  }
}

void runStartupSequence() {
#if SHOW_STARTUP_SPLASH
  unsigned long startMs = millis();
  bootStaticDrawn = false;

  while ((millis() - startMs) < STANDARD_BOOT_TIME_MS) {
#if USE_DCS_BIOS
    DcsBios::loop();
#endif
    float p = (float)(millis() - startMs) / (float)STANDARD_BOOT_TIME_MS;
    updateStandardBootProgress(p);
    delay(80);
  }

  updateStandardBootProgress(1.0f);
  delay(200);
#else
  delay(250);
#endif

  splashComplete = true;
  bootStaticDrawn = false;
}

// -------------------------------------------------------------------
// REAR DIAGNOSTIC BUTTON
// -------------------------------------------------------------------

void updateRearDiagnosticButton() {
#if ENABLE_DIAGNOSTICS && ENABLE_REAR_DIAG_BTN
  bool pressed = digitalRead(PIN_REAR_DIAG_BTN) == LOW;

  if (pressed && (millis() - lastDiagButtonMs > DIAG_DEBOUNCE_MS)) {
    diagnosticsMode = !diagnosticsMode;
    lastDiagButtonMs = millis();

    tft.fillScreen(COL_BLACK);
    invalidateStaticFace();
  }
#endif
}

// -------------------------------------------------------------------
// TEST VALUE GENERATOR
// -------------------------------------------------------------------

void updateTestValues() {
#if USE_TEST_VALUES
  static unsigned long lastTestMs = 0;

  if (millis() - lastTestMs > 80) {
    lastTestMs = millis();

    testAoaDeg += testDirectionUp ? 0.18f : -0.18f;

    if (testAoaDeg >= AOA_MAX_DEG) {
      testAoaDeg = AOA_MAX_DEG;
      testDirectionUp = false;
    }

    if (testAoaDeg <= AOA_MIN_DEG) {
      testAoaDeg = AOA_MIN_DEG;
      testDirectionUp = true;
    }

    aoaTargetDeg = testAoaDeg;

    if (!dcsEverSeen) {
      dcsEverSeen = true;
      lastDcsPacketMs = millis();
      syncStartMs = millis();
      invalidateStaticFace();
    }
  }
#endif
}

// -------------------------------------------------------------------
// SYSTEM STATE MACHINE
// -------------------------------------------------------------------

void updateSystemState() {
  SystemState oldState = currentState;

  if (!splashComplete) {
    currentState = SYS_BOOT;
  } else if (diagnosticsMode) {
    currentState = SYS_DIAG;
  } else {
#if USE_TEST_VALUES
    if (dcsEverSeen && ((millis() - syncStartMs) < SYNC_SWEEP_MS)) {
      currentState = SYS_SYNC;
    } else {
      currentState = SYS_LIVE;
    }
#else
    if (!dcsEverSeen) {
      currentState = SYS_STANDBY;
    } else if ((millis() - lastDcsPacketMs) > DCS_TIMEOUT_MS) {
      currentState = SYS_STALE;
    } else if ((millis() - syncStartMs) < SYNC_SWEEP_MS) {
      currentState = SYS_SYNC;
    } else {
      currentState = SYS_LIVE;
    }
#endif
  }

  if (oldState != currentState) {
    invalidateStaticFace();
  }
}

// -------------------------------------------------------------------
// FACEPLATE AND STATIC OVERLAY
// -------------------------------------------------------------------

void drawScrew(int x, int y, int r) {
  uint16_t edge = COL_FACE_EDGE;
  uint16_t fill = COL_FACE_DARK;

  tft.fillCircle(x, y, r, fill);
  tft.drawCircle(x, y, r, edge);
  tft.drawLine(x - r + 2, y, x + r - 2, y, COL_DIM);
}

void drawFaceplateBase() {
  tft.fillRoundRect(FACE_X, FACE_Y, FACE_W, FACE_H, 7, COL_FACE);
  tft.drawRoundRect(FACE_X, FACE_Y, FACE_W, FACE_H, 7, COL_FACE_EDGE);

  tft.fillRoundRect(FACE_X + 10, FACE_Y + 26, FACE_W - 20, FACE_H - 52, 8, COL_FACE_DARK);
  tft.drawRoundRect(FACE_X + 10, FACE_Y + 26, FACE_W - 20, FACE_H - 52, 8, COL_DIM);

  drawScrew(FACE_X + 20, FACE_Y + 15, 5);
  drawScrew(FACE_X + FACE_W - 20, FACE_Y + 15, 5);
  drawScrew(FACE_X + 20, FACE_Y + FACE_H - 15, 5);
  drawScrew(FACE_X + FACE_W - 20, FACE_Y + FACE_H - 15, 5);

  drawScrew(120, FACE_Y + 15, 6);
  drawScrew(120, FACE_Y + FACE_H - 15, 6);
}

void drawStaticLabels() {
  // Labels intentionally omitted.
  // AOA / DEG / NEG will be etched onto the physical gauge cover.
}

void drawDatumOverlay() {
  uint16_t white = cockpitWhite();
  uint16_t dim = cockpitDim();

  // No software-drawn tape/window bezel.
  // The final AOA aperture/bezel is a physical 3D-printed or etched cover,
  // so the software only renders the aircraft datum marks and tape behaviour.

  tft.fillTriangle(WINDOW_X - 10, DATUM_Y,
                   WINDOW_X - 24, DATUM_Y - 9,
                   WINDOW_X - 24, DATUM_Y + 9,
                   white);

  tft.fillTriangle(WINDOW_X + WINDOW_W + 10, DATUM_Y,
                   WINDOW_X + WINDOW_W + 24, DATUM_Y - 9,
                   WINDOW_X + WINDOW_W + 24, DATUM_Y + 9,
                   white);

  tft.drawLine(WINDOW_X - 14, DATUM_Y, WINDOW_X + WINDOW_W + 14, DATUM_Y, white);
  tft.drawLine(WINDOW_X + 5, DATUM_Y - 1, WINDOW_X + WINDOW_W - 5, DATUM_Y - 1, dim);
}

void drawStaticInstrumentFace() {
  tft.fillScreen(COL_BLACK);

  // No software-drawn faceplate, window surround, screws or bitmap-style bezel.
  // The real AOA face/aperture will be provided by the physical printed/etched cover.
  // The screen renders only the active instrument elements behind that cover.
  drawStaticLabels();
  drawDatumOverlay();

  staticFaceDrawn = true;
}

// -------------------------------------------------------------------
// MOVING AOA TAPE, WINDOW-SPRITE ONLY
// -------------------------------------------------------------------

int aoaToTapeY(float tapeCentreAoa, float aoaValue) {
  float delta = aoaValue - tapeCentreAoa;
  return (WINDOW_H / 2) - (int)(delta * UNIT_PER_DEG);
}

void drawAoaBand(float tapeCentreAoa,
                 float bandLowAoa,
                 float bandHighAoa,
                 uint16_t colour) {
#if ENABLE_AOA_COLOUR_BANDS
  int yHigh = aoaToTapeY(tapeCentreAoa, bandHighAoa);
  int yLow  = aoaToTapeY(tapeCentreAoa, bandLowAoa);

  int y = min(yHigh, yLow);
  int h = abs(yLow - yHigh);

  if (h <= 0) return;

  if (y < 0) {
    h += y;
    y = 0;
  }

  if (y >= WINDOW_H) return;

  if ((y + h) > WINDOW_H) {
    h = WINDOW_H - y;
  }

  if (h <= 0) return;

  // Wider colour strip inside the moving tape, behind the white markings.
  int bandX = WINDOW_W - 18;
  int bandW = 14;

  tape.fillRect(bandX, y, bandW, h, colour);
#endif
}

void drawTapeMajorMark(int y, int value, uint16_t colour) {
  tape.setTextFont(4);
  tape.setTextDatum(MC_DATUM);
  tape.setTextColor(colour, COL_BLACK);

  String label;

  if (value < 0) {
    label = "-" + String(abs(value));
  } else {
    label = String(value);
  }

  int cx = WINDOW_W / 2;

  tape.drawString(label, cx, y);

  int gap = 22;
  tape.drawLine(5, y, cx - gap, y, colour);
  tape.drawLine(cx + gap, y, WINDOW_W - 5, y, colour);
}

void drawTapeMinorMark(int y, int value, uint16_t colour) {
  int cx = WINDOW_W / 2;

  if (value < 0) {
    tape.drawLine(cx - 20, y, cx - 8, y, colour);
    tape.drawLine(cx + 8, y, cx + 20, y, colour);
  } else {
    int len = 34;
    tape.drawLine(cx - (len / 2), y, cx + (len / 2), y, colour);
  }
}

void drawMovingTapeWindow(float aoaDeg, bool stale) {
  if (!tapeReady) return;

  uint16_t colour = stale ? cockpitDim() : cockpitWhite();

  tape.fillSprite(COL_BLACK);

#if ENABLE_AOA_COLOUR_BANDS
  drawAoaBand(aoaDeg,
              AOA_YELLOW_LOW_DEG,
              AOA_YELLOW_HIGH_DEG,
              bandYellowColour(stale));

  drawAoaBand(aoaDeg,
              AOA_GREEN_LOW_DEG,
              AOA_GREEN_HIGH_DEG,
              bandGreenColour(stale));

  drawAoaBand(aoaDeg,
              AOA_RED_LOW_DEG,
              AOA_RED_HIGH_DEG,
              bandRedColour(stale));
#endif

  int centerValue = (int)aoaDeg;
  float offset = aoaDeg - centerValue;
  int scrollOffset = (int)(offset * UNIT_PER_DEG);

  int windowCenterY = WINDOW_H / 2;

  for (int i = -10; i <= 25; i++) {
    int value = centerValue + i;

    if (value < (int)AOA_MIN_DEG || value > (int)AOA_MAX_DEG) {
      continue;
    }

    int y = windowCenterY - (i * UNIT_PER_DEG) + scrollOffset;

    if (y < -30 || y > WINDOW_H + 30) {
      continue;
    }

    if (value % 5 == 0) {
      drawTapeMajorMark(y, value, colour);
    } else {
      drawTapeMinorMark(y, value, colour);
    }
  }

  tft.startWrite();
  tape.pushSprite(WINDOW_X, WINDOW_Y);
  tft.endWrite();
}

// -------------------------------------------------------------------
// OFF FLAG
// -------------------------------------------------------------------

void drawOffFlag() {
  int flagX = WINDOW_X + 5;
  int flagY = DATUM_Y - 36;
  int flagW = WINDOW_W - 10;
  int flagH = 72;

  tft.fillRect(WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, COL_BLACK);

  tft.fillRoundRect(flagX, flagY, flagW, flagH, 2, COL_OFF_ORANGE);
  tft.drawRoundRect(flagX, flagY, flagW, flagH, 2, COL_OFF_RED);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(COL_BLACK, COL_OFF_ORANGE);
  tft.drawString("OFF", WINDOW_X + (WINDOW_W / 2), DATUM_Y);
}

// -------------------------------------------------------------------
// RENDERERS
// -------------------------------------------------------------------

void renderInstrumentLive(bool stale) {
  if (!staticFaceDrawn) {
    drawStaticInstrumentFace();
  }

  drawMovingTapeWindow(aoaDisplayedDeg, stale);
  drawDatumOverlay();

#if SHOW_RAW_DEBUG
  tft.fillRect(0, 0, 92, 26, COL_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COL_GREEN, COL_BLACK);
  tft.drawString("RAW " + String(chAoa.raw), 2, 2);
  tft.drawString("AOA " + String(aoaDisplayedDeg, 2), 2, 14);
#endif

  if (stale) {
    staleCount++;
  }
}

void renderStandby() {
  if (!staticFaceDrawn) {
    drawStaticInstrumentFace();
  }

  // Draw the OFF flag once only. Repainting it every frame causes visible flicker.
  if (!offFlagDrawn) {
    drawOffFlag();
    drawDatumOverlay();
    offFlagDrawn = true;
  }
}

void renderSync() {
  if (!staticFaceDrawn) {
    drawStaticInstrumentFace();
  }

  unsigned long elapsed = millis() - syncStartMs;
  float t = clampFloat((float)elapsed / (float)SYNC_SWEEP_MS, 0.0f, 1.0f);

  float syncValue = lerpFloat(0.0f, aoaTargetDeg, t);

  drawMovingTapeWindow(syncValue, false);
  drawDatumOverlay();

  tft.fillRect(100, 296, 42, 14, COL_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COL_GREEN, COL_BLACK);
  tft.drawString("SYNC", 120, 304);
}

void renderDiagnostics() {
#if ENABLE_DIAGNOSTICS
  if (!diagnosticsDrawn) {
    tft.fillScreen(COL_BLACK);
    tft.setTextDatum(TL_DATUM);

    tft.setTextFont(2);
    tft.setTextColor(COL_AMBER, COL_BLACK);
    tft.drawString("AOA MAINT", 8, 8);

    tft.setTextFont(1);
    tft.setTextColor(COL_GREY, COL_BLACK);
    tft.drawString("BUILD", 8, 34);
    tft.drawString("PHASE", 8, 48);
    tft.drawString("HW", 8, 62);

    tft.drawString("STATE", 8, 88);
    tft.drawString("DCS", 8, 102);
    tft.drawString("AGE", 8, 116);
    tft.drawString("UPDATES", 8, 130);

    tft.drawString("AOA RAW", 8, 154);
    tft.drawString("AOA DEG", 8, 168);
    tft.drawString("TARGET", 8, 182);
    tft.drawString("DISPLAY", 8, 196);

    tft.drawString("NVG", 8, 220);
    tft.drawString("HUD BRT", 8, 234);
    tft.drawString("HUD SEEN", 8, 248);
    tft.drawString("STALE CT", 8, 262);
    tft.drawString("FRAMES", 8, 276);
    tft.drawString("EDITION", 8, 290);

    tft.setTextColor(COL_GREEN, COL_BLACK);
    tft.drawString("Rear button exits", 8, 306);

    diagnosticsDrawn = true;
    lastDiagRefreshMs = 0;
  }

  if ((millis() - lastDiagRefreshMs) < DIAG_REFRESH_MS) {
    return;
  }

  lastDiagRefreshMs = millis();

  const int valueX = 82;
  const int valueW = 152;

  auto clearValue = [&](int y) {
    tft.fillRect(valueX, y, valueW, 11, COL_BLACK);
  };

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COL_WHITE, COL_BLACK);

  clearValue(34);
  tft.drawString("AOA", valueX, 34);

  clearValue(48);
  tft.drawString(BUILD_PHASE_SHORT, valueX, 48);

  clearValue(62);
  tft.drawString(HARDWARE_TARGET_SHORT, valueX, 62);

  clearValue(88);
  tft.drawString(stateName(currentState), valueX, 88);

  clearValue(102);
  tft.drawString(dcsEverSeen ? "SEEN" : "NO DATA", valueX, 102);

  clearValue(116);
  if (lastDcsPacketMs == 0) {
    tft.drawString("NEVER", valueX, 116);
  } else {
    tft.drawString(String(millis() - lastDcsPacketMs) + " ms", valueX, 116);
  }

  clearValue(130);
  tft.drawString(String(chAoa.updateCount), valueX, 130);

  clearValue(154);
  tft.drawString(String(chAoa.raw), valueX, 154);

  clearValue(168);
  tft.drawString(String(chAoa.decoded, 2), valueX, 168);

  clearValue(182);
  tft.drawString(String(aoaTargetDeg, 2), valueX, 182);

  clearValue(196);
  tft.drawString(String(aoaDisplayedDeg, 2), valueX, 196);

  clearValue(220);
  tft.drawString(nvgMode ? "ON" : "OFF", valueX, 220);

  clearValue(234);
  tft.drawString(String(chHudBrtSw.raw), valueX, 234);

  clearValue(248);
  tft.drawString(dcsNvgProxyValid ? "YES" : "NO", valueX, 248);

  clearValue(262);
  tft.drawString(String(staleCount), valueX, 262);

  clearValue(276);
  tft.drawString(String(frameCounter), valueX, 276);

  clearValue(290);
#if ML_F16_EDITION_PRO
  tft.drawString("PRO", valueX, 290);
#else
  tft.drawString("COMMUNITY", valueX, 290);
#endif
#endif
}


void renderCurrentState() {
  switch (currentState) {
    case SYS_BOOT:
      break;

    case SYS_STANDBY:
      renderStandby();
      break;

    case SYS_SYNC:
      renderSync();
      break;

    case SYS_LIVE:
      renderInstrumentLive(false);
      break;

    case SYS_STALE:
      renderInstrumentLive(true);
      break;

    case SYS_DIAG:
      renderDiagnostics();
      break;

    case SYS_OFF:
    default:
      tft.fillScreen(COL_BLACK);
      break;
  }

  lastRenderedState = currentState;
}

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------

void setup() {
  bootMs = millis();

#if ENABLE_NVG_MODE
  nvgMode = START_IN_NVG_MODE;
#endif

#if ENABLE_REAR_DIAG_BTN
  pinMode(PIN_REAR_DIAG_BTN, INPUT_PULLUP);
#endif

  setupDisplay();

  currentState = SYS_BOOT;
  lastRenderedState = SYS_BOOT;

  runStartupSequence();

#if USE_DCS_BIOS
  DcsBios::setup();
#endif

  currentState = SYS_STANDBY;
  invalidateStaticFace();

  lastFrameMs = millis();
}

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------

void loop() {
#if USE_DCS_BIOS
  DcsBios::loop();
#endif

  updateRearDiagnosticButton();
  updateTestValues();
  updateSystemState();

  aoaTargetDeg = clampFloat(aoaTargetDeg, AOA_MIN_DEG, AOA_MAX_DEG);

  aoaDisplayedDeg += (aoaTargetDeg - aoaDisplayedDeg) * 0.16f;
  aoaDisplayedDeg = clampFloat(aoaDisplayedDeg, AOA_MIN_DEG, AOA_MAX_DEG);

  if ((millis() - lastFrameMs) >= FRAME_MS) {
    lastFrameMs = millis();
    frameCounter++;

    renderCurrentState();
  }
}