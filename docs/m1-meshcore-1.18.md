# Moorbock M1 Fieldmonitor on MeshCore Companion 1.18.0

Fieldtest version: 1.1.11; client identifier: `MBK FT1.1.11 v1.18` (18 characters).

Port prepared on 2026-10-17. Target: `ThinkNode_M1_companion_radio_ble`.

Upstream: `companion-v1.18.0`, commit `81dbae592ffad40b4212bc0b37e82b38da08f4bc`.
Fallback: `m1-custom-mc-1.17`, commit `ba37dc08409beb34862de82b1e2c3108350e8639`.

## Integration

The companion core now uses MeshCore 1.18 listeners rather than a direct UI pointer.
The custom message preview still retains contact keys, channel indices, mentions,
request scope and the decoded hop count for replies, without switching away from
the current screen on an incoming message. CLI frames do not create previews.
Queue updates reset preview history only when the queue is drained.

Custom monitor screens, both M1 buttons, position tracking, quick messages,
SHOWPATHS and keepalive controls remain in the BLE companion build. The 1.18
Companion CLI, discover UI, telemetry and nRF52 fixes are integrated. Existing
serial qm.*, fav.list and pos.show commands are retained; standard serial commands
now delegate to the upstream companion command handler.

The keepalive ACK state/counters coexist with upstream ACK listeners. Transport
code zero is treated as unscoped. A KEEPALIVE reply that has no known direct route
uses the default scope for flooding; KEEPALIVEC mirrors a known request scope or
falls back to the default scope. Temporary send scope state is restored after use.

## Verification

- M1 PlatformIO build: SUCCESS; ARM GCC 14.2.1.
- RAM: 160136 / 235520 bytes (68.0%).
- Flash: 414052 / 712704 bytes (58.1%).
- 46 upstream native tests passed: serialization, mesh tables, routing, UTF-8, hex.
- `python3 tools/m1_listener_host_tests.py`: passed with address/undefined sanitizers.
- `python3 tools/m1_keepalive_host_tests.py`: passed with address/undefined sanitizers.
- The two host harnesses compile actual method bodies with simulated dependencies;
  they do not exercise BLE, RF reception/transmission or physical display/buttons.
- Existing WP observer tests have 4 failures and 2 errors, reproduced identically
  on the untouched fallback commit. They refer to separate simple_repeater/WP web
  features absent from this M1 branch and are not used as M1 acceptance tests.

No firmware was uploaded. Hardware smoke tests remain: BLE connection and version,
monitor pages and buttons, direct/channel message replies, SHOWPATHS, KEEPALIVE
ACK success/timeout, KEEPALIVEC scope, GPS and restart with retained preferences.

## Rebuild

```
pio run -e ThinkNode_M1_companion_radio_ble
pio run -e ThinkNode_M1_companion_radio_ble -t create_uf2
python3 tools/m1_listener_host_tests.py
python3 tools/m1_keepalive_host_tests.py
```

The fallback archive includes original DFU images and a complete source bundle.
The final fallback UF2 was generated from the final 1.17 keepalive-scope-fix DFU
image at application base 0x26000 and its payload was compared byte-for-byte.
It is an application firmware fallback, not a device settings/identity readback.

## 1.1.1: preserve the M1 screen during idle

For THINKNODE_M1 only, turnOff powers down the EPD without clearing its image or deep-sleeping the controller. Controller RAM and CRC history are retained for subsequent partial updates. Other targets retain upstream sleep behavior. This fixes the blank screen introduced by upstream 1.18 clearing the panel on idle.

## 1.1.2: refresh on incoming messages

An incoming direct or channel text message wakes an idle E-ink display even when the app is connected over BLE. The current page remains selected, its contents/unread count are refreshed, and the normal idle timer powers the panel down again while preserving the image. OLED behavior is unchanged.

## 1.1.3: prevent accumulated E-ink ghosting

The M1 now renders a complete frame and performs a real full-waveform display update at startup, after waking and after 20 changed partial updates. Forced full refresh also runs if the frame CRC is unchanged. Rendering no longer clears the physical panel before the frame is complete. Idle still retains the image. Other GxEPD targets retain their existing policy. Hardware verification is pending.

## 1.1.4: white cleaning cycle followed by differential content

After a hardware report of pale text immediately after full updates, the M1 cleanup sequence now resets the EPD controller, performs a full white clear and writes the already-rendered content with a partial update. This keeps the image-building path used before 1.1.3 while retaining periodic/wake cleaning. The exact cause of the physical contrast loss is not established by host simulation; a new hardware contrast check is required.

