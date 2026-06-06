/*********************************************************************
 * F16_FUEL_FLOW_INDICATOR_COMMUNITY
 * COMMUNITY EDITION
 *
 * F-16C Fuel Flow Indicator for DCS World
 * Arduino Nano + SH1106 128x64 I2C OLED
 * U8g2 Page Buffer + DCS-BIOS FP Fork
 *
 * Community release scope:
 *   - Working DCS-BIOS driven Fuel Flow Indicator
 *   - 5-digit cockpit display: [10K] [1K] [100] [0] [0]
 *   - Basic rolling digit display using the public calibration table below
 *   - Startup splash
 *   - Standby / live / stale / diagnostics states
 *   - Optional rear diagnostics button
 *   - Optional local test values for bench testing
 *
 * Deliberately excluded from this community edition:
 *   - Commercial/pro edition control switches
 *   - Advanced maintenance/test-panel integration
 *   - DCS-driven NVG/dim proxy handling
 *   - Serial capture logging
 *   - Extended service diagnostics
 *   - Project-specific branding and release metadata
 *********************************************************************/

// -------------------------------------------------------------------
// DCS-BIOS CONFIG
// -------------------------------------------------------------------

#define DCSBIOS_IRQ_SERIAL
#include "DcsBios.h"

// -------------------------------------------------------------------
// DISPLAY LIBRARIES
// -------------------------------------------------------------------

#include <Wire.h>
#include <U8g2lib.h>

// -------------------------------------------------------------------
// OLED CONFIG
// -------------------------------------------------------------------

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

// -------------------------------------------------------------------
// BUILD INFO
// -------------------------------------------------------------------

#define BUILD_NAME        "F16_FUEL_FLOW_COMMUNITY"
#define BUILD_PHASE       "COMMUNITY_EDITION_V1"
#define BUILD_DATE        "2026-05-22"
#define PANEL_ID          "F16_FFI"
#define HARDWARE_TARGET   "ARDUINO_NANO_SH1106_128x64"

// -------------------------------------------------------------------
// COMMUNITY FEATURE SWITCHES
// -------------------------------------------------------------------

#define USE_DCS_BIOS            1
#define USE_TEST_VALUES         0
#define ENABLE_DIAGNOSTICS      1
#define ENABLE_REAR_DIAG_BTN    1
#define SHOW_STARTUP_SPLASH     1
#define ENABLE_SELF_TEST        1

// Normal cockpit boot. Set to 1 only when testing the diagnostics page.
#define START_IN_DIAGS          0

// -------------------------------------------------------------------
// PIN CONFIG
// -------------------------------------------------------------------

#define PIN_DIAG_BUTTON         7

// -------------------------------------------------------------------
// TIMING
// -------------------------------------------------------------------

#define UPDATE_INTERVAL_MS      50
#define DCS_TIMEOUT_MS          3000
#define SPLASH_TIME_MS          1200
#define SELF_TEST_DELAY_MS      80
#define DIAG_HOLD_MS            1200
#define STANDBY_BLINK_MS        600

// -------------------------------------------------------------------
// DIGIT / DRUM DISPLAY CONFIG
// -------------------------------------------------------------------

#define DIGIT_COUNT             5
#define LIVE_DCS_DRUM_COUNT     3

#define DIGIT_SPACING           24
#define DIGIT_BASE_Y            52
#define DIGIT_SCROLL_TRAVEL     40

#define WINDOW_TOP_Y            19
#define WINDOW_BOTTOM_Y         61

#define MASK_TOP_X              0
#define MASK_TOP_Y              0
#define MASK_TOP_W              128
#define MASK_TOP_H              WINDOW_TOP_Y

#define MASK_BOTTOM_X           0
#define MASK_BOTTOM_Y           WINDOW_BOTTOM_Y
#define MASK_BOTTOM_W           128
#define MASK_BOTTOM_H           (64 - WINDOW_BOTTOM_Y)

