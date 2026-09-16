# Roadmap

Plan: [PLAN.md](PLAN.md). Checklist: [../TODO.md](../TODO.md). Updated 2026-09-16.

## First release

| Requirement | State |
| --- | --- |
| Pico 2 W firmware builds reproducibly | Done, `Tools/pico-bridge/build.sh` |
| Steam Controller 2 in over BLE: pairing | Verified, LE Secure Connections |
| Steam Controller 2: bonds kept across reboots | Verified 2026-09-15 in the capture log |
| Steam Controller 2: reconnect after a reboot | Verified 2026-09-16 |
| Steam Controller 2 report decoding | Report 0x45 mapped from a capture and SDL, tested against captured reports |
| Motion, trackpads and back buttons kept in the gamepad state | Done |
| Rumble to the Steam Controller 2 | Verified from XInput on Windows |
| XInput output | Verified on Windows: recognised, input, rumble |
| Switch Pro USB output | Recognised on Windows and by the Switch 2 through the dock, with input; Steam not yet tried |
| Other OGX-Mini modes | HORI and DInput recognised on Windows |
| Mode combos and persistence, Switch Pro as default | Verified 2026-09-16 |
| Capture mode for recording raw reports | Done and used, USB serial, no adapter needed |
| Switch 2: recognition, gyro, rumble, 30 minutes of play | Recognition and input verified 2026-09-16; gyro, rumble, long session to do |

The release is complete only after the hardware checks in
[TESTING.md](TESTING.md) pass. Nothing here claims console compatibility before
that.

## Known gaps

* Menu and View (Start and Back) were reported inverted on 2026-09-14 after
  being swapped back; to recheck on the current firmware.
* Reconnect only matches bonded devices advertising from their stored address are matched; a
  controller rotating resolvable private addresses would not be.
* The IMU axes are not yet mapped to the Switch orientation; needs checking in
  a game or in Steam's controller test.
* The controller stays in lizard mode and also sends mouse (0x40) and keyboard
  (0x41) reports. They are ignored; SDL turns lizard mode off with a feature
  report, the bridge doesn't yet.
* Motion is sent as one sample repeated across the three slots of the Switch
  report. Good enough for aiming; a real 5 ms history would need buffering.
* Rumble decoding from the Switch keeps amplitude and drops frequency.
* Trackpads and back paddles reach the gamepad state but no output mode.
* The Switch Pro mode once failed to enumerate on Windows (code 43) behind a
  USB hub; it enumerates now, but the cause was not isolated.

## After the first release

* Xbox One controller as a Bluetooth input.
* The OpenPuck modes that OGX-Mini does not have yet, each with a table of
  features and the platforms actually tested.
* Trackpads and back paddles exposed to the output modes.
