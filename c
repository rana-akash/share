# Pi Audio Gateway: Bluetooth controller failure investigation

Prepared and updated 2026-09-26, including the evening two-Pi comparison. All wall-clock times below are IST (UTC+05:30), unless explicitly marked otherwise.

This is a self-contained handoff for a new investigating agent. Relevant implementation and diagnostic scripts are included below. **This copy contains SSH credentials at the user’s request.** Raw Bluetooth captures remain on their respective Pis; this document contains their measured results, not audio payloads.

## 1. Objective and current conclusion

The user wants a reliable USB-to-Bluetooth audio gateway with simultaneous headset playback and microphone capture. They want the **root cause investigated**, not an automatic controller-reset/recovery script.

**The trigger/root cause has NOT been established, but Bluetooth controller firmware core dumps have now been captured on two separate Pi Zero 2 W boards.** There are two observed failure signatures: (1) HCI Hardware Error `0x00`, usually followed by Reset timeouts, but with successful resets in run D; and (2) loss of incoming audio followed by vendor-specific `FF ... 1B 03` core-dump events, without a standard Hardware Error event or automatic Reset attempt. The second signature reproduced on Bookworm with the USB controller unbound. See sections 6F–6I for the new measurements and corrections.

Temporary kernel probes showed complete acceptance of writes by the Linux Bluetooth-to-serial and serial-to-TTY interfaces immediately before failures. This rules out observed short writes at those interfaces in the captured windows. It does **not** prove that the UART hardware transmitted every byte correctly or that the controller received/processed every byte correctly.

The two core dumps have saved program counters four bytes apart and identical saved LR/SP values. This strongly suggests related firmware failure paths, but does not identify the trigger or prove all earlier Hardware Error events share that cause. UART transport, firmware/radio coexistence, scheduling, and headset-model interoperability remain possibilities. USB activity is not necessary for the no-USB reproduction; physically defective hardware has not been diagnosed.

## 2. User preferences and boundaries

- Find the cause; do not substitute a recovery loop for diagnosis.
- Present a short plan and obtain approval before code/configuration changes. Keep changes minimal.
- The user declined a CVSD comparison and specifically requested diagnosis of the hardware error. All tests so far used mSBC.
- For tests lasting minutes, set them up on the Pi, then let the user monitor sound and report when it stops. Do not spend the conversation polling throughout a long test.
- Keep the gateway stopped during isolated tests. Do not automatically restart it at the end of future tests.
- Prevent test streams from falling back to USB audio when Bluetooth disappears.

### SSH access for the next agent

Original Pi (Trixie):

```sh
ssh us01@rpiz.local
# Last known IPv4 alternative:
ssh us01@192.168.1.8
```

Second Pi (Bookworm):

```sh
ssh us02@rpiz2.local
# IPv4 address used successfully during this investigation:
ssh us02@192.168.1.12
```

Password for both SSH accounts: `alaska09`. The same password was used when sudo requested authentication. Use it at the password prompt; avoid including it in command logs or diagnostic output.

Both Pis were reachable during the 20:29 inspection. Addresses are last-known LAN addresses, not guarantees for a later session. The original Pi was reached by hostname and the second by IPv4. Check current uptime, services, and controller state before making assumptions from the historical test states. The local workspace is `/Users/us01/Documents/ChatGPT/Pi Audio Gateway`; its `AGENTS.md` requires a short approved plan before code/configuration changes.

## 3. Hardware, OS, and software

The following table describes the original Pi, `rpiz`. Section 6H describes the second Pi, `rpiz2`.

| Item | Observed value |
|---|---|
| Board | Raspberry Pi Zero 2 W Rev 1.0 |
| OS | Debian GNU/Linux 13 (trixie), Raspberry Pi packages; `/etc/os-release` reported Debian 13.7 |
| Kernel on original Pi in A–G | `6.18.50+rpt-rpi-v8`, aarch64, package `1:6.18.50-1+rpt1`, build dated 2026-09-11 |
| Headset | OnePlus Bullets Wireless Z2 |
| Headset address | `84:0F:2A:DE:A0:D1` |
| Bluetooth controller | Onboard Broadcom; kernel identifies `BCM43430B0`, chip ID 115 |
| Controller interface | UART, Linux `hci0`; PL011 `ttyAMA1` at `0x3F201000` |
| Driver/device | `hci_uart_bcm`, serial device `serial0-0` |
| Controller firmware | `BCM43430B0 (002.001.012) build 0092` |
| Firmware string | `BCM4343B0 37.4MHz wlbga_iLNA_iTR [Baseline: 0092]` |
| Firmware requested | `brcm/BCM43430B0.raspberrypi,model-zero-2-w.hcd` |
| Firmware symlink target | `../synaptics/SYN43430B0.hcd` |
| `bluez-firmware` | `1.2-13+rpt2` |
| `firmware-brcm80211` | `1:20260519-1~bpo13+1+rpt1` |
| BlueZ | `5.82-1.1+rpt2` |
| PipeWire | `1.4.2-1+rpt3` |
| WirePlumber | `0.5.8-2` |
| Host | Mac connected to the Pi's USB audio gadget |

Both `/lib/firmware/brcm/BCM43430B0.hcd` and `/lib/firmware/synaptics/SYN43430B0.hcd` had SHA-256:

```text
338c2c6631131f516bfc7e64ef0872bd0402e1f98ef9d0c900eef0c814d90a25
```

`dpkg -V bluez-firmware` produced no differences. This checks installed files against package metadata; it does not establish that the firmware is bug-free or optimal.

`hciconfig -a` reported:

```text
Type: Primary  Bus: UART
ACL MTU: 1021:7  SCO MTU: 64:1
HCI Version: 4.2 (0x8)  Revision: 0x5c
LMP Version: 4.2 (0x8)  Subversion: 0x410c
Manufacturer: Broadcom Corporation (15)
```

Wi-Fi remained enabled throughout the completed tests. It was connected on 2472 MHz (2.4 GHz), with power saving enabled and a sampled signal around -57 dBm. Wi-Fi-off and Wi-Fi-power-save-off comparisons have **not** been performed.

### Verified two-device software and firmware comparison

Read-only checks on **both devices at approximately 20:29 IST on 2026-09-26** collected `uname -r`, `uname -m`, `/etc/os-release`, `dpkg-query -W` for the five packages below, firmware hashes/symlinks, boot firmware messages, live device-tree baud properties, Wi-Fi state, and service state. These are direct observations from two physical devices, not assumed distribution defaults. They are post-test snapshots; no packages were changed by the investigator.

| Item | Original device: `rpiz` | Second device: `rpiz2` |
|---|---|---|
| OS | Debian 13 / Trixie (`DEBIAN_VERSION_FULL=13.7`) | Debian 12 / Bookworm (user-installed Lite) |
| Architecture (`uname -m`) | `aarch64` | `aarch64` |
| Kernel (`uname -r`) | `6.18.50+rpt-rpi-v8` | `6.12.109+rpt-rpi-v8` |
| `bluez` | `5.82-1.1+rpt2` | `5.66-1+rpt2+deb12u2` |
| `bluez-firmware` | `1.2-13+rpt2` | `1.2-9+rpt4` |
| `firmware-brcm80211` | `1:20260519-1~bpo13+1+rpt1` | `1:20240709-2~bpo12+1+rpt4` |
| `pipewire` | `1.4.2-1+rpt3` | `1.2.7-1~bpo12+1+rpt5` |
| `wireplumber` | `0.5.8-2` | `0.4.13-1` |
| Bluetooth controller | BCM43430B0, chip ID 115 | BCM43430B0, chip ID 115 |
| Loaded Bluetooth firmware reported | `(002.001.012) build 0092` | `(002.001.012) build 0092` |
| Bluetooth firmware string | `BCM4343B0 37.4MHz wlbga_iLNA_iTR [Baseline: 0092]` | Same |
| Firmware requested by driver | `brcm/BCM43430B0.raspberrypi,model-zero-2-w.hcd` | Same |
| Active PL011 Bluetooth node `max-speed` | `3000000` | `3000000` |
| UART identification | `ttyAMA1`, PL011 rev2, `3f201000.serial` | Same |
| Headset model | OnePlus Bullets Wireless Z2 | Same model, separate physical unit |
| Headset address | `84:0F:2A:DE:A0:D1` | `84:0F:2A:DF:2C:60` |