#define INVERT_BOX_X            0
#define INVERT_BOX_Y            WINDOW_TOP_Y
#define INVERT_BOX_W            128
#define INVERT_BOX_H            (WINDOW_BOTTOM_Y - WINDOW_TOP_Y)

// -------------------------------------------------------------------
// COMMUNITY CALIBRATION TABLE
// -------------------------------------------------------------------
//
// This table is intentionally retained because the DCS-BIOS FFI drum
// outputs are not a simple 0-9 display value during transitions.
// Builders can adjust these raw values after observing their own fork.

struct DrumCalPoint {
  uint8_t digit;
  uint16_t raw;
};

const DrumCalPoint fuelFlowDrumTable[] = {
  {0,     0},
  {1,  6553},
  {2, 13106},
  {3, 19659},
  {4, 26212},
  {5, 32765},
  {6, 39318},
  {7, 45871},
  {8, 52424},
  {9, 58977}
};

#define DRUM_TABLE_COUNT        (sizeof(fuelFlowDrumTable) / sizeof(fuelFlowDrumTable[0]))
#define DRUM_SETTLE_DEADBAND    220
#define DRUM_TOP_ROLLOVER_RAW   64800

// -------------------------------------------------------------------
// SYSTEM STATES
// -------------------------------------------------------------------

// Kept intentionally simple for the community version.
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

// -------------------------------------------------------------------
// DCS RAW DRUM STRUCTURE
// -------------------------------------------------------------------

struct DrumChannel {
  uint16_t raw;
  uint8_t digit;
  uint8_t nextDigit;
  int8_t offsetY;
  unsigned long lastUpdateMs;
  uint16_t updateCount;
  bool live;
};

DrumChannel drums[LIVE_DCS_DRUM_COUNT];

// -------------------------------------------------------------------
// GLOBAL STATE
// -------------------------------------------------------------------

unsigned long lastFrameMs = 0;
unsigned long lastDcsPacketMs = 0;
unsigned long diagButtonStartMs = 0;
unsigned long syncStartMs = 0;
unsigned long lastTestStepMs = 0;

bool dcsEverSeen = false;
bool syncSeen = false;
bool localDiagnosticsMode = false;

uint16_t testCounter = 0;

// -------------------------------------------------------------------
// FORWARD DECLARATIONS
// -------------------------------------------------------------------

void updateSystemState();
void renderCurrentState();

void drawBootScreen();
void drawStandbyScreen();
void drawSyncScreen();
void drawLiveDisplay(bool stale);
void drawDiagnostics();

void renderPage(void (*drawFunc)());
void renderLivePage(bool stale);

void initialiseDrums();
void selfTestDigits();

void updateDrumFromDcs(uint8_t index, unsigned int rawValue);
void decodeCommunityDrum(uint16_t rawValue, uint8_t &digit, uint8_t &nextDigit, int8_t &offsetY);

void drawMechanicalDrum(int x, int y, uint8_t digit, uint8_t nextDigit, int8_t offsetY);
void drawFixedDigit(int x, int y, uint8_t digit);
void drawDisplayDigits();
void drawZeroDisplay();
void applyWindowMask();

void updateRearDiagnosticsButton();
void updateTestValues();

uint16_t getRenderedFuelFlow();
const char* systemStateName(SystemState state);

// -------------------------------------------------------------------
// BASIC HELPERS
// -------------------------------------------------------------------

void renderPage(void (*drawFunc)()) {
  u8g2.firstPage();
  do {
    drawFunc();
  } while (u8g2.nextPage());
}

void renderLivePage(bool stale) {
  u8g2.firstPage();
  do {
    drawLiveDisplay(stale);
  } while (u8g2.nextPage());
}

void drawCentreText(int y, const char* text) {
  int w = u8g2.getStrWidth(text);
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  u8g2.drawStr(x, y, text);
}

const char* systemStateName(SystemState state) {
  switch (state) {
    case SYS_OFF:     return "OFF";
    case SYS_BOOT:    return "BOOT";
    case SYS_STANDBY: return "STBY";
    case SYS_SYNC:    return "SYNC";
    case SYS_LIVE:    return "LIVE";
    case SYS_STALE:   return "STALE";
    case SYS_DIAG:    return "DIAG";
    default:          return "UNK";
  }
}

