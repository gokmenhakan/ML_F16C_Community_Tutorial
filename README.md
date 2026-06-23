# ML_F16C Community Tutorial

Community release repository for F-16C cockpit simulator panels, gauges, indicators, and supporting build notes for DCS World.

This repository contains simplified community and tutorial versions of selected ML_F16 cockpit simulator modules. The aim is to help other cockpit builders understand the basic hardware, wiring, display logic, and DCS-BIOS integration patterns used in small F-16C simulator panels.

The community releases are intended to be understandable, buildable, and suitable for hobby cockpit builders who want to learn from working examples.

They are simplified public releases, not the full private development, prototype, or commercial/pro versions.

---

## Current Community Releases

| Module                          | Version | Hardware                                  | Release tag                 |
| ------------------------------- | ------: | ----------------------------------------- | --------------------------- |
| F-16C Fuel Flow Indicator       |    v1.0 | Arduino Nano + SH1106 128x64 I2C OLED     | `v1.0-ffi-community`        |
| F-16C Speedbrake Indicator      |    v1.0 | Arduino Nano + SSD1306 128x64 I2C OLED    | `v1.0-speedbrake-community` |
| F-16C Angle of Attack Indicator |    v1.0 | ESP32-D / ESP-WROOM-32 + 2.4-inch SPI TFT | `v1.0-aoa-esp32d-community` |

---

## 1. F-16C Fuel Flow Indicator Community Edition

Release tag:

```text
v1.0-ffi-community
```

This release demonstrates a simplified Arduino Nano and SH1106 128x64 I2C OLED based Fuel Flow Indicator using DCS-BIOS for DCS World.

It includes:

* Arduino Nano support
* SH1106 128x64 I2C OLED support
* DCS-BIOS live fuel-flow input
* Five-digit cockpit-style fuel-flow display
* Startup splash
* Basic self-test
* Standby, live, and stale-data states
* Local test mode
* Basic diagnostics page
* Public fuel-flow drum calibration table

Key files:

```text
FFI_Community_Release_FB.ino
Ml F16 Ffi Community Edition Guide with Code.pdf
```

---

## 2. F-16C Speedbrake Indicator Community Edition

Release tag:

```text
v1.0-speedbrake-community
```

This release demonstrates a simplified Arduino Nano and SSD1306 128x64 I2C OLED based Speedbrake Position Indicator using DCS-BIOS for DCS World.

It provides a small cockpit-style indicator display showing the three main speedbrake states:

```text
CLOSED  = diagonal striped indication
TRANSIT = vertical transition wipe
OPEN    = 3x3 dot indication
```

The community Speedbrake release includes:

* Arduino Nano support
* SSD1306 128x64 I2C OLED support
* DCS-BIOS live speedbrake input
* Startup splash
* Standby / waiting for DCS state
* Live display state
* Stale-data timeout handling
* Local test mode for bench testing without DCS
* Simple display logic suitable for modification by other builders
* Documentation and build guide

The Speedbrake sketch uses the DCS-BIOS F-16C speedbrake indicator output:

```text
F_16C_50_SPEEDBRAKE_INDICATOR
```

Observed and validated raw-value behaviour:

```text
0 to 5000       = CLOSED
5001 to 59999  = TRANSIT
60000 to 65535 = OPEN
```

Key files:

```text
ML_F16_SPEEDBRAKE_COMMUNITY_V1_0.ino
ML F16 SPEEDBRAKE Community EditionGuide.pdf
ML F16 SPEEDBRAKE Community EditionGuide.docx
```

---

## 3. F-16C Angle of Attack Indicator Community Edition

Release tag:

```text
v1.0-aoa-esp32d-community
```

This release demonstrates a simplified ESP32-D / ESP-WROOM-32 and 2.4-inch SPI TFT based Angle of Attack tape indicator using DCS-BIOS for DCS World.

It is based on the validated ML_F16 AOA indicator work, but stripped for public community release.

The community AOA release includes:

* ESP32-D / ESP-WROOM-32 support
* 2.4-inch SPI TFT display support
* DCS-BIOS live AOA input
* Vertical AOA tape display
* Colour band rendering
* OFF flag behaviour before valid data
* Startup splash
* Standby / waiting for DCS state
* Live display state
* Stale-data timeout handling
* Rear diagnostic button support
* Local test mode for bench testing without DCS
* Public AOA calibration table
* Documentation and build guide

