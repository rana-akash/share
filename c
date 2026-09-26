**The captures contradict the explanation that ongoing SCO audio buried all 825 resets.** I analyzed them on the Pi and cross-checked Reset results with `btmon`.

Times below are relative to the first Hardware Error:

| Capture | Last outgoing SCO | First Reset | Reset results |
|---|---:|---:|---|
| Direct mSBC | +19.194 ms | +85.627 ms | **825 commands, no completions**, across 27.77 minutes |
| UART run 2 | −0.602 ms | +81.554 ms | 2 commands, no completions before capture ended |
| UART run 1 | +12.438 ms | +79.628 ms | **Both resets completed successfully** |

In the long direct capture, **every Reset followed the final outgoing SCO submission**. Incoming SCO nevertheless continued for 27.78 minutes. Continued outgoing audio therefore cannot explain the prolonged reset failures. This still does not prove reset bytes reached the controller correctly.

**The successful resets in UART run 1 are an important correction to our handoff.** Their successful completions occurred at +190.471 ms and +301.672 ms, followed by controller initialization commands. The error burst did not invariably produce a persistent command failure. This does not establish that audible audio recovered.

Two additional findings:

- During UART run 2’s **855 ms incoming-SCO gap**, the host submitted **114 outgoing SCO packets**, with no packet-completion events during that interval. Agent 2’s inference that SCO credits must have been returning is unsupported.
- The two main captures omit initial controller setup, so absence of `0xFC27` cannot settle the sleep-configuration question.

**The investigation now needs to explain why run 1 accepted Reset while the other captured failures did not.** H4 desynchronization remains plausible, but the “ongoing audio blocks recovery” account does not fit the long capture.

No files or configuration changed, no resets were issued, and the gateway remains stopped. Audio payloads stayed on the Pi.


  **I found stronger evidence of audio bytes being interpreted as an HCI command, but the initial trigger remains unresolved.**

In UART run 1:

- The last outgoing SCO packet before failure starts with `01 38 AD 00`.
- At **0.507 ms after the first Hardware Error**, the controller reports **Unknown HCI Command `0xAD38`**.
- The host never submitted that opcode as a command in the capture.

Those four audio-framing bytes, interpreted as an H4 command, mean:

| Bytes | Interpretation |
|---|---|
| `01` | HCI command packet |
| `38 AD` | Opcode `0xAD38` |
| `00` | Zero parameters |

This is much stronger evidence for lost packet framing than Hardware Error `0x00` alone. The bytes are normal mSBC framing, consistent with [PipeWire’s framing implementation](https://raw.githubusercontent.com/PipeWire/pipewire/master/spa/plugins/bluez5/hfp-h2.h). **My inference is that the controller interpreted audio framing as a command.** It does not establish whether bytes were lost during transmission, mishandled during reception, or misparsed by firmware.

Other findings narrow the investigation:

- **The long failure was not completely unresponsive:** each of its 825 Reset submissions was followed by another Hardware Error event, typically **0.234 ms later**, but no Reset completion. That timing suggests a reaction to transmitted commands; it is not proof of correct delivery.
- **Software write timing provides no clear discriminator.** Delayed writes followed by closely spaced writes occurred before failure, but similar gaps occurred repeatedly beforehand. These probes still cannot measure pauses inside a packet on the physical UART.
- **A newer capture shows another failure pattern.** In `repeat-20260926-180614`, there were no Hardware Error events. Incoming SCO stopped **230.6 seconds before capture ended**, while another **30,745 outgoing SCO packets** were submitted. Its manual-stop note reports audio loss. We must not count this as a healthy 37-minute run.

**Proposed next experiment:** add temporary tracing at the PL011 transmit functions and baud/termios changes, then repeat the existing isolated mSBC test once. Record transmit counts and timing, preserve the trace at failure, and leave the gateway stopped afterward. This can test for lower-level transmit stalls or reconfiguration, although it cannot directly verify wire bytes.

May I implement that temporary diagnostic and start the test?

I need approval because your [AGENTS.md](/Users/us01/Documents/ChatGPT/Pi%20Audio%20Gateway/AGENTS.md) requires a short plan and approval before code changes. Everything so far was read-only.