uint16_t getRenderedFuelFlow() {
  return ((uint16_t)drums[0].digit * 10000U) +
         ((uint16_t)drums[1].digit * 1000U) +
         ((uint16_t)drums[2].digit * 100U);
}

// -------------------------------------------------------------------
// COMMUNITY DRUM DECODER
// -------------------------------------------------------------------
//
// This is a straightforward table-based decoder. It keeps the display
// usable without exposing extended commercial diagnostics or service
// logic. Adjust the table values above if your DCS-BIOS fork differs.

void decodeCommunityDrum(uint16_t rawValue, uint8_t &digit, uint8_t &nextDigit, int8_t &offsetY) {
  if (rawValue >= DRUM_TOP_ROLLOVER_RAW) {
    digit = 0;
    nextDigit = 1;
    offsetY = 0;
    return;
  }

  uint8_t nearestIndex = 0;
  uint16_t smallestError = 65535;

  for (uint8_t i = 0; i < DRUM_TABLE_COUNT; i++) {
    uint16_t tableRaw = fuelFlowDrumTable[i].raw;
    uint16_t error = (rawValue > tableRaw) ? (rawValue - tableRaw) : (tableRaw - rawValue);

    if (error < smallestError) {
      smallestError = error;
      nearestIndex = i;
    }
  }

  uint16_t nearestRaw = fuelFlowDrumTable[nearestIndex].raw;
  int16_t residual = (int16_t)rawValue - (int16_t)nearestRaw;

  if (residual > -DRUM_SETTLE_DEADBAND && residual < DRUM_SETTLE_DEADBAND) {
    digit = fuelFlowDrumTable[nearestIndex].digit;
    nextDigit = (digit + 1) % 10;
    offsetY = 0;
    return;
  }

  if (residual > 0) {
    digit = fuelFlowDrumTable[nearestIndex].digit;
    nextDigit = (digit + 1) % 10;

    uint16_t span = 6553;
    offsetY = -(int8_t)(((uint32_t)residual * DIGIT_SCROLL_TRAVEL) / span);

    if (offsetY < -DIGIT_SCROLL_TRAVEL) {
      offsetY = -DIGIT_SCROLL_TRAVEL;
    }

    return;
  }

  uint8_t prevIndex = (nearestIndex == 0) ? 9 : nearestIndex - 1;
  uint16_t span = nearestRaw - fuelFlowDrumTable[prevIndex].raw;
  uint16_t absResidual = (uint16_t)(-residual);

  if (span == 0) {
    span = 6553;
  }

  digit = fuelFlowDrumTable[prevIndex].digit;
  nextDigit = fuelFlowDrumTable[nearestIndex].digit;
  offsetY = -(int8_t)(DIGIT_SCROLL_TRAVEL - (((uint32_t)absResidual * DIGIT_SCROLL_TRAVEL) / span));

  if (offsetY < -DIGIT_SCROLL_TRAVEL) {
    offsetY = -DIGIT_SCROLL_TRAVEL;
  }
}

// -------------------------------------------------------------------
// DCS CHANNEL UPDATE
// -------------------------------------------------------------------

void updateDrumFromDcs(uint8_t index, unsigned int rawValue) {
  if (index >= LIVE_DCS_DRUM_COUNT) {
    return;
  }

  drums[index].raw = (uint16_t)rawValue;

  decodeCommunityDrum(
    drums[index].raw,
    drums[index].digit,
    drums[index].nextDigit,
    drums[index].offsetY
  );

  drums[index].lastUpdateMs = millis();

  if (drums[index].updateCount < 65535) {
    drums[index].updateCount++;
  }

  drums[index].live = true;
  lastDcsPacketMs = millis();

  if (!dcsEverSeen) {
    dcsEverSeen = true;
    syncSeen = false;
    syncStartMs = millis();
  }
}

