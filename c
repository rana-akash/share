# Microphone crackling: USB / Bluetooth timing investigation

Date: 2026-09-27. Updated through the approximately 10:58–10:59 priority-test rollback verification. Test timestamps below are Pi local time, IST (UTC+05:30).

## Current conclusion

The user hears intermittent crackling and robotic speech in Mac microphone recordings, while most speech remains clean and recording continues. Mac-to-headset playback is reported good.

Three controlled observations narrow the fault to operation of the combined gateway:

1. A direct headset-microphone recording on the Pi was reported crystal clear by the user.
2. Playing that same clean file into the Pi USB microphone output, with the live bridges still running and the live microphone stream muted, produced crackling and occasional robotic sound in QuickTime on the Mac.
3. Playing the same file through USB with BOTH live bridges stopped produced a clean QuickTime recording, with no crackles, as reported by the user.

**The combined Bluetooth/USB graph's timing or buffering is the leading hypothesis, not a proven root cause.** Corrupted live microphone samples are not necessary to reproduce this fault. USB can deliver this file cleanly in isolation. This does not prove all Bluetooth traffic, scheduling/load effects, USB duplex interactions, or Mac processing are irrelevant.

**The problem remains unresolved.** Removing the shared `--group pi_gateway` option was approved and applied locally and on the Pi. The user initially reported improvement, but subsequent clean-file tests still crackled, including with QuickTime monitoring verified at zero. Raising the USB microphone node's driver priority to 2100 was also approved and tested; it caused rapidly increasing Bluetooth source errors and was reverted immediately. Both live bridges were restored and verified active after that rollback.

Current retained change: shared-group removal only. USB microphone driver priority is back to its prior default (previously observed as 1000). No USB interval, buffer, codec, or UART changes were made during these microphone tests. See Tests D–G below.

## User instructions and approval boundary

- Follow the workspace `AGENTS.md`: present a short plan and obtain approval before code changes; keep changes minimal and simple; obtain approval if the plan changes.
- User approved the completed recording tests and temporary routing interruptions/restoration.
- User subsequently approved shared-group removal with “start”; it remains applied because the first live test was reported better, although not fixed. The improvement is a user observation, not a controlled proof that grouping caused the fault.
- User approved the USB microphone `priority.driver=2100` test with “do it,” including rollback if unsuccessful. That test is complete and reverted.
- Further code/configuration changes require a new short approved plan. The current request authorizes updating this handoff, not a new fix.
- Diagnostic recordings/logs were created; live microphone streams were temporarily muted and restored; services were stopped/restarted for approved tests. Node IDs changed after restarts: always inspect before acting.

## SSH access

Credentials are included here at the user's explicit request.

### Active test Pi: verified login during this session

- Host: `rpiz2`
- IP: `192.168.1.12`
- Username: `us02`
- Command: `ssh us02@192.168.1.12`
- `rpiz2.local` failed to resolve from this Mac; the IP worked.
- Key-only authentication failed; password authentication worked using the saved password from `credsrpi.txt`.
- Passwordless `sudo -n` worked for diagnostic reads of journals and serial counters.

### Other saved Pi access (not tested during these microphone tests)

- Username: `us01`
- Host: `rpiz.local`, last saved IP `192.168.1.8`.
- Original saved credential file contents:

```text
ssh us01@rpiz.local 
 ip 192.168.1.8
```

Use the active test Pi above for this handoff. Do not assume old IPs, node IDs, or process IDs remain current.

## Active system and audio configuration

- Hardware/OS history: Raspberry Pi Zero 2 W, Bookworm; see `BLUETOOTH_FAILURE_HANDOFF.md` for earlier controller failures and mini-UART workaround. That document's earlier stopped-gateway state is superseded by the live state here.
- PipeWire observed: `1.2.7`.
- Headset: OnePlus Bullets Wireless Z2, address `84:0F:2A:DE:A0:D1`.
- Codec: mSBC; source `bluez_input.84_0F_2A_DE_A0_D1.0`, sink `bluez_output.84_0F_2A_DE_A0_D1.1`.
- Bluetooth device path was verified through `3f215040.serial` (mini-UART).
- Running user service: **`pi-miniuart-usb-bullets.service`**, a transient unit. The normally named `pi-audio-gateway.service` is inactive.
- Service command: `/usr/bin/python3 /home/us02/pi-audio-gateway/scripts/gateway.py 84:0F:2A:DE:A0:D1`.
- Transient service has `Restart=no`; no special environment was shown. Working directory was the user's home.
- USB gadget remains bound and negotiated `high-speed`.
- USB gadget configfs directory: `/sys/kernel/config/usb_gadget/pi-audio-gateway/functions/uac2.usb0/`.
- Live attributes: `p_srate=48000`, `p_ssize=2`, `p_chmask=1`, `p_hs_bint=4`, `c_hs_bint=4`, `req_number=32`, `c_sync=async`, `fb_max=5`. A readable `p_sync` attribute was not present.
- ALSA card 1: `UAC2_Gadget`. Pi playback is the microphone sent to the Mac; Pi capture is audio arriving from the Mac.
- USB microphone PCM: `S16_LE`, mono, 48000 Hz, period 256 frames, buffer 4096 frames. PipeWire `api.alsa.headroom=1024`, `api.alsa.period-num=16`.
- Mac Audio MIDI Setup showed the default USB input named **Capture Inactive**, 48000 Hz, 1 channel, 16-bit integer. Default USB output was **Playback Inactive**, stereo, 48000 Hz, 16-bit. These are displayed device names; do not infer stream inactivity from them.
- Both Voice Memos and QuickTime produced the reported distortion with the live gateway. Voice Memos playback options on the currently inspected memo showed Enhance Recording off, Skip Silence off, speed 1x. This was not verification of the earlier memo's settings or macOS microphone mode.

