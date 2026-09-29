# AmiTablet

AmiTablet is a Bluetooth HID remote for Flipper Zero, designed for tablets and desktop systems.

It provides quick access to mouse and navigation controls that are awkward or missing on some devices.

## V1.1 controls

- **UP** — Scroll up
- **DOWN** — Scroll down
- **LEFT** — Copy (Ctrl+C)
- **RIGHT** — Paste (Ctrl+V)
- **OK** — Right-click
- **BACK** — Screenshot (sends Print Screen to Windows)
- **Hold BACK** — Exit AmiTablet

Holding **Up** or **Down** repeats the mouse-wheel action.

**LEFT** sends HID Ctrl down + C press, then releases C and Ctrl, so the
connected host performs a normal Copy. **RIGHT** does the same with V
(Paste). No browser Back/Forward consumer keys are used anymore.

A short **BACK** press sends the HID keyboard Print Screen key to the
connected Windows host, which takes a normal full-screen screenshot.
A long **BACK** press exits AmiTablet and never triggers a screenshot.

## Target

- Flipper Zero
- Momentum firmware
- Windows / Android
- Bluetooth HID

## V1.1 screen

Portrait UI (`ViewPortOrientationVerticalFlip`, 64x128) reproducing
`image.png` pixel-for-pixel where possible:

1. Header frame: `AMITABLET` title, discreet `v1.1` label at the
   upper-right, filled `BT-OK` / `BT-WAIT` capsule.
2. Large black block with a white circular control area: `UP` /
   `SCROLL` on top, `SCROLL` / `DOWN` at the bottom, `CTRL` + `+` +
   `C` (copy) on the left, `CTRL` + `+` + `V` (paste) on the right,
   black diamond in the center (right-click).
3. Footer strip: `HOLD BACK TO EXIT` (long press exits).

All texts, the circle and the diamond are bitmaps extracted from the
64x128 reference. Composed lines (`v1.1`, `BT-WAIT`) reuse the
reference glyphs; the version dot is a clean single pixel.

## Bluetooth

V0.2 gave AmiTablet its own Bluetooth identity
(kept unchanged since, including in V1.1):

- advertised name: **AmiTablet**
- stable BLE address derived from the Flipper address
- dedicated pairing/bond storage

This avoids mixing AmiTablet pairing keys with the normal Flipper Bluetooth identity.

The status line shows:

- `BLE: WAIT` — waiting for a host
- `BLE: OK` — connected

Before testing V0.2, remove any old broken AmiTablet/unknown HID pairing from Windows and pair again with the new **AmiTablet** device.

## Current status

**V1.1 circular UI and copy/paste controls**

Kept intact from previous versions:

- dedicated BLE HID identity
- dedicated bonding storage
- explicit bonding/pairing configuration
- BLE connection status
- mouse-wheel scrolling
- right mouse button
- HID keyboard copy (Ctrl+C) / paste (Ctrl+V)
- HID keyboard Print Screen screenshot
- clean BLE profile restoration when the app exits

## Notes

Back and Forward use standard HID Consumer Control commands. Their exact behavior depends on the host OS and the foreground application.
