# AmiTablet

AmiTablet is a Bluetooth HID remote for Flipper Zero, designed for tablets and desktop systems.

It provides quick access to mouse and navigation controls that are awkward or missing on some devices.

## V0.2 controls

- **Up** — Scroll up
- **Down** — Scroll down
- **Left** — Back
- **Right** — Forward
- **OK** — Right click
- **BACK** — Exit AmiTablet

Holding **Up** or **Down** repeats the mouse-wheel action.

## Target

- Flipper Zero
- Momentum firmware
- Windows / Android
- Bluetooth HID

## Bluetooth

V0.2 gives AmiTablet its own Bluetooth identity:

- advertised name: **AmiTablet**
- stable BLE address derived from the Flipper address
- dedicated pairing/bond storage

This avoids mixing AmiTablet pairing keys with the normal Flipper Bluetooth identity.

The status line shows:

- `BLE: WAIT` — waiting for a host
- `BLE: OK` — connected

Before testing V0.2, remove any old broken AmiTablet/unknown HID pairing from Windows and pair again with the new **AmiTablet** device.

## Current status

**V0.2 pairing fix**

Implemented:

- dedicated BLE HID identity
- dedicated bonding storage
- explicit bonding/pairing configuration
- BLE connection status
- mouse-wheel scrolling
- right mouse button
- HID Consumer Back / Forward navigation
- clean BLE profile restoration when the app exits

## Notes

Back and Forward use standard HID Consumer Control commands. Their exact behavior depends on the host OS and the foreground application.