## Test A: simultaneous direct Pi capture / Mac recording

Time: approximately 10:25:19–10:26:04.

Pi command targeted the actual headset source, with fallback/reconnection disabled:

```sh
timeout -s INT 45 pw-record \
  --target=bluez_input.84_0F_2A_DE_A0_D1.0 \
  --properties='{ node.dont-fallback = true node.dont-reconnect = true }' \
  --rate=16000 --channels=1 --format=s16 headset-direct.wav
```

- WAV is 44.69 seconds, mono, 16 kHz, signed 16-bit.
- User listened and reported the Pi file crystal clear; Mac recording distorted. The agent did not independently establish perceptual quality by listening.
- File sample peak was about -2.70 dBFS, with no full-scale clipping demonstrated.
- `pw-record` ERR count rose from 0 to 5 near startup and remained there. A corresponding xrun was logged around 10:25:21.
- Bluetooth source ERR count stayed 1 during running snapshots; USB microphone output stayed 0; Mac-to-Pi source stayed 2.
- Serial snapshots showed advancing TX/RX counts but no explicit overrun field. Do not interpret that as proof that no UART overruns occurred.
- `decode failed: -3` messages appeared before and after the direct capture interval. They do not establish the cause of the Mac-only distortion in this comparison.

Remote artifacts:
`/home/us02/pi-gateway-diagnosis/mic-compare-20260927-102519/`

Files include `headset-direct.wav`, `pw-top.txt`, `pw-before.json`, `journal.txt`, `uart-before.txt`, `uart-after.txt`, `start.txt`, `end.txt`, and `record.txt` (empty recording stderr/stdout log).

Remote pointer: `/home/us02/pi-gateway-diagnosis/mic-compare-current.txt`.
Local copied diagnostics: `/private/tmp/pi-mic-compare/mic-compare-20260927-102519/` (empty `record.txt` was not present in the copied listing).
Workspace audio copy: `diagnostics/mic-compare-20260927/headset-direct.wav`.

## Test B: clean file over USB while both live bridges run

Time: 10:38:12–10:38:57.

- Muted only the live microphone bridge's playback stream, then node 56 (`output.pw-loopback-1156`), using `wpctl set-mute 56 1`.
- Played Test A's file with `pw-play --target=pi_gateway_usb_microphone`, volume 1, fallback/reconnection disabled.
- Both loopbacks remained active; muting did not remove their clock/scheduling relationships.
- Restored node 56 to unmuted; verified volume 1.00.
- User reported most of the QuickTime recording clean but intermittent crackles and robotic sound.

Diagnostics:

- USB microphone output and `pw-play`: ERR remained 0.
- Bluetooth source: ERR remained 1.
- Mac-to-Pi USB source: ERR rose from 2 to 23.
- Repeated PipeWire messages for **`hw:1c` (Mac-to-Pi capture, opposite to the microphone direction)**:
  `follower delay:4068 target:2048 thr:2048 resample:0, resync`.
  Other reported delays were 4080, 4075, 4081, 4088, 4095, with suppressed repetitions.
- Additional Bluetooth decoder errors occurred during this test, but the transmitted speech came from the clean file and live mic audio was muted.
- Live inspection showed BOTH USB nodes using `node.driver-id=39`, the Bluetooth microphone source.

Artifacts: `/home/us02/pi-gateway-diagnosis/usb-clean-20260927-103811/`.
Pointer: `/home/us02/pi-gateway-diagnosis/usb-clean-current.txt`.
Files: `start.txt`, `end.txt`, `pw-top.txt`, `play.txt`.
Related logs are in the system journal for the test interval.

## Test C: USB-only clean file, both live bridges stopped

