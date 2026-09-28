# AmiTablet

AmiTablet is a Bluetooth HID remote for Flipper Zero, designed for tablets.

It provides quick access to mouse and navigation controls that are awkward or missing on many tablets.

## V0.1 controls

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
- Android tablets first
- Bluetooth HID

## Bluetooth

AmiTablet starts its own BLE HID profile and advertises while the app is open.

On first use, pair the Flipper with the tablet while AmiTablet is running. Pairing information is stored in AmiTablet's app data so later launches can reconnect.

The status line shows:

- `BLE: WAIT` — waiting for the tablet
- `BLE: OK` — connected

## Current status

**V0.1 implementation**

Implemented:

- Bluetooth HID startup
- BLE connection status
- Mouse-wheel scrolling
- Right mouse button
- HID Consumer Back / Forward navigation
- Clean BLE profile restoration when the app exits

## Notes

Back and Forward use standard HID Consumer Control commands. Their exact behavior can depend on the Android version and the application currently in the foreground.

## Roadmap

- Test V0.1 on a real tablet
- Tune scroll direction/speed if needed
- Add an optional mouse mode
- Add left-click support in mouse mode
- Optional media controls
