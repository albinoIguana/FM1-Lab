#!/usr/bin/env python3
"""Guard the startup ordering that prevents BLE from locking the audio vector."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
app = (root / "app" / "app.c").read_text()
header = (root / "app" / "app.h").read_text()
base = (root / "base" / "device" / "main_fm1.c").read_text()

# Extract app_init's body, not comments or the separate runtime BLE toggle.
start = app.index("void app_init(void)")
brace = app.index("{", start)
depth = 0
end = None
for i in range(brace, len(app)):
    if app[i] == "{":
        depth += 1
    elif app[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
if end is None:
    raise SystemExit("startup BLE test: app_init body is unterminated")
if "plat_ble_enable" in app[brace:end]:
    raise SystemExit("startup BLE test: app_init must not start BLE before audio_init")

if '#define APP_BLE_AT_BOOT (!fm1lab_ble_off())' not in header:
    raise SystemExit("startup BLE test: APP_BLE_AT_BOOT must reflect the saved BLE setting")

main_start = base.index("static void fm1_main(void)")
main_end = base.index("/* one boot-packed load image", main_start)
main = base[main_start:main_end]
positions = {
    "app_init": main.index("app_init();"),
    "audio_init": main.index("audio_init();"),
    "top_guard_lock": main.index("fm1_guard_lock_top();"),
    "radio_boot_note": main.index("radio_boot_note();"),
    "deferred_ble_hook": main.index("if (!radio_last_bad && APP_BLE_AT_BOOT)"),
    "ble_start": main.index("ble_start();", main.index("if (!radio_last_bad && APP_BLE_AT_BOOT)")),
}
ordered = list(positions.values())
if ordered != sorted(ordered):
    raise SystemExit(f"startup BLE test: platform startup order is wrong: {positions}")

print("BLE startup order: app_init -> audio IRQ -> timer IRQ/top guard -> guarded BLE startup")
