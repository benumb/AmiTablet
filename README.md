# AmiTablet

AmiTablet is a Bluetooth HID remote for Flipper Zero, designed for tablets and desktop systems.

It provides quick access to mouse and navigation controls that are awkward or missing on some devices.

## V1.0 controls

- **UP** — Scroll up
- **DOWN** — Scroll down
- **LEFT** — Back
- **RIGHT** — Forward
- **OK** — Right-click
- **BACK** — Screenshot (sends Print Screen to Windows)
- **Hold BACK** — Exit AmiTablet

Holding **Up** or **Down** repeats the mouse-wheel action.

A short **BACK** press sends the HID keyboard Print Screen key to the
connected Windows host, which takes a normal full-screen screenshot.
A long **BACK** press exits AmiTablet and never triggers a screenshot.

## Target

- Flipper Zero
- Momentum firmware
- Windows / Android
- Bluetooth HID

## V1.0 screen

Portrait UI (`ViewPortOrientationVerticalFlip`, 64x128) reproducing the
structure of `image.png` (approved mockup 2):

1. Header card (framed): tablet-and-stylus icon, `AmiTablet` title,
   discreet `v1.0` label at the right, inverted `BLE: WAIT` / `BLE: OK`
   capsule with Bluetooth mark and confirmation tick when connected.
2. Six framed control rows, each with a keycap pictogram, a divider and
   a label: Up = Scroll up, Down = Scroll down, Left = Back,
   Right = Forward, OK = Right-click, BACK = Screenshot.
3. Footer bar (framed): `Hold BACK` / `to exit` (long press exits).

Adaptations forced by the 64px canvas: the version sits below the title
(the mockup same-line placement overflows), the BACK keycap is a
return-arrow glyph (a `BACK` capsule plus the `Screenshot` label cannot
co-exist), and the footer hint is two lines (`Hold BACK to exit` is
91px on a single line).

## Bluetooth

V0.2 gave AmiTablet its own Bluetooth identity
(kept unchanged since, including in V1.0):

- advertised name: **AmiTablet**
- stable BLE address derived from the Flipper address
- dedicated pairing/bond storage

This avoids mixing AmiTablet pairing keys with the normal Flipper Bluetooth identity.

The status line shows:

- `BLE: WAIT` — waiting for a host
- `BLE: OK` — connected

Before testing V0.2, remove any old broken AmiTablet/unknown HID pairing from Windows and pair again with the new **AmiTablet** device.

## Current status

**V1.0 UI and screenshot control**

Kept intact from previous versions:

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
