# ML_F16C Community Tutorial

Community release repository for F-16C cockpit simulator panels and gauges for DCS World.

## Current Release

### F-16C Fuel Flow Indicator Community Edition V1

This release provides a working Community Edition sketch for the F-16C Fuel Flow Indicator.

## Target Hardware

- Arduino Nano / ATmega328P
- SH1106 128x64 I2C OLED display
- Optional rear diagnostics button
- USB serial connection to the DCS-BIOS host PC

## Software Requirements

- Arduino IDE
- U8g2 library
- Skunk Works DCS-BIOS fork
- DCS-BIOS Bridge
- DCS World F-16C

## Included Features

- Startup splash
- Basic digit self-test
- Standby mode
- Live DCS-BIOS fuel-flow display
- Stale-data indication
- Simple rear-button diagnostics page
- Local test mode
- Public fuel-flow drum calibration table

## Files

- `FFI_Community_Release_FB.ino` — Arduino sketch
- `Ml F16 Ffi Community Edition Guide with Code.pdf` — setup and usage guide

## Important Notes

This is a simplified Community Edition. Advanced prototype, service, diagnostics, and commercial/pro features are not included.

## Disclaimer

This project is a hobby cockpit-simulator component for use with DCS World. It is not affiliated with, endorsed by, or approved by Eagle Dynamics, Lockheed Martin, the United States Air Force, or any aircraft manufacturer/operator.

Simulation and educational use only.
