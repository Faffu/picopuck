# Testing

Record every run: date, firmware checksum, console and controller firmware
versions, and what actually happened. An entry without a result is not a pass.

## Software

```
./Tools/pico-bridge/run_tests.sh                                     # host tests
./Tools/pico-bridge/build.sh build/bridge                            # firmware
./Tools/pico-bridge/build.sh build/capture -DOGXM_FORCE_CAPTURE=ON   # capture firmware
```

Host tests cover:

* Steam Controller 2 state reports copied from a real capture: a button, a
  trigger pulled through its click, a stick at full travel, a trackpad click.
* Back paddle bits, and captured status and mouse reports plus a truncated
  report being rejected without touching the state.
* The rumble output report encoding.
* Switch Pro stick packing at centre and both ends, motion conversion with axis
  remap and offsets, rumble decoding from a neutral and a strong packet.
* The capture ring: order, truncation, drops when full, reports ignored until
  enabled, log text chunking.

Not covered by host tests: the input reset on disconnect, the USB protocol and
flash storage, all of which depend on the Pico runtime and are checked on
hardware.

Latest results, 2026-09-14:

| Check | Result |
| --- | --- |
| Host tests | Pass |
| Bridge firmware build | Pass, `artifacts/bridge/sha256.txt` |
| Capture firmware build | Pass, `artifacts/capture/sha256.txt` |
| Baseline OGX-Mini build | Pass, 2026-09-10, `artifacts/baseline/` |

Builds embed a timestamp, so a rebuild produces a different checksum from the
same source. Compare commits, not checksums.

## Hardware log

Controller: Steam Controller 2, `28de:1303`, firmware revision `6a628345`.
Board: Pico 2 W. Host: Windows 11
(`10.0.26100`), at first through a VIA Labs `2109:2817` USB hub.

### 2026-09-13, capture mode

* Found by BLE scan, appearance gamepad, HID service. Pairing first failed with
  SM reason 3 (authentication requirements); passes once LE Secure Connections
  are enabled.
* Reports arrive as HID over GATT, not over the Valve GATT service the original
  Steam Controller uses. State report `0x45` at about 133 Hz, plus lizard mode
  mouse `0x40`, keyboard `0x41` and status `0x43`.
* One capture pressing each control in turn; sample reports from it are in the
  host tests. The Steam and quick access buttons are not in it; their bits come
  from SDL.

### 2026-09-13 and 14, gamepad firmware on Windows

* XInput: recognised, buttons, sticks and triggers work in a browser gamepad
  tester. Rumble from the tester reaches the controller and stops. Pass.
* Start and Back: SDL's naming swapped them, reverted. On 2026-09-14 they were
  reported inverted again; not rechecked since. Open.
* USB "device not recognised" (code 43, device descriptor failure) was seen in
  gamepad modes behind the hub. It went away after bounding USB string
  descriptor lookups, which read past the table when Windows asked for index
  `0xEE`.
* Switch Pro mode: first code 43 behind the hub. After the subcommand reply fix
  (fields one byte late) and the 2wiCC SPI data, it is recognised on Windows.
  Whether the hub or the firmware caused the earlier failure was not isolated.
  Steam not yet tried.
* HORI (Switch wired) and DInput: recognised. Pass.
* Mode combos switch modes. Pass.
* Bonds were lost on every mode change: BTstack's flash write was refused
  because core0 was not a lockout victim. Fixed.
* 2026-09-15, capture build: the stored bond list survives a reboot, so bonds
  are saved. After the reboot the controller advertises directed
  (`ADV_DIRECT_IND`, no data) and undirected but not discoverable, without an
  appearance; Bluepad32 only connects to advertising with a gamepad appearance
  and ignored it. Bluepad32 now reconnects to bonded addresses advertising
  like that. Reconnect verified on 2026-09-16.
* 2026-09-16, Switch 2 in Switch Pro mode: the controller reconnects after a
  reboot (reconnect fix verified), but the console gets no input. Two causes
  found in code: every output report from the host was echoed back as an input
  report (OGX-Mini's generic HID callback), and only the last output report
  between two USB polls was kept, so a subcommand could be lost behind a
  rumble report and the handshake stall. Both fixed; the console still showed
  no controller at first, then the Switch 2 recognised the controller and
  received input. Gyro, rumble and a long session not checked yet.
* Later on 2026-09-14 mode combos stopped working, with either Menu or View.
  Cause found in code: after each report Bluepad32 also delivered its generic
  parser's empty state, zeroing PadIn every other update, so a combo rarely
  held for three seconds and input could flicker. Fixed, to verify.

## Checks still to run

### Steam Controller 2

1. Reconnect without pairing again after a mode change and after a Pico power cycle.
2. Reconnect after the controller is switched off and on.
3. Start (Menu ≡) is button 9 and Back (View ⧉) is button 8 in XInput.
4. Steam and quick access buttons.
5. Input stops cleanly when the controller is switched off, nothing stays held.

### Switch 2, USB dock, wired Pro Controller communication enabled

1. The console recognises the controller.
2. Buttons and sticks in a game.
3. Gyro aiming: correct orientation on all three axes, no drift after a minute.
4. Rumble starts and stops, and follows the intensity setting.
5. Mode change combo works and survives a power cycle.
6. Thirty minutes of play with no disconnect or freeze.

### Windows

1. Switch Pro mode in Steam: recognised, input test, gyro, rumble.

### Linux

1. XInput recognised, `evtest` shows the events.
2. Rumble.

### Record

For each session note: Pico firmware checksum and commit, Steam Controller 2
firmware version, Switch 2 system version, Windows and Linux versions, and the
result of every check above.