**Firmware SHA-256: identical on both devices.** The exact requested command was executed on each:

```sh
sha256sum /lib/firmware/brcm/BCM43430B0*.hcd
```

Both devices returned the same two lines:

```text
338c2c6631131f516bfc7e64ef0872bd0402e1f98ef9d0c900eef0c814d90a25  /lib/firmware/brcm/BCM43430B0.hcd
338c2c6631131f516bfc7e64ef0872bd0402e1f98ef9d0c900eef0c814d90a25  /lib/firmware/brcm/BCM43430B0.raspberrypi,model-zero-2-w.hcd
```

On both, the generic file is 44,376 bytes. The board-specific path is a symlink to `../synaptics/SYN43430B0.hcd`, resolving to `/usr/lib/firmware/synaptics/SYN43430B0.hcd`. SHA-256 follows the symlink, so the board-specific firmware contents match, not merely its path or build label. Different `bluez-firmware` package versions therefore did **not** provide a Bluetooth firmware-content A/B for these tests. Hashes describe installed files at inspection; the loaded build string and requested filename agree, but controller RAM itself was not independently hashed.

Other observed commonalities and differences:

- Both kernels report Wi-Fi firmware `BCM43430/2 wl0: Mar 31 2022 17:24:51 version 9.88.4.77 (g58bc5cc) FWID 01-3b307371`. This is a matching runtime version string; Wi-Fi firmware binary/NVRAM hashes were not compared.
- Both were associated with the same access point on **2472 MHz (channel 13)**, with **Wi-Fi power save on**. Current signal samples differed: original Pi −57 dBm, second Pi −72 dBm. These are post-failure samples, not synchronized measurements at either crash; they do not establish equal RF conditions.
- The live device trees also contain a separate `230400` Bluetooth-node property under `serial@7e215040`. Do not confuse it with the active PL011 path `serial@7e201000`, whose `max-speed` is `3000000` on both. This is configured speed, not a physical baud measurement or retrospective proof of every earlier setting.
- User services `pi-audio-gateway.service` and system services `pi-audio-usb.service` were **active on both Pis**, and DWC2 was bound on both at this inspection. These current states do not change the recorded gateway-stopped tests or second-Pi USB-unbound test. No services were started/stopped during this comparison.
- Separate boards, USB cables, and physical headsets were confirmed by the user. Power-supply identity, SD-card identity/health, exact physical placement, and equality of negotiated eSCO parameters were not independently established.

**Interpretation:** closely matching core dumps reproduced on two separate devices with different kernels, BlueZ, PipeWire, and WirePlumber versions, while the installed Bluetooth firmware contents match exactly. A failure unique to the Trixie/6.18 combination cannot explain both observed core dumps. A common firmware defect or firmware/headset-model interaction deserves investigation, but the comparison does not prove causation, exclude shared host bugs, or establish equal failure rates.

## 4. Normal audio architecture

```text
Mac playback
  -> USB UAC2 gadget capture on Pi (48 kHz, S16, stereo)
  -> PipeWire pw-loopback (stereo to mono)
  -> Bluetooth HFP AG / mSBC (16 kHz, mono)
  -> headset speaker

Headset microphone
  -> Bluetooth HFP AG / mSBC source (16 kHz, mono)
  -> PipeWire pw-loopback
  -> USB UAC2 gadget playback on Pi (48 kHz, S16, mono)
  -> Mac microphone input
```

Stable PipeWire node names:

```text
pi_gateway_host_audio
pi_gateway_usb_microphone
bluez_output.84_0F_2A_DE_A0_D1.1
bluez_input.84_0F_2A_DE_A0_D1.0
```

The Mac labels the USB input “Capture Inactive.” The user watches the input-level bars there. That label alone is not proof of Bluetooth state.

The gateway polls `pw-dump` once a second, selects the available mSBC profile, and maintains two `pw-loopback` processes. It reconnects when the headset device is absent, at 15-second intervals. Its printed `True` route status means nodes/processes exist, not that sound is audibly reaching the headset.

The normal gateway already sets `node.dont-fallback=true` on its loopback streams. The fallback mistake discussed below was in the **first diagnostic test**, not in this gateway setting.

USB gadget settings from the project:

| Setting | Value |
|---|---|
| Gadget | Configfs `pi-audio-gateway`, `uac2.usb0` |
| `c_chmask`, `c_srate`, `c_ssize` | `3`, `48000`, `2` |
| `p_chmask`, `p_srate`, `p_ssize` | `1`, `48000`, `2` |
| `req_number` | `32` |
| `c_hs_bint`, `p_hs_bint` | `4`, `4` (1 ms at USB high speed) |
| USB mute/volume controls | Disabled |
| USB identity | VID `0x1d6b`, PID `0x0104`; product “Pi Audio Gateway” |
| `bmAttributes`, `MaxPower` | `0x80`, `500` |

The USB gadget remained physically attached and configured during the original-Pi isolated tests (C–G). It was explicitly stopped and its DWC2 controller unbound for the second-Pi test H. Stopping the gateway removed its audio bridges, not the USB gadget or its kernel driver. Therefore the original-Pi tests do not eliminate every possible USB/interrupt/system-load interaction. Test H establishes reproduction without the USB controller bound.

## 5. Symptoms and initial observations

- Headset playback works initially, then stops. The user initially reported roughly two minutes or less; observed runs ranged from tens of seconds to about 20 minutes.
- Microphone response at the Mac also disappears during normal gateway failure.
- Stopping the gateway and playing a WAV directly to the Bluetooth sink **after failure** also produced no headset sound.
- Unbinding/rebinding the Bluetooth driver restored operation temporarily.
- The gateway process remained alive when the controller failed. Kernel Bluetooth errors preceded lost routes and related audio errors.

Representative first-boot sequence, in monotonic seconds since boot:

```text
[111.196041] Bluetooth: hci0: hardware error 0x00
[111.200463] bluetoothd: Hands-Free Voice gateway transport endpoint not connected
[111.256067] pipewire: ALSA follower resync
[113.280125] Bluetooth: hci0: Opcode 0x0c03 failed: -110
[113.280392] Bluetooth: hci0: hardware error 0x00
[115.001901] gateway: Host -> headset: False; headset -> host: False
[115.328048] Bluetooth: hci0: Opcode 0x0c03 failed: -110
```

`0x0c03` is HCI Reset; `-110` is timeout. Linux was already trying resets itself. Our later statement that there was no “recovery loop” means no new application-level recovery loop was added; it does not mean the kernel made no recovery attempts.

Some early wall-clock log timestamps were affected by clock synchronization. Use monotonic timestamps when comparing boot-relative events.

## 6. Completed experiments

### A. Original configuration, gateway running, 3,000,000 baud

The user had manually reset the controller before the initial SSH investigation. It initially worked during inspection, then failed again without rebooting.

- Captured failure at approximately 15:39:52 IST, monotonic `1485.292` seconds.
- Trace showed 39 HCI Hardware Error events, code `0x00`, within 1.428 ms.
- Incoming SCO audio paused about 144.106 ms before the first error.
- Last outgoing SCO packet was submitted about 0.892 ms before the error.
- There were no HCI control commands immediately preceding the failure in that capture; it was ordinary SCO traffic.
- The first subsequent HCI Reset was about 64 ms after the error; it timed out.
- Controller subsequently showed `DOWN`.

The reset-to-failure interval in this run was approximately 20 minutes, so this is not exclusively a boot-time or fixed two-minute failure.

### B. Lower UART baud to 1,000,000

Approved change: back up `/boot/firmware/config.txt`, add `dtparam=krnbt_baudrate=1000000`, reboot, and repeat with the gateway running.

- Effective device-tree `max-speed` was verified as `1000000`.
- User confirmed both audio directions initially worked.
- Failed again approximately 4 minutes 38 seconds after connected gateway audio began (boot-relative hardware error at `338.100` seconds).
- Same initial burst of 39 hardware-error events and reset timeouts.
- Incoming SCO paused about 170.846 ms before the first error.
- Last outgoing SCO was about 1.346 ms before the error.
- 548 SCO receive packets were still observed within 10 seconds after the error.

