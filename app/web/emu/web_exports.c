/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab browser emulator exports.
 *
 * The host simulator's JS worklet reads these explicit entry points to show
 * the FM1 audio-budget model. Keep these wrappers in the web-only link so
 * device firmware exports and code size are unaffected.
 */
#include <stdint.h>

uint32_t app_audio_cpu_pct(void);
uint32_t app_audio_xruns(void);
uint32_t app_audio_core0_pct(void);
uint32_t app_audio_core2_pct(void);

#define WEB_EXPORT __attribute__((used, visibility("default")))

WEB_EXPORT uint32_t web_app_audio_cpu_pct(void) { return app_audio_cpu_pct(); }
WEB_EXPORT uint32_t web_app_audio_xruns(void) { return app_audio_xruns(); }
WEB_EXPORT uint32_t web_app_audio_core0_pct(void) { return app_audio_core0_pct(); }
WEB_EXPORT uint32_t web_app_audio_core2_pct(void) { return app_audio_core2_pct(); }
