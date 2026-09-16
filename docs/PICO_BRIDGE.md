# Steam Controller 2 → Pico 2 W → USB bridge

Fork of [OGX-Mini](https://github.com/wiredopposite/OGX-Mini) that takes a Steam
Controller 2 in over Bluetooth Low Energy and presents it over USB as a Nintendo
Switch Pro Controller (default) or as any of the OGX-Mini pads, XInput included.

Repository: [`Faffu/picopuck`](https://github.com/Faffu/picopuck). The OGX-Mini sources and
their dependencies (Bluepad32, BTstack, libfixmath, tinyusb) are vendored with
the fork's patches already applied.

Status: [ROADMAP.md](ROADMAP.md). Checks and results: [TESTING.md](TESTING.md).

## Architecture

```
Steam Controller 2 --BLE HID over GATT--> BTstack / Bluepad32 (core1)
                                                |
                                  ogxm_input_report_hook
                                  (capture ring, SteamController2::decode)
                                                |
                                           Gamepad state
                                   (PadIn + PadMotion + PadOut)
                                                |
                   SwitchProDevice / XInputDevice / ... (core0) --USB--> console or PC
```

* **Input.** The Steam Controller 2 identifies as `28de:1303`, pairs with LE
  Secure Connections ("just works") and sends HID over GATT reports.
  Bluepad32's generic report entry `uni_hid_parse_input_report` calls
  `ogxm_input_report_hook` first (patch in
  `Firmware/external/patches/bluepad32_uni.diff`). The hook copies every report
  to the capture ring and, for `28de:1303`, decodes it in
  `Firmware/RP2040/src/SteamController2/` instead of letting Bluepad32 fall back
  to its Android parser. State report `0x45` carries everything; its layout is
  documented in `SteamController2.h`. Mouse (`0x40`), keyboard (`0x41`) and
  status (`0x43`) reports from the controller's lizard mode are ignored.
* **State.** `Gamepad` gained `PadMotion`: accelerometer in milli-g, gyroscope in
  0.1 deg/s, both trackpads and the back paddles, each with a validity bit.
  Buttons, sticks and triggers keep using the existing `PadIn` mapping and
  profile scaling, so user profiles apply unchanged.
* **Output.** `SwitchProDevice` (`USBDevice/DeviceDriver/SwitchPro/`) implements
  the wired Pro Controller: vendor `0x80` commands, `0x01` subcommands with
  SPI flash reads, the `0x30` input report every 8 ms carrying motion, and
  rumble decoding back into `PadOut`. The other modes are the existing
  OGX-Mini drivers.
* **Rumble to the controller.** `PadOut` is sent as output report `0x80` over
  GATT every 40 ms while rumble is on; the controller stops by itself about
  50 ms after the last one. Other Bluetooth controllers keep Bluepad32's rumble
  path, refreshed every 240 ms.
* **Disconnect.** Losing the controller resets input, motion and rumble state, so
  nothing is left held down.
* **Flash.** OGX-Mini settings (mode, profiles) use the last 4 flash sectors,
  BTstack's Bluetooth bonds the 2 sectors below them. On the Pico W boards both
  cores are multicore lockout victims, so a flash write on one core pauses the
  other.

## Button mapping

| Steam Controller 2 | Gamepad | Switch Pro | XInput |
| --- | --- | --- | --- |
| A / B / X / Y | A / B / X / Y | B / A / Y / X (Nintendo positions) | A / B / X / Y |
| LB / RB | LB / RB | L / R | LB / RB |
| LT / RT | analog triggers, full at the click | ZL / ZR | LT / RT |
| Menu (≡) | Start | + | Start |
| View (⧉) | Back | − | Back |
| Steam | Home | Home | Guide |
| Quick access (…) | Misc | Capture | none |
| Stick clicks | L3 / R3 | L3 / R3 | L3 / R3 |
| D-pad, sticks | D-pad, sticks | D-pad, sticks | D-pad, sticks |
| Trackpads, L4 R4 L5 R5 | kept in `PadMotion` | not sent | not sent |

The Menu and View bits are `0x40` and `0x4000` as tested on the controller;
SDL names them the other way round. Their mapping was reported inverted on
2026-09-14 and is still being checked, see [TESTING.md](TESTING.md).

## Build

```
./Tools/pico-bridge/build.sh                                    # build/pico2w
./Tools/pico-bridge/build.sh build/bridge                       # pick a directory
./Tools/pico-bridge/build.sh build/capture -DOGXM_FORCE_CAPTURE=ON  # capture build
```

The script fetches pico-sdk 2.1.0 into `/tmp/pico-sdk-2.1.0` (override with
`PICO_SDK_PATH`), applies two fixes it needs on a modern host, and configures a
Release build for `PI_PICO2W` with `MAX_GAMEPADS=1`. Extra arguments go to
CMake.

* pioasm's headers miss `<cstdint>` under GCC 15 and later.
* The patch step reads git's English error text, so the build runs under
  `LC_ALL=C`. With vendored sources the patch step is skipped.

Requirements: `arm-none-eabi-gcc`, `cmake`, `ninja`, `git`, `python3`.

Builds are not byte-identical: `BUILD_DATETIME` is compiled in, so the checksum
changes on every build. `artifacts/bridge/` holds the firmware with its
checksum, `artifacts/capture/` the capture build, `artifacts/baseline/` the
unmodified OGX-Mini build.

## Flash

Hold BOOTSEL while plugging the Pico 2 W in, then copy the `.uf2` to the
`RP2350` drive. The board reboots into the firmware. Settings and bonds live in
their own flash sectors, which a `.uf2` does not overwrite.

## Pairing

1. Power the Pico 2 W. The LED blinks while it scans.
2. Put the Steam Controller 2 into pairing mode, close to the Pico, with no
   other host (PC, Steam, puck) it could connect to instead.
3. The LED goes solid once the controller is connected.

The bond is written to flash and survives reboots (verified 2026-09-15). A
bonded Steam Controller 2 comes back with directed or non-discoverable
advertising and no appearance, which stock Bluepad32 ignores; the patched
advertising handler connects to bonded addresses advertising like that, so
power-ups and mode changes should reconnect on their own (to verify).

## Modes

Hold the combo for three seconds; the board stores the mode and reboots into it.
Start is the Menu (≡) button.

| Mode | Combo |
| --- | --- |
| Switch Pro (default) | Start + RB + D-pad Down |
| Switch wired (HORI) | Start + D-pad Down |
| XInput | Start + D-pad Up |
| DInput | Start + RB + D-pad Left |
| PS3 | Start + D-pad Left |
| PlayStation Classic | Start + A |
| Original Xbox | Start + D-pad Right |
| Web app configuration | Start + LB + RB |
| Report capture | Start + LB + D-pad Down |

On a Switch or Switch 2, enable wired Pro Controller communication in the
controller settings, otherwise the console only uses USB for pairing
information.

On Windows the Switch Pro mode shows up as a Pro Controller, not as a plain
Windows gamepad; Steam and SDL support Pro Controllers, not yet checked with
this firmware.

## Capturing controller reports

Capture mode records what the controller actually sends, so the decoder can be
checked and extended. No adapter or programmer is needed.

1. Get into capture mode, either with Start + LB + D-pad Down, or by flashing
   the capture build (`artifacts/capture/picopuck-capture-PI_PICO2W.uf2`),
   which always boots into capture and ignores mode combos. Use the capture
   build when the decoder can't read the buttons yet. The board enumerates as a
   serial port.
2. Open the port with DTR set. Linux: `cat /dev/ttyACM0 | tee capture.log`.
   Windows: `powershell -ExecutionPolicy Bypass -File Tools\pico-bridge\capture.ps1 -Port COM5`,
   which writes `capture.log`, shows the latest line a few times a second and
   adds a `# mark <key>` line for every key pressed, to tag what you do on the
   controller. Use letters as marks; Enter records an empty mark.
3. Pair or connect the controller. Every Bluetooth input report is printed as
   one line: timestamp in seconds, length, then the bytes in hex. The capture
   build also prints Bluepad32's info log (discovery, pairing, GATT, VID/PID)
   and the HID descriptor (`# hd` lines).
4. Press one control at a time and hold it for a moment, with a mark before
   each, then rotate the controller for the motion fields.

Reports and log text share a 256 entry ring between the radio and USB. Reports
are only queued while the port is open, so boot and pairing log text is not
crowded out. If the terminal can't keep up the board prints a
`# dropped N, truncated M` line, so a gap is visible rather than silent.

## Tuning

* Controller counts to internal motion units:
  `SteamController2::MotionScale` (defaults: ±2 g, ±2000 deg/s).
* Orientation, sensitivity and offsets of the motion data sent to the Switch:
  `SwitchProDevice::imu_calibration()`, a `SwitchProCodec::ImuCalibration` with
  a per axis remap, num/den scaling and offsets. The axes are not yet mapped to
  the Switch orientation.
* Rumble strength from the Switch Pro side: `SwitchProDevice::rumble_intensity()`,
  0-255.

## Tests

```
./Tools/pico-bridge/run_tests.sh
```

Builds the decoder, the capture ring and the Switch Pro conversions on the host
and asserts over reports captured from the real controller, rejected reports,
rumble encoding, stick packing, motion conversion, rumble decoding and the
capture ring.

## Attribution

The Steam Controller 2 report layout was measured from captures and checked
against SDL's `SDL_hidapi_steam_triton.c` (zlib, Valve Corporation), which also
gives the button names, the sensor ranges and the haptic rumble output report.
The original Steam Controller path is Bluepad32's `uni_hid_parser_steam.c`
(Apache-2.0, Ricardo Quesada). The Switch Pro protocol follows the public
Nintendo Switch reverse engineering notes and 2wiCC (MIT, KNfLrPn,
https://github.com/knflrpn/2wiCC), whose USB descriptors match ours byte for
byte and whose SPI flash contents and subcommand replies are reused. OGX-Mini
itself is GPL-3.0; see `LICENSE`.