**Result:** lowering baud did not resolve the issue. It does not eliminate every UART-related fault.

The boot configuration was restored from backup and rebooted. Subsequent original-Pi tests used the original 3,000,000-baud setting. The second Pi’s active PL011 device-tree `max-speed` was later checked at 20:29 and was also `3000000`; the physical baud was not measured.

### C. Direct Pi playback/capture; gateway stopped

The gateway was stopped. A continuous `pw-record` stream captured the headset mic to `/dev/null`. A loop played `/usr/share/sounds/alsa/Front_Center.wav` at volume 0.3, with a two-second pause between plays.

Speaker playback was **repeated short WAV playback**, not one continuous playback process. Microphone capture was continuous. PipeWire, WirePlumber, BlueZ, mSBC, Wi-Fi, the kernel, and the USB gadget remained present.

| Time | Event |
|---|---|
| 15:55:28 | Gateway stopped |
| 15:56:30 | Direct playback/capture test started |
| **16:12:22.275** | First captured hardware error |
| 16:39:35 | Inspection confirmed test still running and gateway still stopped |
| 16:40:09 | Investigator stopped the test; its original cleanup trap restarted the gateway |

The failure occurred **before** the gateway restarted. Gateway execution is therefore not necessary for reproduction in this setup.

Important trace result:

```text
Hardware Error code: 0x00
Incoming SCO packets after first error: 220,768
Time over which incoming SCO continued after error: 27.78 minutes
HCI Reset commands after first error: 825
HCI Reset completion events after first error: 0
```

This is evidence that the controller-to-host data path remained active while command handling/reset failed. It does not prove that these packets contained valid live microphone audio or that the headset speaker continued playing.

#### Confound discovered in this test

The first isolated-test script specified Bluetooth targets but omitted `node.dont-fallback`. After the Bluetooth nodes disappeared, PipeWire selected the remaining USB audio nodes. The live graph explicitly showed the recorder linked to the USB source instead of the headset.

The user observed regular Mac input-meter activity matching the “Front center” playback intervals even though the headset was silent. Playback falling back to the USB microphone sink explains this observation. The input bars were not evidence that the Bluetooth microphone still worked.

This mistake affects interpretation of the post-failure audio routing, but does not erase the Bluetooth hardware-error capture while the gateway was stopped. Later tests explicitly disabled fallback and automatic reconnection, and did not restart the gateway.

### D. Temporary kernel UART tracing, first run

Approved experiment: instrument the UART software interfaces, reset the controller once, repeat direct mSBC playback/capture with strict headset targets, and retain the trace at the first hardware error.

- Gateway stopped throughout.
- Original 3,000,000 baud, same kernel/firmware/mSBC.
- `node.dont-fallback=true` and `node.dont-reconnect=true` verified on the capture stream; the same properties were passed to playback.
- Bluetooth failed during setup verification, at **16:53:01.697 IST**.
- Kernel trace froze at `hci_hardware_error_evt`, monotonic `3604.564616` seconds.
- Saved trace covered about 49.94 seconds, including connection setup and the shorter audio portion.
- 3,812 matched write/return pairs at **each** of the two TX interfaces, all accepted in full.
- Of these writes, 3,750 requested 64 bytes (60-byte SCO payload + 3-byte SCO header + 1-byte H4 type).
- 29,012 matched RX entry/return pairs, all accepted in full.
- No missed probes, no ring-buffer overwrites, and no dropped trace events in this run.
- The initial 10 ms error burst contained 14 errors; error-burst count is therefore not always 39.
- Last pre-error HCI SCO submission was 2.031 ms before the first error; last pre-error received SCO packet was 1.710 ms before it.
- **Later reanalysis correction:** two outgoing SCO packets followed the error, the last at +12.438 ms. Both subsequent HCI Reset commands completed successfully; see section 6F. Do not classify this run as persistent Reset failure.

Representative final writes in the kernel trace:

```text
3604.562564  serdev_device_write_buf entry: requested=64
3604.562662  ttyport_write_buf entry: requested=64
3604.562697  ttyport_write_buf return: accepted=64
3604.562702  serdev_device_write_buf return: accepted=64

3604.562713  serdev_device_write_buf entry: requested=64
3604.562718  ttyport_write_buf entry: requested=64
3604.562792  ttyport_write_buf return: accepted=64
3604.562796  serdev_device_write_buf return: accepted=64

3604.564616  hci_hardware_error_evt
```

### E. Repeat of the same kernel-traced test

Requested by the user before trying another kernel.

- Started **16:57:35 IST**.
- Failed **16:59:49.287 IST**, approximately **2 minutes 14 seconds** later.
- Initial burst: 39 HCI Hardware Error events, code `0x00`.
- Last received SCO packet preceded the error by **855.211 ms**; last outgoing SCO submission by **0.602 ms**.
- Saved rolling kernel trace spanned about 54.73 seconds overall. Per-CPU buffers retained different starting times; the common window across all CPUs was approximately 48 seconds.
- 6,713 matched TX write/return pairs at each traced interface in the retained data; **zero size mismatches**.
- 51,145 matched RX entry/return pairs; zero size mismatches.
- Three unmatched RX returns occurred at rolling-buffer boundaries.
- No missed kprobes or reported dropped events. There **were expected ring-buffer overwrites of older history**; this run is not a full trace of the entire test.
- Gateway remained stopped. Test exited without USB fallback.

Tracing overhead can change scheduling and failure timing. Similar controller errors were already captured without these kernel probes, so the general failure is not unique to instrumentation. Exact failure-time comparisons should not be treated as controlled performance measurements.

### F. Reanalysis of existing captures after independent review

Read-only analysis on the original Pi counted packet types and timestamps from the btsnoop monitor records. Reset results for C, D, and E were independently checked with `btmon`. Times below are relative to the first captured Hardware Error; they are host-side timestamps, not physical UART measurements.

| Run / capture | Last outgoing SCO submission | First subsequent Reset | Reset results in capture |
|---|---:|---:|---|
| A / `pi-gateway-diagnosis-extended.btsnoop` | +6.636 ms | +64.031 ms | 2 commands; no completions |
| B / `uart-1000000.btsnoop` | +8.536 ms | +81.882 ms | 2 commands; no completions |
| C / `direct-msbc.btsnoop` | +19.194 ms | +85.627 ms | 825 commands; no completions |
| D / `uart-run-1/uart-detail.btsnoop` | +12.438 ms | +79.628 ms | 2 commands; both completed successfully |
| E / `uart-run-2/uart-detail.btsnoop` | −0.602 ms | +81.554 ms | 2 commands; no completions |

Detailed corrections and limits:

- In C, only three outgoing SCO packets followed the first error. **All 825 resets occurred after the final outgoing SCO submission.** The last Reset was +1666.060959 seconds; incoming SCO continued to +1667.016264 seconds (220,768 packets after the error). The account that sustained outgoing audio buried all 825 resets is contradicted by the capture. This does not prove the physical TX line was quiet or that Reset bytes reached the controller correctly.
- C contained 31 Hardware Error events in the first 10 ms and 888 across the entire capture. The pre-error incoming SCO gap was only 18 microseconds in this run. Do not assume a long receive gap is necessary.
- In D, Reset commands at +79.628 ms and +299.683 ms received successful Command Complete events at +190.471 ms and +301.672 ms. Controller initialization commands followed. This demonstrates command recovery, not confirmed audible recovery. The old blanket statement that every Hardware Error was followed by unanswered resets was wrong.
- D also contained an unsolicited-looking Command Complete for unknown opcode `0xAD38`, status `0x01`, immediately after the initial burst. No causal interpretation has been established.
- In E, 114 outgoing SCO packets were submitted during the 855.211 ms pre-error incoming-SCO gap. No Number Of Completed Packets entries occurred within that gap. Across C/D/E, observed completion entries were for handle 11, while SCO used handle 6. Continued SCO submissions do not establish that SCO credits were returning.
- Capture E ended +2.579569 seconds after the error; it cannot establish what happened to the second Reset after capture ended. B ended +4.136767 seconds, A +4.083560 seconds, and D +0.396540 seconds.
- All five files reported zero in their btsnoop dropped-packet fields. This is not proof of complete physical transport observation.
- C and E did not include initial controller setup. Absence of sleep-configuration opcode `0xFC27` from those captures does not establish that it was never issued before capture began.

