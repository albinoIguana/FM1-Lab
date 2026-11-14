# Startup candidate validation

This document records the hardware qualification sequence for the single-core startup candidate.

## Candidate source

- Candidate source commit: `54c2e5040fc53be130cbe9419d2b1f6494f061f3`
- Change under test: defer the own-stack BLE startup call until the platform `APP_BLE_AT_BOOT` hook, after audio interrupt registration and after the REC-held recovery/update-mode check.
- Bluetooth MIDI remains compiled into the firmware. This is a startup-order change, not a user preference redesign.

## Flash and boot qualification

Do not treat a successful native build or web-installer deployment as proof of physical hardware compatibility.

1. Confirm the candidate installer page identifies the intended build number, commit, package size, and SHA-256.
2. Preserve a full 1 MiB flash backup before installing.
3. Install from the candidate web installer.
4. Confirm a normal cold boot reaches the instrument UI and produces audio.
5. Confirm the FM1 remains usable if BLE startup fails; BLE must not prevent audio or recovery/update mode.
6. Power-cycle and repeat the boot/audio check.
7. Separately verify REC-held update mode remains reachable.

Record the exact installer build identity and any crash report before changing source again.