// -------------------------------------------------------------------
// DCS-BIOS CALLBACKS - F-16C FUEL FLOW COUNTER
// -------------------------------------------------------------------

void onFuelFlowCounter10kChange(unsigned int newValue) {
  updateDrumFromDcs(0, newValue);
}

void onFuelFlowCounter1kChange(unsigned int newValue) {
  updateDrumFromDcs(1, newValue);
}

void onFuelFlowCounter100Change(unsigned int newValue) {
  updateDrumFromDcs(2, newValue);
}

// -------------------------------------------------------------------
// DCS-BIOS BUFFERS
// -------------------------------------------------------------------
//
// In the DCS-BIOS F-16C fork used for this project, these macros expand
// to address/mask/shift parameters for IntegerBuffer.

#if USE_DCS_BIOS

DcsBios::IntegerBuffer fuelFlowCounter10kBuffer(
  F_16C_50_FUELFLOWCOUNTER_10K,
  onFuelFlowCounter10kChange
);

DcsBios::IntegerBuffer fuelFlowCounter1kBuffer(
  F_16C_50_FUELFLOWCOUNTER_1K,
  onFuelFlowCounter1kChange
);

DcsBios::IntegerBuffer fuelFlowCounter100Buffer(
  F_16C_50_FUELFLOWCOUNTER_100,
  onFuelFlowCounter100Change
);

#endif

// -------------------------------------------------------------------
// STATE MACHINE
// -------------------------------------------------------------------

void updateSystemState() {
  if (localDiagnosticsMode) {
    currentState = SYS_DIAG;
    return;
  }

  if (!dcsEverSeen) {
    currentState = SYS_STANDBY;
    return;
  }

  if ((millis() - lastDcsPacketMs) > DCS_TIMEOUT_MS) {
    currentState = SYS_STALE;
    return;
  }

  if (!syncSeen) {
    currentState = SYS_SYNC;

    if ((millis() - syncStartMs) > 800) {
      syncSeen = true;
    }

    return;
  }

  currentState = SYS_LIVE;
}

// -------------------------------------------------------------------
// DIAGNOSTICS BUTTON
// -------------------------------------------------------------------

void updateRearDiagnosticsButton() {
#if ENABLE_DIAGNOSTICS && ENABLE_REAR_DIAG_BTN

  bool pressed = digitalRead(PIN_DIAG_BUTTON) == LOW;

  if (pressed) {
    if (diagButtonStartMs == 0) {
      diagButtonStartMs = millis();
    }

    if ((millis() - diagButtonStartMs) > DIAG_HOLD_MS) {
      localDiagnosticsMode = !localDiagnosticsMode;
      diagButtonStartMs = 0;
      delay(350);
    }
  } else {
    diagButtonStartMs = 0;
  }

#endif
}

// -------------------------------------------------------------------
// TEST VALUES
// -------------------------------------------------------------------

void updateTestValues() {
#if USE_TEST_VALUES

  if ((millis() - lastTestStepMs) < 120) {
    return;
  }

  lastTestStepMs = millis();
  testCounter += 100;

  if (testCounter > 99900) {
    testCounter = 0;
  }

  uint8_t d0 = (testCounter / 10000) % 10;
  uint8_t d1 = (testCounter / 1000)  % 10;
  uint8_t d2 = (testCounter / 100)   % 10;

  drums[0].raw = fuelFlowDrumTable[d0].raw;
  drums[1].raw = fuelFlowDrumTable[d1].raw;
  drums[2].raw = fuelFlowDrumTable[d2].raw;

  for (uint8_t i = 0; i < LIVE_DCS_DRUM_COUNT; i++) {
    decodeCommunityDrum(
      drums[i].raw,
      drums[i].digit,
      drums[i].nextDigit,
      drums[i].offsetY
    );

    drums[i].live = true;
    drums[i].lastUpdateMs = millis();
  }

  dcsEverSeen = true;

  if (!syncSeen && syncStartMs == 0) {
    syncStartMs = millis();
  }

  lastDcsPacketMs = millis();

#endif
}