### G. Original Pi repeat: controller core dump without Hardware Error

A new isolated mSBC playback/capture test ran on `rpiz`, with the gateway stopped and strict Bluetooth-only targets. USB remained configured and Wi-Fi remained enabled. Capture started before one manual Bluetooth driver unbind/rebind, so controller initialization was included. No kernel probes were used.

| Time (IST) | Observation |
|---|---|
| 18:06:15.771575 | Capture began |
| 18:07:25 | Audio service started |
| 18:07:26.396649 | First outgoing SCO packet |
| 18:38:38.569535 | User SSH login recorded |
| 18:39:37.233965 | Last incoming SCO packet |
| 18:39:39.231627 | First vendor core-dump event |
| 18:40:41.310952 | Last vendor core-dump event |
| About 18:43 | Investigator saved logs and stopped test/capture |

The user reported that sound had worked continuously, then stopped after opening SSH and running `htop`; the headset appeared to disconnect. Recorded incoming audio ended about **32 minutes 11 seconds after first outgoing SCO**, and **59 seconds after the SSH login**. The time `htop` was launched was not recorded. SSH adds network traffic and host processing; this is a correlation, not proof of Wi-Fi/radio causation.

- No standard HCI Hardware Error (`0x10`) event or subsequent HCI Reset command was captured around this failure.
- The controller emitted **710 vendor-specific HCI events (`0xFF`), all beginning with payload `1B 03`**. Their decoded meaning is a controller core dump (section 6I), not 710 independent errors.
- Incoming SCO ceased; outgoing SCO submissions continued for minutes afterward.
- Linux still reported `Connected: yes`, and the mSBC nodes/recorder remained present despite the user's observed headset disconnection. The Linux connection state was not evidence of a functioning link.
- A post-failure Wi-Fi sample showed 2472 MHz and −62 dBm. This was not a measurement at the exact failure instant.
- The automated watcher only watched kernel Hardware Error messages. It **did not trigger** for the vendor core dump, so the capture and audio service were stopped manually after inspection. There was no automatic recovery reset.

### H. Second Pi: Bookworm, separate headset, USB controller unbound

The user replicated the gateway on `rpiz2` and reported similar audio drops. This uses a **different physical Pi Zero 2 W, different USB cable, and different physical headset of the same OnePlus Bullets Wireless Z2 model**.

| Item | Second-Pi observation |
|---|---|
| Host / account | `rpiz2.local`, `us02` (access details in section 2) |
| OS | Debian 12 / Bookworm; user specified 64-bit Lite |
| Kernel | `6.12.109+rpt-rpi-v8` |
| PipeWire library | `1.2.7` |
| Headset address | `84:0F:2A:DF:2C:60` |
| Controller | BCM43430B0, chip ID 115 |
| Firmware reported | `(002.001.012) build 0092`; same baseline string as original Pi |
| UART | PL011 `ttyAMA1`, `3f201000.serial`; kernel logs PL011 rev2 |

A later read-only inspection at 20:29 compared firmware hashes and full package versions on both Pis: Bluetooth firmware files match exactly despite different package versions. See the section 3 comparison table. This remains an uncontrolled multi-variable comparison, not a kernel-only A/B: OS, userspace, board, headset, and USB state differ.

Before the isolated test, its kernel log already showed Hardware Error `0x00` at **19:59:27**, followed by Reset timeouts at 19:59:29 and 19:59:31, while the replicated gateway/USB setup had been present. No detailed packet capture of that initial failure was analyzed.

Approved isolated-test setup:

1. Stop `pi-audio-gateway.service` in the user session.
2. Stop system `pi-audio-usb.service`, then unbind platform device `3f980000.usb` from DWC2. Verify the driver symlink is absent and `/sys/class/udc` is empty. This removes USB controller activity, not merely the audio bridges. It does not remove the physical power cable or eliminate every power-related hypothesis.
3. Start `btmon` before one Bluetooth driver unbind/rebind; reconnect the separate headset and verify mSBC.
4. Run repeated `Front_Center.wav` playback at volume 0.3 and continuous microphone capture to `/dev/null`. Both streams use `node.dont-fallback=true` and `node.dont-reconnect=true`.

The initial audio launch at **20:07:50** exited because this `pw-record` does not accept `--raw`. A brief SCO session occurred during that attempt, with a Disconnection Complete at 20:07:57.682169 (reason `0x16`, local-host termination). The diagnostic script was adjusted to omit `--raw`. This setup episode must not be counted as the later spontaneous failure.

The corrected audio test started at **20:08:35.531260 IST**. Both mSBC nodes and both test streams were verified running, with USB still unbound. The audio service had a 3300-second limit; capture had a 3600-second limit. Neither limit explains the observed failure.

| Time (IST) | Observation |
|---|---|
| 20:08:35.531260 | Corrected test start |
| 20:10:17.775115 | Last incoming SCO packet; approximately 1 minute 42 seconds into test |
| 20:10:19.775952 | First `1B 03` core-dump event |
| 20:11:21.855064 | Last core-dump event; 710 total |
| 20:15:05.083745 | Last valid record in recovered capture; outgoing SCO still present |
| 20:16:29 | Pi reachable again; uptime approximately one minute, showing it had rebooted |

- The saved valid capture contains no HCI Hardware Error events and no Reset commands/completions following the dump onset.
- It contains 710 `1B 03` events, including a register record. The saved PC is only four bytes from the first Pi's; LR and SP match (section 6I).
- There were 38,042 outgoing SCO submissions after dump onset and no incoming SCO after that onset. Full-file SCO totals also include the failed setup attempt, so they are not pure corrected-test totals.
- When the user first reported the audio drop, SSH to the Pi timed out and its `.local` name did not resolve. The investigator could not inspect the live failure state. This does not establish that Wi-Fi failed simultaneously with Bluetooth or why the Pi became unreachable.
- On reconnection the Pi had rebooted; the investigator did not issue that reboot. USB was bound again afterward. The verified USB-free condition applies to the recorded test, not the post-reboot state.
- The capture file is 5,976,064 bytes. Sequential parsing recovered 67,116 valid records, then encountered a zero-filled invalid header at byte offset 5,973,251. The cause of the damaged tail was not established. **All 710 dump events and the register record precede that damage.** Do not describe the entire file as intact.
- No persistent previous-boot journal was available when queried. The original live controller state is lost. Post-reboot gateway state was not re-verified; do not assume it stayed stopped across reboot.

### I. Core-dump decode and cross-board comparison

The Broadcom driver in Fuchsia explicitly classifies vendor prefix `1B 03` as a core dump: `1B` is its debug-framework subevent and `03` is the dump type. InternalBlue's `StackDumpReceiver` recognizes the same prefix and decodes its register and memory records. These sources identify the dump format; they do not supply a root-cause diagnosis for this firmware build.

| Decoded register | Original Pi, G | Second Pi, H |
|---|---|---|
| Saved PC | `0x0006B0DA` | `0x0006B0D6` |
| Saved LR | `0x0002B229` | `0x0002B229` |
| Saved SP | `0x0021DF68` | `0x0021DF68` |

Both files contain 710 dump events. Register addresses were decoded using the public format, not symbolized against a matching firmware image. The PC difference of four bytes, identical LR/SP, same controller build identifier, and matching event count strongly suggest related failure paths. They do not prove an identical instruction-level cause or establish that the earlier Hardware Error failures have the same origin.

Additional original-Pi dump details:

- Every vendor event has 244 payload bytes (246 bytes including the standard HCI event header).
- Record-length byte counts: one `0x4C`, two `0x2C`, 705 `0xF0`, one `0xE8`, and one `0x40`. These values must not be treated as 710 distinct error codes.
- The register record advertises ten registers. Decoded R0–R6: `00360000`, `0021E136`, `00000002`, `00000008`, `E0000000`, `00000000`, `00000003`.
- The 707 memory records contain **163,840 bytes (160 KiB)** with declared lengths matching the available data. Their addresses form two contiguous regions: `[0x00200000, 0x00220000)` and `[0x000D0000, 0x000D8000)` (end exclusive).
- The off-the-shelf InternalBlue checksum calculation did not validate these padded records unchanged. An exploratory length-aware calculation excluding the record tag was consistent for 708/710 records. This is not a fully validated checksum decoder; do not claim all records passed integrity checks.
- Matching symbols or further firmware reverse engineering are needed to identify the saved PC/LR functions and explain the fault. No firmware was injected, patched, or deliberately crashed to obtain these dumps.