## 1.1.5: prevent path text wrapping
Hardware tests rejected the 1.1.3/1.1.4 full refresh changes (pale/blank display). This version restores the 1.1.2 display refresh and sleep implementation. GFX automatic text wrapping is disabled so width measurement sees the full single-line path and DisplayDriver can truncate it with ellipsis. Existing partial-refresh ghosting may remain; this change addresses line wrapping, not panel waveforms.

## 1.1.6: intermediate differential erase
1.1.5 hardware photo confirms one-line path layout, but strong previous-frame residue on path, noise, MSG and SNR. For changed M1 frames, write white to current controller RAM and refresh differentially, synchronise both controller planes to white, then display the already rendered frame differentially. No controller reset or added full refresh. MCU framebuffer remains intact. Unchanged frames and idle remain untouched. A brief white intermediate image is expected; panel outcome requires hardware verification.

## 1.1.7: repeat erase pulse before history synchronisation
Hardware feedback for 1.1.6: path improved, but old and new MSG digits still overlap. Test a second differential white erase while keeping the previous-image plane unchanged, then synchronise white and draw new content. This reinforces the same erase transition instead of a white-to-white update. No added full refresh or reset; one additional partial cycle per changed frame increases display update latency. Physical contrast/erase efficacy remains unverified until user feedback.

## 1.1.8: correct M1 panel driver
1.1.7 hardware photo still contains the entire boot splash behind the home screen. Repeated partial erases failed. Meshtastic's ThinkNode M1 configuration explicitly selects GxEPD2_154_D67, whereas MeshCore's generic fallback selects GxEPD2_150_BN. The latter uploads a custom partial LUT; the D67 driver uses the panel OTP waveforms. Select D67 only for M1; other boards retain existing selection. Remove the speculative white erase passes and controller reset. Render completed frames, full refresh at boot/wake and after ten changed partial updates, retain the last physical image at idle. One-line ellipsis remains enabled. Full refresh will briefly flash; visual outcome requires hardware verification.

Evidence: https://github.com/meshtastic/firmware/blob/master/variants/nrf52840/ELECROW-ThinkNode-M1/platformio.ini (model and ten-fast-refresh limit). Existing GxEPD2 1.6.2 D67 implementation used; no wholesale Meshtastic fork migration. Source comparison with Meshtastic fork at c7eb4c3c shows different APIs and history handling, so it was not substituted blindly.

## 1.1.9: full refresh for changed M1 frames
Hardware feedback for 1.1.8: splash disappears cleanly; first home image has strong contrast, subsequent partial updates fade. Keep D67, use full updates for changed content and explicit startup/wake refresh requests. Identical frames do not refresh. Idle retains the last image; messages still wake the display. This intentionally trades flashing and longer synchronous display updates for contrast. Library declares full_refresh_time=2600ms; actual duration is panel-dependent. Hardware outcome requires confirmation. No partial-LUT tuning or white erase sequence.

## 1.1.10: pinned Meshtastic D67 implementation
Hardware photo rejects 1.1.9 full-only contrast. The standard GxEPD2 1.6.2 D67 driver differs materially from Meshtastic's pinned M1 D67 driver: post-refresh writes target both planes in standard vs current plane only in fork; fork explicitly initialises/powers on before refresh and uses 0xf4 for full update then separate powerOff, standard uses 0xf7 with integrated power-off. Import the pinned reference as a separate local ThinkNodeM1EPD library; preserve license and command sequencing. Adapt only constructor/SPI API, class names, include, and WIDTH_VISIBLE. Keep full-only policy from 1.1.9 to isolate driver behavior; changed frames flash and block the loop during hardware update. Panel contrast still requires user confirmation; source/register tests cannot prove image quality.

## 1.1.11: restore original 1.17 display implementation
User confirms original archived 1.17 fallback is much better after flashing it for comparison. Restore GxEPDDisplay.h/.cpp from original commit ba37dc08409beb34862de82b1e2c3108350e8639, with only automatic text wrapping disabled for correct path ellipsis. Thus startup, original 150_BN driver, refresh mode, and idle behavior match the original source: no panel powerOff on idle, no full-refresh scheduling, no custom driver. Remove isolated reference driver and its test. Existing MeshCore 1.18 port and message-wake behavior retained. The restore is evidence-driven but still needs hardware confirmation under 1.18. Residual ghosting is not claimed solved.