// -------------------------------------------------------------------
// INITIALISATION
// -------------------------------------------------------------------

void initialiseDrums() {
  for (uint8_t i = 0; i < LIVE_DCS_DRUM_COUNT; i++) {
    drums[i].raw = 0;
    drums[i].digit = 0;
    drums[i].nextDigit = 1;
    drums[i].offsetY = 0;
    drums[i].lastUpdateMs = 0;
    drums[i].updateCount = 0;
    drums[i].live = false;
  }
}

// -------------------------------------------------------------------
// SELF TEST
// -------------------------------------------------------------------

void selfTestDigits() {
#if ENABLE_SELF_TEST

  for (uint8_t d = 0; d <= 9; d++) {
    u8g2.firstPage();

    do {
      u8g2.setFont(u8g2_font_logisoso32_tn);

      char str[2];
      str[0] = '0' + d;
      str[1] = '\0';

      for (uint8_t i = 0; i < DIGIT_COUNT; i++) {
        int x = i * DIGIT_SPACING;
        u8g2.drawStr(x, DIGIT_BASE_Y, str);
      }

    } while (u8g2.nextPage());

    delay(SELF_TEST_DELAY_MS);
  }

#endif
}

// -------------------------------------------------------------------
// DIGIT DRAWING
// -------------------------------------------------------------------

void drawMechanicalDrum(int x, int y, uint8_t digit, uint8_t nextDigit, int8_t offsetY) {
  char buf[2];

  buf[0] = '0' + digit;
  buf[1] = '\0';
  u8g2.drawStr(x, y + offsetY, buf);

  if (offsetY != 0) {
    buf[0] = '0' + nextDigit;
    buf[1] = '\0';
    u8g2.drawStr(x, y + offsetY + DIGIT_SCROLL_TRAVEL, buf);
  }
}

void drawFixedDigit(int x, int y, uint8_t digit) {
  char buf[2];

  buf[0] = '0' + digit;
  buf[1] = '\0';

  u8g2.drawStr(x, y, buf);
}

void applyWindowMask() {
  u8g2.setDrawColor(0);

  u8g2.drawBox(MASK_TOP_X, MASK_TOP_Y, MASK_TOP_W, MASK_TOP_H);
  u8g2.drawBox(MASK_BOTTOM_X, MASK_BOTTOM_Y, MASK_BOTTOM_W, MASK_BOTTOM_H);

  u8g2.setDrawColor(1);
}

void drawDisplayDigits() {
  u8g2.setFont(u8g2_font_logisoso32_tn);

  for (uint8_t i = 0; i < LIVE_DCS_DRUM_COUNT; i++) {
    int posX = i * DIGIT_SPACING;

    drawMechanicalDrum(
      posX,
      DIGIT_BASE_Y,
      drums[i].digit,
      drums[i].nextDigit,
      drums[i].offsetY
    );
  }

  drawFixedDigit(3 * DIGIT_SPACING, DIGIT_BASE_Y, 0);
  drawFixedDigit(4 * DIGIT_SPACING, DIGIT_BASE_Y, 0);

  applyWindowMask();
}

void drawZeroDisplay() {
  u8g2.setFont(u8g2_font_logisoso32_tn);

  for (uint8_t i = 0; i < DIGIT_COUNT; i++) {
    drawFixedDigit(i * DIGIT_SPACING, DIGIT_BASE_Y, 0);
  }

  applyWindowMask();
}

// -------------------------------------------------------------------
// RENDERERS
// -------------------------------------------------------------------

void drawBootScreen() {
  u8g2.setFont(u8g2_font_6x10_tf);
  drawCentreText(12, "F-16C FFI");

  u8g2.setFont(u8g2_font_7x14B_tf);
  drawCentreText(30, "FUEL FLOW");

  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentreText(46, "COMMUNITY EDITION");
  drawCentreText(60, "DCS-BIOS");
}

void drawStandbyScreen() {
  bool blinkOn = ((millis() / STANDBY_BLINK_MS) % 2) == 0;

  if (blinkOn) {
    drawZeroDisplay();
  }
}