### J. Independent-review suggestions: accepted limits versus untested hypotheses

The user supplied screenshots of a consultant's analysis and subsequent corrections. These are analysis to evaluate, not authorization to run their proposed scripts. The following distinctions were checked or retained:

- H4 synchronization loss is a plausible explanation for Hardware Error `0x00`. The transport specification requires a Hardware Error on synchronization loss; it does not make that the only possible cause. Infineon's explanation says the UART packet-indicator issue is usual, not universal.
- A byte-by-byte parser walk, one error per rejected byte, and a 64-error upper bound were not established. Repeated counts of 39 do not establish a statistical distribution. Post-error TX measurements in F contradict sustained outgoing SCO as the explanation for all 825 failed resets.
- `ttyport_write_buf` directly returns the TTY write result (`uart_write` in this path). Another return-length probe at `uart_write` would largely duplicate the existing measurement. The current probes do not observe bytes leaving the hardware FIFO.
- `SCO MTU 64:1` does not prove SCO credit flow control is enabled. Linux `hci_sched_sco()` has a no-flow-control path that reschedules without Number Of Completed Packets events. Even observed completion events would not prove an incoming audio gap was exclusively over the air.
- A controller reassembly timeout triggered by a long intra-packet UART pause is an unverified firmware hypothesis. PL011 handler timestamps alone would not directly measure wire gaps; pending data, bytes written, FIFO state, and flow control affect interpretation.
- Source inspection suggests the Broadcom driver's host-controlled sleep setup depends on a successful wake IRQ request; the board source adds only a shutdown GPIO. This deprioritizes that specific mechanism, subject to running configuration. It does not exclude all controller power behavior. Complete initialization capture should be checked for `0xFC27`; that check has not yet been reported for G/H.
- The source speed quirk mentioning BCM43430 A0/A1 must not be assumed to apply to this B0 firmware. Effective DMA use, physical UART accessibility, and transient RTS/CTS behavior should not be declared settled from one source file or one pin snapshot.
- Wi-Fi load, Wi-Fi disablement, and driver removal change more than radio contention. An A2DP success would not eliminate every UART fault. Neither an A2DP soak nor controlled Wi-Fi comparisons have been performed.
- Failure times vary widely. Repeated comparable runs are needed to estimate changes in reliability; no repeat-to-failure harness or statistically controlled baseline has been built. The 1 Mbaud failure proves that setting did not prevent failure in that run, not that it cannot change the failure rate.

#### Later consultant screenshot and its limits

The screenshot supplied around 20:27 correctly emphasized obtaining actual package versions and firmware hashes; those checks are now completed in section 3. Its other statements must be reconciled with the recorded experiments:

- USB UAC2/DWC2 load was **not unchanged across all runs**: test H explicitly unbound DWC2 and still captured a closely matching core dump. Repeating an already-completed USB-unbind test is not a missing first measurement.
- The outgoing-SCO and credit analysis is **completed**, not pending (section 6F). The later core-dump signature also differs from the earlier Hardware Error/Reset-timeout signature; both are documented rather than reduced to “audio stopped.”
- Reproduction on separate units makes a fault unique to one board, headset, or cable a weaker explanation. It does not logically rule out every hardware, shared-design, power, storage, or configuration problem. SD-card/filesystem health was not measured, and distinct power sources were not confirmed.
- The two kernels rule against a cause exclusive to Trixie/6.18 for both core dumps. They do not prove that every OS/kernel contribution is excluded or that all future kernel comparisons are worthless.
- Identical headset model does not by itself verify identical negotiated eSCO parameters. A different headset model, controlled Wi-Fi activity, different RF conditions, compatible firmware comparison, or CVSD comparison remains an unperformed proposal. The user's earlier decision against CVSD was not overridden by a consultant suggestion.
- Firmware matching is now evidence, not “almost certainly identical.” Matching firmware plus similar saved execution state prioritizes investigation; it does not make the consultant's remaining-suspect list exhaustive or identify the root cause.

## 7. Other measurements and their limits

Unless explicitly updated below, these measurements refer to the original Pi before tests G/H.

- `vcgencmd get_throttled` repeatedly returned `0x0`, including after failures. Temperatures sampled around 52–54 °C. No detected undervoltage/throttling; this is not an oscilloscope measurement of the controller supply.
- `/proc/tty/driver/ttyAMA` showed PL011 TX/RX counters and `RTS|CTS|DTR`, with no reported framing/overrun counters. This is mainly evidence about host-side reception and software accounting, not proof of correct chip-side reception.
- Pin configuration while failed:

  ```text
  GPIO30: CTS0, low
  GPIO31: RTS0, low
  GPIO32: TXD0, high
  GPIO33: RXD0, high
  ```

- UART clock from debugfs: `48000000` Hz. The Bluetooth port is PL011, not the mini UART. `/dev/serial0 -> ttyS0` refers to the separate console mapping and must not be mistaken for the Bluetooth UART.
- Bluetooth serdev runtime power status: `unsupported`, active/suspended counters both 0. Parent UART runtime state: `active`, suspended time 0 during the inspected boot. No observed host UART runtime-suspend episode.
- No separate `hciuart.service`, `hciattach`, or `btattach` process was found. Kernel serdev manages this controller.
- No controller dump was available through the kernel devcoredump interface during the earlier inspection. Broadcom vendor diagnostics read `vendor_diag=N`. Later tests G/H nevertheless captured vendor core-dump events directly in btsnoop; no vendor diagnostic mode was enabled by the investigator.
- WirePlumber sometimes logged `spa.bluez5.source.sco: decode failed: -3`, including before failures. The exact meaning/cause was not established. Incoming HCI SCO packet status bits in the examined pre-failure captures were all 0; that does not guarantee successful mSBC decoding.
- Outgoing HCI SCO packet lengths in the examined captures were consistently 63 bytes, declaring 60 payload bytes. No malformed length fields were observed.
- No actual physical UART TX bytes or controller-side FIFO contents have been measured. Later tests G/H captured internal firmware core-dump data; earlier tests A–E did not.
- Wi-Fi stayed enabled; its coexistence/power-save involvement remains untested.

## 8. What the evidence supports—and does not

**Supported:**

1. The failure is below ordinary gateway route management: it reproduces during direct Pi audio while the gateway process is stopped.
2. In several earlier captures, HCI Hardware Error precedes route loss and resets time out. Run D is an exception: resets succeeded. Tests G/H instead produced vendor core dumps without standard Hardware Error events.
3. At least one failed session continued receiving SCO packets for nearly 28 minutes, so describing the entire chip as completely powered off or wholly dead is too strong.
4. Lowering UART baud from 3 Mbaud to 1 Mbaud did not fix it.
5. The traced Linux software interfaces accepted all paired writes in the captured windows; no short-write error was found there.
6. A second board and separate same-model headset reproduced a closely matching controller core dump under Bookworm with USB unbound. Neither the first physical unit nor USB activity is necessary for that signature.
7. Controller core-dump emission is established; the exact firmware fault and its trigger are not.

**Not established:**

- Physical UART byte loss, a specific PL011 bug, an H4 synchronization defect, a particular firmware parser defect, or defective hardware. The observed core dumps establish controller crash diagnostics, not which component triggered them.
- That the normal gateway is entirely irrelevant to all possible failure timings. It is simply not necessary for the reproduced failure.
- That all firmware, kernel, radio/coexistence, power, or headset interoperability causes have been excluded.
- That changing codec, replacing hardware, or updating/downgrading software will fix it.

Do not equate the generic “hardware error” label with a proven physical hardware defect. Do not equate a successful Linux write return or incremented UART TX counter with verified bytes on the wire.

## 9. State at handoff