The AOA sketch uses the DCS-BIOS F-16C AOA output:

```text
F_16C_50_AOA_VALUE
```

Key files:

```text
ML_F16_AOA_INDICATOR_COMMUNITY.ino
ML_F16_AOA_INDICATOR_COMMUNITY.docx
```

---

## Project Status

The repository is in its early public community/tutorial phase.

Current public community modules:

* F-16C Fuel Flow Indicator Community Edition
* F-16C Speedbrake Indicator Community Edition
* F-16C Angle of Attack Indicator Community Edition

The focus of these releases is to provide working, understandable, and buildable examples rather than fully commercial cockpit products.

Advanced prototype, commercial, service, hidden diagnostics, advanced calibration tools, enclosure revisions, and protected development features are not included in the community editions.

---

## Roadmap

Planned and potential future community/tutorial releases include:

* [x] F-16C Fuel Flow Indicator Community Edition
* [x] F-16C Speedbrake Indicator Community Edition
* [x] F-16C Angle of Attack Indicator Community Edition
* [ ] F-16C Vertical Velocity Indicator Community Edition
* [ ] Basic STL/3MF builder packs for selected modules
* [ ] Wiring diagrams and build notes
* [ ] Additional OLED / TFT cockpit indicators
* [ ] Additional ML_F16 panel and gauge examples

The next likely community/tutorial module is:

```text
F-16C Vertical Velocity Indicator Community Edition
```

---

## Community vs Advanced Builds

The community releases are intended for learning, experimentation, and hobby cockpit building.

They may include:

* simplified code
* basic DCS-BIOS integration
* basic display rendering
* simple test modes
* introductory build notes
* simple wiring diagrams
* public build documentation

They do not include the full advanced prototype feature set used in private development builds, such as:

* advanced diagnostics
* hidden service modes
* advanced calibration tools
* commercial support tooling
* advanced enclosure revisions
* protected rendering or realism logic
* production-ready support packs
* private build-control framework
* commercial productisation features

The purpose is to provide enough working material for cockpit builders to build a basic version, while keeping the more advanced private and commercial development work protected.

---

## Typical Hardware Used

Community modules currently use simple, low-cost hardware such as:

* Arduino Nano or compatible ATmega328P board
* ESP32-D / ESP-WROOM-32 board
* Small I2C OLED displays
* Small SPI TFT displays
* USB serial connection to the DCS-BIOS host PC
* Optional rear pushbuttons for diagnostics or local testing
* 3D-printed or laser-cut mounting parts, where released

Exact hardware requirements are documented in each module guide.

---

## DCS-BIOS Notes

These community builds are designed for DCS World and the F-16C Viper module.

They use DCS-BIOS to receive cockpit data from the simulator and drive external Arduino or ESP32 based displays.

For best results, use a current F-16C capable DCS-BIOS fork and DCS-BIOS Bridge or equivalent serial connection method.

If a module remains in standby mode, check:

* DCS World is running
* The F-16C is loaded
* DCS-BIOS is installed correctly
* DCS-BIOS Bridge is running
* The correct Arduino or ESP32 serial port is selected
* The Arduino Serial Monitor is not using the same port
* The required F-16C DCS-BIOS control name exists in your installed fork

---

## Builder Feedback

If you build one of the modules and find an issue, useful feedback includes:

* wiring corrections
* library/version problems
* DCS-BIOS control-name changes
* display-driver issues
* unclear instructions
* missing diagrams
* build photos or test results

This kind of feedback is more useful than view counts because it shows whether the releases are understandable and buildable.

---

## Follow the Project

If you are interested in future F-16C cockpit simulator module releases, please star the repository to follow the project.

More ML_F16 community modules and build notes may be added over time.

---

## Disclaimer

This is an independent community cockpit-simulator project for hobby use.

It is not affiliated with, endorsed by, or approved by Eagle Dynamics, DCS World, General Dynamics, Lockheed Martin, the United States Air Force, or any official F-16 programme.

DCS World and F-16C Viper are trademarks of their respective owners.

All aircraft references are used for simulation, educational, and hobby cockpit-building purposes only.