void drawSyncScreen() {
  drawDisplayDigits();
}

void drawLiveDisplay(bool stale) {
  if (stale) {
    u8g2.drawBox(INVERT_BOX_X, INVERT_BOX_Y, INVERT_BOX_W, INVERT_BOX_H);
    u8g2.setDrawColor(0);

    u8g2.setFont(u8g2_font_logisoso32_tn);

    for (uint8_t i = 0; i < LIVE_DCS_DRUM_COUNT; i++) {
      int posX = i * DIGIT_SPACING;

      drawMechanicalDrum(
        posX,
        DIGIT_BASE_Y,
        drums[i].digit,
        drums[i].nextDigit,
        drums[i].offsetY
      );
    }

    drawFixedDigit(3 * DIGIT_SPACING, DIGIT_BASE_Y, 0);
    drawFixedDigit(4 * DIGIT_SPACING, DIGIT_BASE_Y, 0);

    u8g2.setDrawColor(1);
    applyWindowMask();
  } else {
    drawDisplayDigits();
  }
}

void drawDiagnostics() {
  char buf[32];

  u8g2.setFont(u8g2_font_5x8_tf);

  snprintf(buf, sizeof(buf), "FF:%05u", getRenderedFuelFlow());
  u8g2.drawStr(0, 8, buf);

  snprintf(buf, sizeof(buf), "S:%s", systemStateName(currentState));
  u8g2.drawStr(68, 8, buf);

  snprintf(buf, sizeof(buf), "10K R%u D%u O%d", drums[0].raw, drums[0].digit, drums[0].offsetY);
  u8g2.drawStr(0, 22, buf);

  snprintf(buf, sizeof(buf), "1K  R%u D%u O%d", drums[1].raw, drums[1].digit, drums[1].offsetY);
  u8g2.drawStr(0, 34, buf);

  snprintf(buf, sizeof(buf), "100 R%u D%u O%d", drums[2].raw, drums[2].digit, drums[2].offsetY);
  u8g2.drawStr(0, 46, buf);

  snprintf(buf, sizeof(buf), "DCS:%s", dcsEverSeen ? "SEEN" : "WAIT");
  u8g2.drawStr(0, 62, buf);
}

void renderCurrentState() {
  switch (currentState) {
    case SYS_BOOT:
      renderPage(drawBootScreen);
      break;

    case SYS_STANDBY:
      renderPage(drawStandbyScreen);
      break;

    case SYS_SYNC:
      renderPage(drawSyncScreen);
      break;

    case SYS_LIVE:
      renderLivePage(false);
      break;

    case SYS_STALE:
      renderLivePage(true);
      break;

    case SYS_DIAG:
      renderPage(drawDiagnostics);
      break;

    case SYS_OFF:
    default:
      u8g2.firstPage();
      do {
      } while (u8g2.nextPage());
      break;
  }
}

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------

void setup() {
#if ENABLE_REAR_DIAG_BTN
  pinMode(PIN_DIAG_BUTTON, INPUT_PULLUP);
#endif

  initialiseDrums();

  u8g2.begin();
  u8g2.setContrast(255);

#if SHOW_STARTUP_SPLASH
  currentState = SYS_BOOT;
  renderCurrentState();
  delay(SPLASH_TIME_MS);
#endif

#if ENABLE_SELF_TEST
  selfTestDigits();
#endif

#if USE_DCS_BIOS
  DcsBios::setup();
#endif

#if START_IN_DIAGS
  localDiagnosticsMode = true;
  currentState = SYS_DIAG;
#else
  currentState = SYS_STANDBY;
#endif
}

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------

void loop() {
#if USE_DCS_BIOS
  DcsBios::loop();
#endif

  updateRearDiagnosticsButton();

#if USE_TEST_VALUES
  updateTestValues();
#endif

  updateSystemState();

  unsigned long now = millis();

  if ((now - lastFrameMs) >= UPDATE_INTERVAL_MS) {
    renderCurrentState();
    lastFrameMs = now;
  }
}