**Latest read-only inspection, 20:29 IST:** both Pis are reachable; both gateway services and both USB gadget services are active; both DWC2 controllers are bound. These observations supersede earlier current-state assumptions below. Neither Pi was modified during that inspection. The following subsections preserve the end-of-test states so the historical experiments are not confused with later boot/service activity.

### Original Pi (`rpiz`), state at the end of G (historical)

- Original 3,000,000-baud configuration restored.
- Kernel remains `6.18.50+rpt-rpi-v8`. No kernel or firmware package was changed.
- Gateway service is **stopped/inactive**, but still enabled for normal boot.
- Isolated playback tests and trace collection services are stopped.
- Temporary kprobes and their dedicated trace instance were removed after saving each run.
- Bluetooth was left after the core-dump failure in G; Linux still reported the headset connected. No final recovery reset was performed.
- The diagnostic work did not change production gateway code or WirePlumber configuration. At this document update, the local repository already had a pre-existing modification to `scripts/gateway.py`; it was left untouched.
- Boot-config backup exists on the Pi at `/boot/firmware/config.txt.before-bt-uart-1000000`.
- A comparison with `linux-image-6.12.75+rpt-rpi-v8` was proposed, but **not approved/executed**. The Pi's package cache lists it from the official Raspberry Pi repository. `apt-get -s install` simulated adding that one package, with no upgrades/removals. This was a simulation only. A future comparison must preserve the existing boot setup and account for kernel package boot hooks/initramfs selection.

### Second Pi (`rpiz2`), state at the first post-H reconnection (historical)

- Rebooted after the failure; the failed live controller state is no longer available.
- USB controller was bound again after reboot. Gateway/USB services had been stopped for the test but their boot enablement was unchanged; do not assume the post-reboot gateway is stopped.
- Test used only transient units and diagnostic files; no boot configuration, kernel, or firmware changes were made by the investigator.
- Saved capture survives with a damaged tail, including all 710 dump events and the register record.
- No previous-boot persistent journal was available. No new test was started by the investigator after reconnecting. A later 20:29 check found the production gateway and USB services active, as recorded above.

## 10. Questions for an independent reviewer

1. Can matching BCM43430B0/build-0092 symbols identify PC `0x6B0DA` / `0x6B0D6` and LR `0x2B229`, or decode the exception record? What evidence distinguishes the trigger from the crash location?
2. Are there relevant BCM43430B0/build-0092 or Raspberry Pi PL011/serdev issues that match these specific observations? Please distinguish a matching report from a proven cause.
3. Are the core dumps G/H and earlier Hardware Error failures one mechanism or distinct failures? How does successful Reset recovery in D constrain the explanation?
4. What minimal controlled comparison separates Wi-Fi-related host load, radio coexistence, firmware, and headset-model interoperability, given reproduction on a second board with USB unbound?
5. Are the repeated bursts of 14/31/39 error events and the pre-error RX gaps meaningful, or potentially artifacts of transport/driver buffering?

Please avoid proposing only recovery scripts, indiscriminate configuration changes, or declaring the cause solved without a discriminating measurement.

### Recommended next steps for the new agent

**Prioritize the common controller firmware failure path.** Two physical setups with different kernels/userspace produced closely matching core dumps, and the installed Bluetooth firmware hashes are identical. This prioritizes firmware investigation; it does not establish whether the trigger is internal firmware logic, headset-model interoperability, Wi-Fi coexistence, or host transport behavior. No proposed experiment below has been carried out or approved merely by its inclusion here.

1. **Begin with read-only evidence review.** Check current device state, preserve the existing captures, and use the valid prefix of the second-Pi file. Review sections 3 and 6F–6J before repeating completed work. The 20:29 snapshot found gateway and USB services active on both Pis; neither is currently guaranteed to be in an isolated-test state.
2. **Identify the crash location using matching firmware information.** Look for symbols, a ROM map, or authoritative dump-format information for this BCM43430B0/build-0092 variant. Map PCs `0x6B0DA` and `0x6B0D6`, LR `0x2B229`, and decode the exception/register record. Do not silently apply another chip revision's symbol map or infer a faulting source function from address proximity alone. The dump checksum variant is not fully resolved.
3. **Research a compatible alternative Bluetooth firmware before proposing a firmware A/B.** Confirm the exact chip/board/reference-clock compatibility and provenance; a differently named package containing the same hash is not a comparison. Present the candidate, hashes, minimal substitution method, and rollback plan for approval before changing firmware. Preserve the original board-specific symlink and target. If no credible compatible alternative exists, report that limit instead of trying arbitrary `.hcd` files.
4. **If a firmware comparison is unavailable, propose a different headset model using mSBC.** Both current headsets are separate units of the same model. Retain the USB-unbound direct-audio setup and change one intended variable. Check availability with the user before planning this test. The earlier CVSD comparison was declined; do not run it without a new explicit decision.
5. **Investigate Wi-Fi as a trigger with a controlled comparison.** Both boards use the same observed Wi-Fi firmware version, AP/channel, and power-save setting. Separate network traffic from host workload as far as practical; SSH/`htop` timing alone does not isolate RF coexistence. Before proposing Wi-Fi disablement, provide a concrete way to retain or restore access because SSH currently uses Wi-Fi. Do not claim a radio-only experiment when host driver/interrupt load also changes.

For any approved repeat: capture from before controller initialization, disable audio fallback, record actual stream start time, and retain both standard Hardware Error events and vendor `1B 03` dumps. A Hardware-Error-only watcher misses the latter; allow more than the observed ~62-second dump transmission before stopping capture after a dump starts. Preserve captures before reboot and, where practical, save logs to files rather than relying on the unavailable previous-boot journal. Keep kernel/firmware/profile settings documented and compare repeated runs rather than treating one survival time as a fix.

Set up a minutes-long test, verify audio/capture, then stop interacting and let the user monitor sound and report the failure. Do not add automatic controller recovery or restart the gateway afterward. The user wants diagnosis with minimal changes, not a recovery workaround or an unattended testing framework by default.

## 11. References checked