Artifacts: `/home/us02/pi-gateway-diagnosis/usb-only-20260927-104239/`.
Pointer: `/home/us02/pi-gateway-diagnosis/usb-only-current.txt`.
Exact start/end timestamps are in `start.txt` / `end.txt`; playback lasted approximately 45 seconds.

- Stopped `pi-miniuart-usb-bullets.service`, removing both live loopbacks.
- Left USB gadget, PipeWire, and WirePlumber running. Did not explicitly disconnect the headset or change UART configuration.
- Played the SAME clean file to `pi_gateway_usb_microphone` with fallback/reconnection disabled and volume 1.
- User recorded in QuickTime and reported **good audio, no crackles**.
- During active playback, USB microphone node 68 was the graph driver, quantum 2048 at 48000 Hz. `pw-play` followed it.
- USB microphone and `pw-play` ERR counts stayed 0.
- Other audio nodes were idle in the inspected snapshots. This differs from the combined graph's 1024/48000 quantum as well as its driver and workload; do not claim driver selection alone was isolated.
- Stopping the transient service removed its unit. Restoration therefore used the approved fallback:

```sh
systemd-run --user --unit=pi-miniuart-usb-bullets \
  /usr/bin/python3 /home/us02/pi-audio-gateway/scripts/gateway.py \
  84:0F:2A:DE:A0:D1
```

Restoration was verified: service active, Mac audio → headset and headset mic → USB links both active. Latest loopback PIDs observed were 3195 and 3196; these are historical, not stable identifiers. The normally named `pi-audio-gateway.service` was not started.

## Test D: shared scheduling group removed — retained

Approved with “start.” Before the change, both loopbacks were launched with:

```python
"pw-loopback", "--group", "pi_gateway", "--channels", str(channels),
```

PipeWire documents that nodes in the same `node.group` always use the same driver:
https://pipewire.pages.freedesktop.org/pipewire/group__pw__keys.html

The only new code change removed `"--group", "pi_gateway",` from that command, in both workspace `scripts/gateway.py` and `/home/us02/pi-audio-gateway/scripts/gateway.py`. The pre-existing `-m` argument was preserved. Python syntax was checked, the gateway restarted, and both routes verified active.

- Before-change SHA-256 on both copies: `d47b5e5262609198a6b4e05bee6ba5df43b7b6bc69b376a0d4046d863b26ecdc`.
- After-change SHA-256 on both copies: `414874c50624fdc0ce0e8c8a93cdf30fd1896933b1eb78de2b5e338e9ca9c5be`.
- Pi backup: `/home/us02/pi-gateway-diagnosis/gateway-before-ungroup-20260927.py`.
- New graph: Mac-to-Pi USB source drove the headset playback route; Bluetooth microphone source still drove the USB microphone route. Both used quantum 1024 at 48000 Hz. Removing the shared group separated the two directions, but did NOT make the USB microphone its own timing source.
- User tested live recording and reported “still crackles although better.” Playback quality was not separately confirmed in that reply.
- Three-minute timing monitor started at 10:47:43: `/home/us02/pi-gateway-diagnosis/ungroup-test-20260927-104743/`, pointer `ungroup-current.txt`, transient unit `pi-ungroup-monitor.service`.
- Mac-to-Pi USB ERR stayed 29; USB microphone and loopback ERR stayed 0. Bluetooth source running snapshots settled at 3. The first snapshot displayed 0 while timing values were uninitialized; do not count that apparent 0→3 as three newly observed errors during the sample.
- Journal after 10:47:43 continued to show `decode failed: -3`; the inspected output did not show further USB resynchronizations.

## Test E: clean file with separated bridge groups

Artifacts: `/home/us02/pi-gateway-diagnosis/ungroup-clean-20260927-105256/`; pointer `ungroup-clean-current.txt`.

- Both updated bridges ran. Only live mic playback stream 45 (`output.pw-loopback-3447`) was muted; the clean Test A file was sent to `pi_gateway_usb_microphone`, then the live stream restored and verified unmuted at volume 1.00.
- User reported “crackles a lot” with occasional robotic sound.
- User then disclosed that QuickTime monitoring/playback volume had been set to 100%, so the input was also being heard while recording. This adds a return playback path if Mac output targets the gateway. It was a potential confound, not proof of feedback or the fault's cause; live microphone audio was muted during the clean-file test.
- `pw-play` ERR remained 0; Bluetooth source ERR remained 3; Mac-to-Pi source ERR remained 29. This test's printed summary filtered out follower rows with quantum zero, so do not use that summary alone to claim every follower was error-free. Full `pw-top.txt` is retained.
- Decoder errors continued, but live microphone samples were not the speech source.
- Files: `start.txt`, `end.txt`, `pw-top.txt`, `play.txt`, `journal.txt`.

## Test F: same clean-file test with QuickTime monitoring off

