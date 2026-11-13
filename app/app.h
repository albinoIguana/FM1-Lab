/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The FM1-Lab: the smallest complete fm1-base firmware. Copy this directory to start a firmware.
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "FM1 LAB"
#define APP_SPLASH
#define APP_SPLASH_RGB RGB(42, 210, 184)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_OVERLOAD                    /* shed a voice gracefully when the measured render exceeds 90% */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_BTDEV                        /* Bluetooth MIDI devices (lib/btdev.h) */
#define APP_BLE_AT_BOOT (!fm1lab_ble_off())   /* Bluetooth at power-on: as the player left it (REC held) */
int fm1lab_ble_off(void);

/* the store: the app's objects, the panel calibration (the platform's) and Bluetooth's address and pairing among
 * them, and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_INSTRUMENTS0, OBJ_INSTRUMENTS1, OBJ_INSTRUMENTS2, OBJ_INSTRUMENTS3, OBJ_INSERTS, OBJ_NOBJ };
/* Preserve Felucca's projects (0x97000..0x9EFFF), FM6 bank (0x9F000/0xFE000),
 * samples (0xA0000..0xDBFFF), user preset banks (0xDC000..0xDFFFF),
 * autosave (0xE5000..0xE6FFF), voice bank (0xE7000) and globals (0xFC000+).
 * App objects live in the otherwise unused user-reserved sectors EA000..EFFFF;
 * the 12-sector pool at F0000..FBFFF holds four independent patch banks plus the Instrument directory. */
#define APP_STORE_MAP {0xEA000u, 0xEC000u, 0xEE000u, 0xF0000u, 0xF0000u, 0xF0000u, 0xF0000u, 0xF0000u}
#define APP_STORE_POOL
#define APP_STORE_POOL_BASE 0xF0000u
#define APP_STORE_POOL_N 12u
#define APP_STORE_POOL_OLD 0u
#define APP_STORE_POOL_OBJ0 OBJ_INSTRUMENTS0
#define APP_STORE_POOL_OBJN 5u

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_overload(void);                /* audio deadline >90%: shed one voice with a short fade */
uint32_t app_audio_cpu_pct(void);       /* measured, smoothed device render load (percent) */
uint32_t app_audio_xruns(void);          /* audio DMA deadline misses since boot */
uint32_t app_audio_overload_shed_count(void); /* FM6 voices faded after overload callbacks */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_splash(void);                  /* suppress the generic platform splash */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): octave, wave, note (-1 none), dirty, turnsN */
#define APP_HOST_EXPECT
#ifdef PLAT_HOST
#define APP_HOST_FAILS
#define APP_HOST_CMD
int app_host_fails(void);
int app_host_cmd(const char *cmd,const char *a,const char *b,const char *c,const char *d);
#endif
int app_expect(const char *what, int *got);