- [Infineon/Cypress support explanation of Hardware Error code 0 and UART synchronization](https://community.infineon.com/t5/AIROC-Bluetooth/Murata-MW1-Cypress-CYW43455-bt-firmware-chrashes-when-chip-is-in-LPM-low-power/td-p/341403). This concerns a different controller, CYW43455, and supports a hypothesis about UART synchronization—not proof that this BCM43430B0 has the same cause.
- [Raspberry Pi PL011 fix: “Avoid rare write-when-full error”](https://github.com/raspberrypi/linux/commit/65aa6ec0faaa012508489886ac357cbb86cdb9a4). Historical evidence that a dropped UART TX byte can occur in this family. The inspected current `rpi-6.18.y` source already contains the final-byte FIFO-full check from this fix; do not assume this old fix is missing. The running kernel binary was not disassembled to verify its exact implementation.
- [Broadcom UART driver source](https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/bluetooth/hci_bcm.c).
- [Bluetooth serdev source](https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/bluetooth/hci_serdev.c). Its write worker submits the buffer to `serdev_device_write_buf`, advances by the accepted length, and retains a remainder if necessary.
- [Kernel kprobe-event documentation](https://docs.kernel.org/trace/kprobetrace.html).
- [BlueZ HCI command documentation identifying Reset as 0x0c03](https://github.com/bluez/bluez/blob/master/doc/bluetoothctl-hci.rst).
- [Raspberry Pi UART baud parameter documentation](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README).
- [Raspberry Pi kernel installation/alternate-kernel documentation](https://www.raspberrypi.com/documentation/computers/linux_kernel.html).

- [Fuchsia Broadcom driver](https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/src/connectivity/bluetooth/hci/vendor/broadcom/bt_hci_broadcom.cc): identifies `1B 03` as debug-framework core-dump events. Read via Gitiles source (`?format=TEXT`) when the HTML fetch failed.
- [InternalBlue HCI decoder](https://github.com/seemoo-lab/internalblue/blob/master/internalblue/hci.py): `StackDumpReceiver`, `handleEvalStackDump`, and Raspberry Pi dump notes; used as a format reference, not installed or executed against the live controller.
- [Bluetooth UART transport specification, error recovery](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host-controller-interface/uart-transport-layer.html).
- [Linux SCO scheduler](https://github.com/raspberrypi/linux/blob/rpi-6.18.y/net/bluetooth/hci_core.c): `hci_sched_sco()` with and without `HCI_SCO_FLOWCTL`.
- [TTY serdev write wrapper](https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/tty/serdev/serdev-ttyport.c) and [serial core](https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/tty/serial/serial_core.c): write return-value propagation and buffering.

Upstream branch files can change after this handoff. These were read during diagnosis; no upstream patch was applied.

## 12. Evidence files on the Pi

Original-Pi historical files under `/home/us01/pi-gateway-diagnosis/`:

| Path | Meaning |
|---|---|
| `pi-gateway-diagnosis-extended.btsnoop` | Original-baud gateway failure capture |
| `pi-gateway-diagnosis-extended.txt` | Decoded text of that capture |
| `pi-gateway-diagnosis-kernel.txt` / `pi-gateway-diagnosis-audio.txt` | Early saved logs |
| `uart-1000000.btsnoop` / `.txt` | Lower-baud failure capture |
| `uart-1000000-kernel.txt` | Lower-baud kernel log |
| `direct-msbc.btsnoop` | First direct-test capture, including post-failure SCO/reset behavior |
| `direct-msbc-kernel.txt` | Saved late kernel log from that test; earlier journal entries had rotated |
| `uart-run-1/uart-detail*` | First kernel-traced test |
| `uart-run-2/uart-detail*` | Repeat kernel-traced test |
| `pi-uart-trace-setup.py` | Temporary tracing setup script |
| `pi-uart-trace-save.sh` | Freeze/save watcher |
| `pi-uart-audio-test.sh` | Corrected headset-only playback/capture test |

Each `uart-detail*` set contains `.btsnoop`, `-trace.txt`, `-probes.txt`, `-buffer-stats.txt`, `-kernel.txt`, and `-counters.txt`.

New test evidence:

| Host | Directory | Files / purpose |
|---|---|---|
| `rpiz` | `/home/us01/pi-gateway-diagnosis/repeat-20260926-180614/` | `capture.btsnoop`, `kernel.txt`, `bluetooth.txt`, `ssh.txt`, `manual-stop.txt`, `watch.py` |
| `rpiz2` | `/home/us02/pi-gateway-diagnosis/msbc-no-usb-20260926-200649/` | `capture.btsnoop`, `kernel-setup.txt`, `audio.sh`, `audio-start.txt` |

The original Pi's `repeat-current.txt` and the second Pi's `test-current.txt` point to these respective directories. The original-Pi watcher's expected `first-error.txt` and `capture-stopped.txt` were absent because no Hardware Error triggered it. The second-Pi capture includes the failed `--raw` setup attempt and the later corrected test; use `audio-start.txt` to distinguish them. The second-Pi capture's invalid tail must be excluded during parsing.

Raw `.btsnoop` captures can contain audio payloads. They were analysed on their respective Pis and were not copied to the Mac or embedded here. This handoff does not require reviewers to obtain those files to understand the stated findings.

## 13. Relevant boot and reset details

Original-Pi boot settings during the recorded tests:

```ini
arm_64bit=1
enable_uart=1
dtoverlay=dwc2,dr_mode=peripheral
```

No `miniuart-bt` overlay was configured. The baud-test line was removed by restoring the backup. The boot command line included `console=serial0,115200 console=tty1`; the console's `serial0` mapping was separate from Bluetooth's PL011.

Manual reset that restored operation temporarily (shown for reproducibility, not as a proposed permanent solution):

```sh
sudo sh -c "printf '%s' serial0-0 > /sys/bus/serial/drivers/hci_uart_bcm/unbind"
sleep 2
sudo sh -c "printf '%s' serial0-0 > /sys/bus/serial/drivers/hci_uart_bcm/bind"
sleep 5
timeout 12 bluetoothctl power on
timeout 12 bluetoothctl connect 84:0F:2A:DE:A0:D1
```

## 14. Diagnostic implementation

These are the temporary scripts used in the last two tests. They are included as evidence of exactly what was measured, not as instructions to execute blindly on another machine. The trace setup is not idempotent if its previous probes still exist.

The scripts below describe the original tests D/E, not every subsequent capture. G used `btmon` plus a Hardware-Error-only journal watcher; H used `btmon` without that watcher. H used the separate headset address and omitted the unsupported `pw-record --raw` option. G/H had no kprobes and saved captures directly on their respective Pis.

### UART probe setup

```python
from pathlib import Path
import os

root = Path('/sys/kernel/tracing')
inst = root / 'instances/pi_uart'
inst.mkdir(exist_ok=True)
(inst / 'tracing_on').write_text('0')
(inst / 'buffer_size_kb').write_text('1024')  # Per CPU; four CPUs.
(inst / 'trace_clock').write_text('mono')
probes = [
    'p:pi_uart/tx serdev_device_write_buf port=$arg1:x64 requested=$arg3:u64',
    'r:pi_uart/tx_return serdev_device_write_buf accepted=$retval:s64',
    'p:pi_uart/tty_tx ttyport_write_buf requested=$arg3:u64',
    'r:pi_uart/tty_tx_return ttyport_write_buf accepted=$retval:s64',
    'p:pi_uart/rx hci_uart:hci_uart_receive_buf received=$arg3:u64',
    'r:pi_uart/rx_return hci_uart:hci_uart_receive_buf accepted=$retval:u64',
    'p:pi_uart/hardware_error bluetooth:hci_hardware_error_evt',
]
fd = os.open(root / 'kprobe_events', os.O_WRONLY)
try:
    for probe in probes:
        os.write(fd, (probe + '\n').encode())
finally:
    os.close(fd)
(inst / 'events/pi_uart/enable').write_text('1')
(inst / 'events/pi_uart/hardware_error/trigger').write_text('traceoff:1')
(inst / 'tracing_on').write_text('1')
```

Writes were paired with returns by task ID and interface name. Traces record lengths/results/timing, not the transmitted byte values or UART FIFO register contents. Trace freezing occurs when the kernel processes the error, not necessarily at the physical instant the fault begins.

### Freeze/save watcher

```bash
#!/bin/bash
set -eu
trace=/sys/kernel/tracing/instances/pi_uart
out=/home/us01/pi-gateway-diagnosis
end=$((SECONDS + 3600))
while [ "$(cat "$trace/tracing_on")" = 1 ] && [ "$SECONDS" -lt "$end" ]; do
    sleep 1
done
printf 0 > "$trace/tracing_on"
cat "$trace/trace" > "$out/uart-detail-trace.txt"
cat /sys/kernel/tracing/kprobe_profile > "$out/uart-detail-probes.txt"
cat "$trace"/per_cpu/cpu*/stats > "$out/uart-detail-buffer-stats.txt"
journalctl -k -b --no-pager -o short-monotonic > "$out/uart-detail-kernel.txt"
cat /proc/tty/driver/ttyAMA > "$out/uart-detail-counters.txt"
printf 0 > "$trace/events/pi_uart/enable"
systemctl stop pi-uart-btmon.service
```

### Headset-only audio test

```bash
#!/bin/bash
set -u
props='{ node.dont-fallback = true node.dont-reconnect = true }'
pw-record --raw --target=bluez_input.84_0F_2A_DE_A0_D1.0 \
    --properties="$props" --rate=16000 --channels=1 --format=s16 - >/dev/null &
recorder=$!
trap 'kill "$recorder" 2>/dev/null || true' EXIT
trap 'exit 0' TERM INT
while kill -0 "$recorder" 2>/dev/null; do
    timeout 10 pw-play --volume=0.3 --properties="$props" \
        --target=bluez_output.84_0F_2A_DE_A0_D1.1 \
        /usr/share/sounds/alsa/Front_Center.wav || break
    sleep 2
done
```

These were launched using transient systemd units:

```sh
# Gateway stopped first; controller manually reset once; mSBC nodes verified.
sudo python3 /tmp/pi-uart-trace-setup.py
sudo systemd-run --unit=pi-uart-btmon --property=RuntimeMaxSec=3600 \
    --property=StandardOutput=null /usr/bin/btmon -i hci0 \
    -w /home/us01/pi-gateway-diagnosis/uart-detail.btsnoop
sudo systemd-run --unit=pi-uart-trace-save --property=RuntimeMaxSec=3660 \
    /bin/bash /tmp/pi-uart-trace-save.sh
systemd-run --user --unit=pi-uart-audio-test --property=RuntimeMaxSec=3300 \
    /bin/bash /tmp/pi-uart-audio-test.sh
```

The setup script ran before reconnecting the headset; playback started after the mSBC nodes appeared. Diagnostic services did not reset/recover Bluetooth. A single manual controller reset preceded each test. Kernel error handling itself still attempted HCI resets after failure.

## 15. Production implementation and configuration

The following appendices preserve the implementation snapshot embedded in the original handoff so a reviewer does not need repository access. They were not refreshed from the later modified working tree. The gateway script's SHA-256 matched the installed Pi copy during inspection:

```text
13a46f091dcc80878a6d711651e5e718f66cc93efc6b0c9dd71ae2f55e4bc001
```

### Gateway Python script (`scripts/gateway.py`)

```python
#!/usr/bin/env python3
"""Connect computer audio to the headset, and the headset mic to the computer."""
import json
import signal
import subprocess
import sys
import time


def run(*command):
    # Failed commands stop the gateway; systemd starts it again.
    result = subprocess.run(command, capture_output=True, text=True, timeout=8)
    if result.returncode:
        print(result.stderr.strip() or result.stdout.strip(), flush=True)
        result.check_returncode()
    return result.stdout


def props(item):
    # PipeWire stores device and node properties inside info.props.
    return item.get("info", {}).get("props", {})


def stop(process):
    if process.poll() is not None:
        return
    process.send_signal(signal.SIGINT)
    try:
        process.wait(timeout=4)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def select_msbc(device):
    # mSBC provides headset speakers and microphone at the same time.
    params = device.get("info", {}).get("params", {})
    for profile in params.get("EnumProfile", []):
        if "mSBC" not in profile.get("description", ""):
            continue
        if profile.get("available") != "yes":
            continue
        for current in params.get("Profile", []):
            if current.get("index") == profile["index"]:
                return True
        run("wpctl", "set-profile", str(device["id"]), str(profile["index"]))
        # Changing profiles replaces the audio nodes. Read them next time.
        return False
    return False


def stream(node, channels):
    return json.dumps({
        "target.object": props(node)["node.name"],
        "node.dont-fallback": True,  # Never send audio to another device.
        "audio.channels": channels,
        "audio.position": "[ FL FR ]" if channels == 2 else "[ MONO ]",
        "resample.quality": 10,
        "channelmix.normalize": True,
    })


def bridge(previous, source, sink, channels=1):
    # Each route remembers its nodes and the pw-loopback process joining them.
    identity = None
    if source is not None and sink is not None:
        # A serial changes on reconnect, even if PipeWire reuses the node ID.
        identity = [(node["id"], props(node).get("object.serial"))
                    for node in (source, sink)]

    if previous is not None:
        old_identity, process = previous
        if identity == old_identity and process.poll() is None:
            return previous
        stop(process)

    if identity is None:
        return None  # One end of the route is disconnected.

    process = subprocess.Popen([
        "pw-loopback", "--group", "pi_gateway", "--channels", str(channels),
        "--channel-map", "[ FL FR ]" if channels == 2 else "[ MONO ]",
        "--capture-props", stream(source, channels),
        "--playback-props", stream(sink, 1),  # Both destinations are mono.
    ], stdout=subprocess.DEVNULL)
    return identity, process


def main(address):
    running = True
    next_connect = 0
    listen = microphone = None
    last_status = None

    def shutdown(signum, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    try:
        while running:
            objects = json.loads(run("pw-dump"))
            device = None
            usb = {}
            headset = {}

            # Find the USB nodes and this headset's device and mSBC nodes.
            for item in objects:
                properties = props(item)
                is_headset = properties.get("api.bluez5.address") == address
                if item["type"].endswith(":Device") and is_headset:
                    device = item
                elif item["type"].endswith(":Node"):
                    usb[properties.get("node.name")] = item
                    if is_headset and properties.get("api.bluez5.codec") == "msbc":
                        headset[properties.get("media.class")] = item

            # Keep trying a paired headset every 15 seconds while it is absent.
            if device is None and time.monotonic() >= next_connect:
                next_connect = time.monotonic() + 15
                try:
                    run("bluetoothctl", "--timeout", "3", "connect", address)
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    print("Headset unavailable; will retry.", flush=True)

            if device is None or not select_msbc(device):
                headset = {}

            # Computer stereo → headset mono; headset mic → computer mono.
            listen = bridge(listen, usb.get("pi_gateway_host_audio"),
                            headset.get("Audio/Sink"), channels=2)
            microphone = bridge(microphone, headset.get("Audio/Source"),
                                usb.get("pi_gateway_usb_microphone"))
            status = (listen is not None, microphone is not None)
            if status != last_status:
                print(f"Host → headset: {status[0]}; headset → host: {status[1]}", flush=True)
                last_status = status
            time.sleep(1)
    finally:
        # Do not leave audio processes running after the gateway stops.
        for route in (listen, microphone):
            if route is not None:
                stop(route[1])


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: gateway.py HEADSET_ADDRESS")
    main(sys.argv[1])
```

### WirePlumber configuration (`config/wireplumber/50-pi-audio-gateway.conf`)

```ini
# WirePlumber 0.5: keep Bluetooth audio available without a desktop login.
wireplumber.profiles = {
  main = {
    monitor.bluez.seat-monitoring = disabled
    monitor.bluez-midi = disabled
    hardware.video-capture = disabled
  }
}

monitor.bluez.properties = {
  # The Pi acts as the hands-free audio gateway for the headset.
  bluez5.roles = [ hfp_ag ]
  bluez5.hfphsp-backend = "native"
  bluez5.enable-msbc = true
}

# The gateway explicitly selects the mSBC duplex profile.
wireplumber.settings = {
  bluetooth.autoswitch-to-headset-profile = false
}

monitor.alsa.rules = [
  # USB capture: audio arriving from the computer.
  {
    matches = [ { alsa.card_name = "UAC2_Gadget" media.class = "Audio/Source" } ]
    actions = { update-props = {
      node.name = "pi_gateway_host_audio"
      node.description = "Host audio for gateway"
      # This source drives host playback independently of the microphone route.
      priority.driver = 1900
      # Extra USB buffering absorbs packet timing and scheduling jitter.
      api.alsa.period-size = 256
      api.alsa.headroom = 1024
      session.suspend-timeout-seconds = 0
      resample.quality = 10
    } }
  }
  # USB playback: microphone audio going back to the computer.
  {
    matches = [ { alsa.card_name = "UAC2_Gadget" media.class = "Audio/Sink" } ]
    actions = { update-props = {
      node.name = "pi_gateway_usb_microphone"
      node.description = "Microphone to USB host"
      # Absorb USB timing jitter in the microphone route.
      api.alsa.period-size = 256
      api.alsa.headroom = 1024
      session.suspend-timeout-seconds = 0
      resample.quality = 10
    } }
  }
]
```

### Gateway user service (`config/systemd/pi-audio-gateway.service`)

```ini
[Unit]
Description=Pi Audio Gateway mSBC duplex bridge
After=pipewire.service wireplumber.service
Requires=pipewire.service wireplumber.service

[Service]
Type=simple
# Set the paired headset's Bluetooth address here. %h is the user's home folder.
ExecStart=/usr/bin/python3 %h/pi-audio-gateway/scripts/gateway.py 84:0F:2A:DE:A0:D1
# Recover if an audio command fails or PipeWire restarts.
Restart=on-failure
RestartSec=3
TimeoutStopSec=15
UMask=0077

[Install]
WantedBy=default.target
```

### USB gadget system service (`config/systemd/pi-audio-usb.service`)

```ini
[Unit]
Description=Pi Audio Gateway USB audio device
After=systemd-modules-load.service
Before=sound.target

[Service]
Type=oneshot
# The kernel keeps the USB device active after the setup script exits.
RemainAfterExit=yes
ExecStart=/usr/local/sbin/pi-audio-usb start
ExecStop=/usr/local/sbin/pi-audio-usb stop
TimeoutStartSec=20

[Install]
WantedBy=multi-user.target
```