Artifacts: `/home/us02/pi-gateway-diagnosis/no-monitor-clean-20260927-105530/`; pointer `no-monitor-clean-current.txt`.

- Same routing and file as Test E; live mic stream temporarily muted and automatically restored.
- QuickTime was independently inspected through the UI during recording: recording active, volume slider **0**. No UI setting was changed by the agent.
- User reported “crackles now too.” This reproduces the clean-file fault without QuickTime monitoring. Monitoring is therefore not necessary for this symptom.
- USB microphone, player, and loopback ERR counters stayed 0; Mac-to-Pi source stayed 29. Bluetooth's initialized running count stayed 3; an initial zero with uninitialized timing was again present.
- Files: `start.txt`, `end.txt`, `pw-top.txt`, `play.txt`, `journal.txt`.
- Zero PipeWire errors do not prove that the samples reaching the Mac were intact. The audible result remains a user listening observation.

## Test G: USB microphone timing priority 2100 — failed and reverted

Approved plan: make USB the microphone route's timing source by raising its driver priority above Bluetooth's 2010, restart services, test, and revert if ineffective.

Changed only the USB microphone sink rule in the active Bookworm/WirePlumber 0.4 configuration:

`/home/us02/.config/wireplumber/main.lua.d/51-pi-audio-gateway.lua`

Added temporarily:

```lua
["priority.driver"] = 2100,
```

Backup: `/home/us02/pi-gateway-diagnosis/usb-rule-before-priority2100-20260927.lua`.

- Restarted `pipewire.service`, `wireplumber.service`, and `pi-miniuart-usb-bullets.service` as user services.
- Verified USB microphone node 42 became the driver at 1024/48000. Bluetooth microphone node 49 became its follower.
- Within several seconds the Bluetooth source ERR counter advanced **22 → 68 → 115 → 162**, approximately 47 errors per second. USB microphone and loopback counters remained 0 in these snapshots.
- This is an objective regression in the live route. The test was aborted before requesting another QuickTime clean-file recording; no user listening result exists for this setting.
- Restored the backed-up Lua file and restarted the same services at about 10:58:03.
- Bluetooth nodes were initially absent while reconnecting. A subsequent check verified the headset connected, both loopbacks present, and all four endpoint links active. Do not mistake the first post-restart snapshot for the final state.
- Final verified live loopback PIDs were 4266 (Mac→headset) and 4267 (headset→Mac), with USB microphone node 42, Mac audio source 43, Bluetooth source 49, Bluetooth sink 50, and microphone playback stream 58. These are historical identifiers only.
- Priority 2100 is **not active** and was not added to the repository's WirePlumber 0.5 configuration. Shared-group removal remains active.

PipeWire documents highest `priority.driver` selection and normally prefers capture sources so adaptive resampling happens on sinks:
https://docs.pipewire.org/1.2/page_man_pipewire-props_7.html

Interpretation: reversing the timing driver alone is not a viable live-microphone fix in the tested setup. It does not establish the exact clock, resampler, buffer, or driver defect. Do not describe this test as a successful fix or as a completed clean-file comparison.

## Remaining investigation

- No new fix is approved or selected. Keep subsequent experiments minimal and change one factor at a time.
- A useful next diagnostic would compare the samples immediately before the USB hardware output with the Mac recording during the same clean-file test. Neither a sink-monitor recording nor an aligned waveform comparison has yet been made.
- The USB-only success also changed graph quantum (2048 versus 1024), active routes, and workload. These remain confounding variables; driver selection alone has not been established as the cause.
- Bluetooth decoder errors are a separate unresolved observation. They cannot explain clean-file corruption through live mic samples when that stream is muted, but Bluetooth scheduling/traffic can still affect the combined system.
- Preserve the mini-UART workaround and existing USB attributes unless a specific new test is approved. Do not add controller reset/recovery behavior as a substitute for diagnosis.

## Workspace state and limitations

Workspace: `/Users/us01/Documents/ChatGPT/Pi Audio Gateway`.

Before the initial handoff, `git status` already showed modifications in `BLUETOOTH_FAILURE_HANDOFF.md` and `scripts/gateway.py`, plus untracked `diagnostics/`. The pre-existing gateway diff changes `--channel-map` to `-m`; preserve it. The current additional gateway diff removes `--group pi_gateway`. No other gateway code was changed in these tests.

Mac recordings have not been copied into the workspace or waveform-compared with the reference. Quality conclusions above are user listening reports. QuickTime capture was observed running, but the disabled microphone menu did not expose a selected-item marker in the accessibility text; system default USB input was verified separately. QuickTime monitoring at zero WAS verified during Test F. macOS microphone mode was not verified.

Avoid declaring the USB driver, Bluetooth codec, Mac hardware, or shared group definitively faulty before testing a targeted change. The established result is reproducible distortion with the combined graph and clean output in the USB-only test.
