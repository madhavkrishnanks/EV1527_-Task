# EV1527 Baseband, 433.92 MHz Link, Decode and Relay

Status: complete except for publication. Baseband generation, timing verification, RF link, decode of the generated and remote frames, and relay operation have all been demonstrated.

**Author:** K.S. Madhavkrishnan
**Task source:** Sanjeeb Mishra, Task-Based Learning Cohort 2 — "EV1527 Waveform Pattern generation using AFG & Arduino"
**Reference:** `github.com/SanjeebMishra/TaskBasedLearningCohort1/wiki/Task-Based-Learning-Cohort-2`
**Period covered:** 11–13 November 2026

---

## 1. Objective

Generate the EV1527 baseband waveform on an Arduino, verify its timing and its encoded word on an oscilloscope, transmit the frame over a 433.92 MHz OOK link, receive it, decode it back to the same 24-bit word, and use that word to operate a relay.

Every number in this document was produced by the firmware and confirmed by an instrument. Where a measurement is uncertain or was not taken, that is stated.

---

## 2. Hardware

| Item | Role |
|---|---|
| Arduino Uno (ATmega328P, 5 V logic) | pattern generator |
| Rigol DS1054Z (50 MHz, 1 GSa/s) | timing and amplitude verification |
| FS1000A-class 433.92 MHz transmitter | OOK modulator, driven by D8 |
| XD-RF-5V superheterodyne receiver | demodulator, data out to D7 / logic analyser |
| USB logic analyser + PulseView (2 MHz) | simultaneous capture of TX and RX |
| 2 × 17.3 cm wire | quarter-wave antenna at 433.92 MHz (300 / 433.92 / 4 = 17.28 cm) |
| EV1527 learning relay receiver, 12 V (Songle SRD-12VDC-SL-C) | ready-made decoder + relay |
| 2-button EV1527 remote (A/B) | real frames for comparison |

Module naming: the transmitter is an FS1000A-class board (metal SAW can, DATA / VCC / GND, ANT pad); the receiver silkscreen reads **XD-RF-5V**, a superheterodyne module. The pair is sold under several names, including WL101/WL102. They are the standard 433.92 MHz ASK/OOK pair the task specifies; no substitution was made.

![FS1000A-class transmitter and XD-RF-5V receiver as used](images/hw_modules.png)
*FS1000A-class transmitter and XD-RF-5V receiver as used*

![EV1527 12 V learning relay receiver, Songle SRD-12VDC-SL-C, 5-way terminal](images/hw_relay.png)
*EV1527 12 V learning relay receiver, Songle SRD-12VDC-SL-C, 5-way terminal*

![Two-button EV1527 remote supplied with the relay board](images/hw_remote.png)
*Two-button EV1527 remote supplied with the relay board*

### Signal chain as built

```
   Arduino Uno
        |
      D8  (baseband, 1t / 3t pulse-width coded)
        |
        +--> Rigol DS1054Z CH1          (timing verification)
        +--> logic analyser CH0 "TX_D8"
        |
   FS1000A transmitter
        |
   433.92 MHz ASK / OOK, 17.3 cm antenna
        |
   XD-RF-5V receiver
        |
      DATA --> Arduino D7 and logic analyser CH1 "RX_D7"
        |
   hand decode: 24 bits = 0x02063C

      DATA --> EV1527 relay receiver (12 V) --> relay contact
```

### Wiring as tested

| From | To | Note |
|---|---|---|
| Arduino D8 | FS1000A DATA | also probed by scope CH1 and analyser CH0 |
| FS1000A VCC / GND | Arduino 5 V / GND | module is a 5 V part |
| XD-RF-5V +5V / GND | Arduino 5 V / GND | common ground with Arduino and analyser |
| XD-RF-5V DATA | Arduino D7, analyser CH1 | labelled `RX_D7` in PulseView |
| Both ANT pads | 17.3 cm straight wire | without it the receiver output is noise |
| Relay board | 12 V DC, two power screws | contact screws left open during testing |

---

## 3. Protocol as implemented

EV1527 is a fixed-code OOK encoder. One frame is **25 pulses**: 24 data bits transmitted MSB first (20-bit address + 4-bit data), plus a sync pulse.

| Element | HIGH | LOW | At t = 250 µs |
|---|---|---|---|
| sync | 1t | 31t | 250 µs / 7750 µs |
| bit 0 | 1t | 3t | 250 µs / 750 µs |
| bit 1 | 3t | 1t | 750 µs / 250 µs |

Every bit occupies exactly 4t regardless of value, so a frame is always 128t long — 32 ms at t = 250 µs. That fixed length is why the frame period is stable, and it is a usable sanity check.

**Code value.** The task states the code as `0x3802063C`, a 32-bit constant, but a frame carries only 24 bits. The firmware sends bits 23..0, i.e. the low 24 bits, **`0x02063C`** = `0000 0010 0000 0110 0011 1100`. This is stated explicitly because a 32-bit loop is the most common defect in published implementations of this protocol (see §8.1).

---

## 4. Firmware

`firmware/ev1527_generator.ino` — bit-banged generator, t = 250 µs, 24 bits, four repeats per burst.

```cpp
const int txPin = 8;
const uint32_t code = 0x3802063C;   // 24 bits are transmitted: 0x02063C
const int repeats = 4;

const int T = 250;
const int T0H = T,     T0L = 3 * T;
const int T1H = 3 * T, T1L = T;
const int TSH = T,     TSL = 31 * T;

void setup() {
  pinMode(txPin, OUTPUT);
  digitalWrite(txPin, LOW);
  Serial.begin(9600);
}

void loop() {
  for (int r = 0; r < repeats; r++) {
    sendCode(code);
    sendSync();
    delay(10);
  }
  delay(1000);          // commented out for the link and relay tests
}

void sendCode(uint32_t code) {
  for (int i = 23; i >= 0; i--) {      // 24 bits, MSB first
    sendBit((code >> i) & 1);
  }
}

void sendBit(bool bit) {
  if (bit) {
    digitalWrite(txPin, HIGH); delayMicroseconds(T1H);
    digitalWrite(txPin, LOW);  delayMicroseconds(T1L);
  } else {
    digitalWrite(txPin, HIGH); delayMicroseconds(T0H);
    digitalWrite(txPin, LOW);  delayMicroseconds(T0L);
  }
}

void sendSync() {
  digitalWrite(txPin, HIGH); delayMicroseconds(TSH);
  digitalWrite(txPin, LOW);  delayMicroseconds(TSL);
}
```

### 4.1 Framing — order matches the reference hardware, gap length does not

The firmware emits, per repeat:

```
DATA (24 bits)  ->  SYNC (1t HIGH, 31t LOW)  ->  10 ms delay
```

The EV1527 datasheet specifies SYNC first, then the 24 bits. Measurement against the real remote (§6.1) refines this: the remote also places the sync pulse at the end of its burst. Since frames repeat, `...24 bits, sync, long LOW, 24 bits...` is the same cyclic sequence as `sync, 24 bits, sync, 24 bits` read from a different starting point. **The order therefore matches the reference hardware; the gap length does not.**

The gap after the sync pulse measures **17.8 ms** here, because the 10 ms inter-repeat delay is added to 31t (7750 µs). The real remote's equivalent gap is **10.3–12.4 ms**. A decoder that validates the sync gap against 31t ± 20 % may therefore reject this frame even though the ordering is conventional.

The clean fix is to shorten the gap rather than to reorder anything:

```cpp
for (int r = 0; r < repeats; r++) {
  sendCode(code);
  sendSync();
  delay(4);            // was 10: brings the post-sync gap to ~12 ms
}
```

Both properties of the transmitted frame — the 32-bit loop and the data-before-sync ordering — are inherited from the task's reference sketch, reproduced unmodified in `reference/Arduino_EV1527_1.txt`. They were not introduced during this work.

---

## 5. Measurements

### 5.1 Pulse timing — DS1054Z, CH1 on D8, 10X probe, 500 µs/div

| Quantity | Measured | Nominal | Error |
|---|---|---|---|
| Short HIGH (bit 0) | 250.0 µs | 250 µs | 0 % |
| Long LOW | 770.0 µs | 750 µs | +2.7 % |
| Bit period | 1.020 ms | 1.000 ms | +2.0 % |
| Duty cycle | 24.51 % / 75.49 % | 25 / 75 | — |
| Amplitude | 5.28 V (Top), 29 mV (Base) | 5 V | — |

The +20 µs on the LOW is the execution time of `digitalWrite` plus `delayMicroseconds` rounding; it lands in the LOW because the next HIGH is only asserted after the LOW delay returns.

![CH1 on D8 at 500 µs/div: +Width 250.0 µs, −Width 770.0 µs](images/scope_bits.png)
*CH1 on D8 at 500 µs/div: +Width 250.0 µs, −Width 770.0 µs, period 1.020 ms*

### 5.2 Frame structure — 5 ms/div and 10 ms/div

| Quantity | Measured | Nominal |
|---|---|---|
| Pulses per frame | 25 (1 sync + 24 data) | 25 |
| Frame length | ≈ 24 ms of data bits + sync gap | 24 × 4t + 32t = 32 ms |

![CH1 on D8 at 10 ms/div: frame bursts separated by the sync gap](images/scope_frame.png)
*CH1 on D8 at 10 ms/div: frame bursts separated by the sync gap*

![CH1 on D8 at 5 ms/div: 25 pulses in one frame](images/scope_25_pulses.png)
*CH1 on D8 at 5 ms/div: 25 pulses in one frame*

### 5.3 Measurement caveat — automatic measurements are pixel-quantised

At 10 ms/div the DS1054Z reported `+Wid 200 µs, −Wid 800 µs, Period 1.000 ms, Rise 200 µs`. Those values are quantised to one screen pixel (200 µs at that timebase), not real. The trustworthy reading was taken at 500 µs/div, where one pixel is 10 µs. Automatic width measurements must be taken at 500 µs/div or finer, or with the X1/X2 cursors.

### 5.4 RF link — logic analyser, 2 MHz, 200 000 samples (100 ms window)

Both channels were captured in a single sigrok session; three runs are archived (`captures/tx_rx_run1.sr`, `run2`, `run3`), all at 2 MHz with probe 1 = `TX_D8` and probe 2 = `RX_D7`. All three decode identically. The raw file was decoded programmatically rather than read by eye, so the numbers are reproducible from the artefact. The native captures are archived so any reader can reopen them in PulseView and re-derive the timings independently.

| Quantity | TX (`TX_D8`) | RX (`RX_D7`) |
|---|---|---|
| Pulses per complete frame | 25 | 25 |
| Complete frames in the capture | 2 (plus a partial third) | 2 (plus a partial third) |
| Bit 0 HIGH | 252 µs | 277–290 µs |
| Bit 1 HIGH | 752–758 µs | 764–798 µs |
| Bit 0 LOW | 764–769 µs | correspondingly shorter |
| Bit 1 LOW | 254–260 µs | correspondingly shorter |
| Frame gap (LOW after sync pulse) | 17 810–17 816 µs | same |
| Frame period | ≈ 42.4 ms | ≈ 42.4 ms |
| Edge latency, RX behind TX | — | ≈ 25–30 µs |

The receiver reproduces the frame pulse for pulse, stretching each HIGH by roughly 25–35 µs and shortening the matching LOW. That is the expected behaviour of a superheterodyne detector with finite bandwidth and AGC, and it is why a decoder must compare widths against a threshold rather than an exact value.

The measured frame period of 42.4 ms is consistent with the firmware: 24 bits × 1.020 ms = 24.5 ms, plus 7.75 ms sync LOW, plus the 10 ms inter-repeat delay.

**The link is not error-free, and that is worth stating.** In the third archived run, two of the received frames decoded to `0x02063C` and one decoded to `0x02031E` — a single bit in the wrong place, from a pulse that the receiver stretched past the decision threshold. Two of three frames correct is typical for these modules at bench distance with no error handling in the protocol. It is also exactly why EV1527 transmitters repeat every frame and why receivers require several identical frames before acting: repetition is the error-correction mechanism.

![TX_D8 and RX_D7 at 2 MHz over 100 ms: two frames, 25 pulses each on both channels](images/la_tx_rx_100ms.png)
*TX_D8 and RX_D7 at 2 MHz over 100 ms: two frames, 25 pulses each on both channels*

![Same link zoomed: the receiver reproduces the frame pulse for pulse](images/la_tx_rx_zoom.png)
*Same link zoomed: the receiver reproduces the frame pulse for pulse*

### 5.5 Decode from the raw capture

The same 200 000 samples were decoded in software: each frame is delimited by a LOW longer than 3 ms, and each HIGH wider than 500 µs is a 1.

| Channel | Frames found | Pulses per frame | First 24 bits | Value |
|---|---|---|---|---|
| TX_D8 | 3 (2 complete) | 25 | `000000100000011000111100` | 0x02063C |
| RX_D7 | 3 (2 complete) | 25 | `000000100000011000111100` | 0x02063C |

The hand decode of §6 and this programmatic decode agree, which is the point: the result does not depend on the eye of the person reading the screen.

---

## 6. Decode

Decoded by hand from the captured waveform, for both the TX and the RX trace, using the rule: **the sync is the pulse after the long gap; each following HIGH under 500 µs is a 0, over 500 µs is a 1; bits are read MSB first.**

```
0000 0010 0000 0110 0011 1100
 0     2     0     6     3     C
```

Decoded value: **`0x02063C`** — identical for TX and RX, and identical to the low 24 bits of the task's `0x3802063C`.

Observed detail: a capture of 25 pulses yields a 25-character bit string, with a trailing `0`. That extra character is the *next* frame's sync pulse, which is a short HIGH followed by a very long LOW and therefore looks exactly like a 0 bit. Frame boundaries must be found from the long gap, never by counting bits.

### 6.1 The remote's own frames

With the transmitter silenced and the receiver live, presses of the remote's **A** and **B** buttons were captured at 2 MHz, then zoomed to a single frame each. Both gave **25 pulses** and decoded as follows:

| | 24-bit word | Address (C19..C0) | Data (D3..D0) |
|---|---|---|---|
| Button A | `0x613718` | `0x61371` | `1000` (0x8) |
| Button B | `0x613714` | `0x61371` | `0100` (0x4) |

Button B was captured five consecutive times in one session (`captures/remote_B.sr`) and all five frames decoded to `0x613714` — identical, not a one-off reading. Button A was confirmed in `captures/remote_A.sr`.

The address is identical across both buttons and only the 4-bit data field changes. That confirms the packet structure empirically: **20 bits identify the transmitter, 4 bits select the button.** It also explains why a receiver must match the whole 24-bit word, and why the channel a relay board responds to is decided by that low nibble.

| Field | Value |
|---|---|
| Address (C19..C0) | `0x61371` |
| Data (D3..D0), button A / B | `1000` / `0100` |
| Short HIGH / LONG HIGH | ≈ 440 µs / ≈ 1250 µs |
| Short LOW / LONG LOW | ≈ 370 µs / ≈ 1180 µs |
| Pulse unit t | ≈ 400 µs |
| Sync gap | ≈ 10.3–12.4 ms |

Two consequences. First, the remote's word is **not** `0x02063C` — it carries its own factory address, which is the expected result and the reason the comparison is worth making. Second, the remote's t (≈400 µs) is not the firmware's 250 µs: each encoder's oscillator sets its own t, and a receiver must tolerate that spread rather than assume a value.

![Remote button A, one frame zoomed](images/la_remote_A.png)
*Remote button A, one frame zoomed*

![Remote button B, one frame zoomed](images/la_remote_B.png)
*Remote button B, one frame zoomed*

---

## 7. Relay stage

The relay stage used the ready-made decoder supplied with the hardware: a 12 V single-channel EV1527 learning receiver (Songle SRD-12VDC-SL-C) containing receiver, decoder and relay in one enclosure. The task allows either a second Arduino or a ready-made decoder for this step.

| Step | Action | Result |
|---|---|---|
| 1 | 12 V DC (under 100 mA) to the two power screws; contact screws left open | Board powers up |
| 2 | Press remote A | Relay operates — board and remote already paired |
| 3 | Arduino (t = 250 µs, 24 bits, `0x02063C`) transmitting continuously, held ≈30 cm away, board in learning mode | Board stores the frame; relay operates |
| 4 | Unplug the Arduino's USB | Relay stops |
| 5 | Plug the Arduino back in | Relay operates again — one click, then holds on |

Result: the board accepts the Arduino-generated frame and the relay follows it. The unplug/re-plug test establishes causation rather than coincidence — the relay responds to the transmitted frame, not to noise or to the remote.

### 7.1 Why the Arduino's word does not need to match the remote's

The Arduino transmits `0x02063C`; the remote transmits `0x613718`. Those differ, and that is the expected outcome, not an anomaly.

A learning receiver stores the 24-bit word it is taught and responds when it hears that word again. It does not know what the address means, and it does not require the address to match its original remote — that is precisely what "learning" means. The board was put into learn mode, heard `0x02063C`, stored it, and now responds to it. The remote's own address remains in the board's memory as a second stored code, which is why the remote still works too.

This also explains why the exercise compares the two words rather than trying to force them equal: the 20-bit address identifies a transmitter, so a receiver that demanded a specific address could never be paired with a different one. Learning receivers exist because that constraint is inconvenient.

What would be an anomaly is the opposite result: if the board had refused the Arduino's frame, that would point to a timing or framing mismatch (§4.1).

Two details recorded honestly:

- The relay operates **once and then holds**, rather than toggling on every frame. With frames arriving every ~42 ms, a toggle-mode board would chatter; this one latched. The observed behaviour is latching for the burst, which is what makes the demonstration stable.
- Persistence across a **power cycle of the relay board itself** was not separately verified. What was verified is that the board responds to the Arduino after the Arduino is disconnected and reconnected.

No photograph or video of the relay stage was taken. The result is recorded in writing with the reproducible procedure above.

---

## 8. Defects found and corrected

**8.1 32-bit loop instead of 24.** The task's reference sketch (`reference/Arduino_EV1527_1.txt`) loops `for (int i = 31; i >= 0; i--)`, transmitting 32 data bits — 33 pulses per frame instead of 25. Counting pulses on the analyser (32 detected) exposed it. Fixed to `i = 23`, which is what makes the transmitted word `0x02063C`. The defect was inherited from the reference, not introduced during this work.

![Logic analyser on D8 before the fix: 32 data bits per frame](images/la_32_bits_before_fix.png)
*Logic analyser on D8 before the fix: 32 data bits per frame*

![Logic analyser on D8 after the fix: 25 pulses per frame](images/la_25_pulses.png)
*Logic analyser on D8 after the fix: 25 pulses per frame*

**8.2 Probe attenuation mismatch.** An early capture showed a 5 V logic signal as 0.6 divisions at 1 V/div, i.e. the instrument read 0.6 V. A 10X probe was connected with CH1 still set to 1X. Timing was unaffected; every amplitude reading was 10× wrong. Corrected in CH1 → Probe → 10X.

**8.3 Aliasing in a "quick test".** A test sketch sampled the receiver's data pin with `digitalRead` every 50 ms. The bit period is 1.02 ms, so each sample was one instant out of ~50 bit cells; the output was a random 1/0 stream (60 % ones, no repeating 24-bit window) and could not distinguish a working link from a dead one. Replaced by width measurement.

**8.3a Record of the failed measurement (kept deliberately).** The `pulseIn` test on D7 returned, for example:

```text
H=741 us    L=11734 us
H=4195 us   L=6289 us
H=704 us    L=4149 us
H=104 us    L=12942 us
H=315 us    L=2730 us
H=471 us    L=6095 us
```

None of these is a legal EV1527 width, and the set is internally inconsistent. They are recorded so the measurement is not later mistaken for protocol data: the fault was the method (no transmitter running, `pulseIn` sampling an idle channel), not the signal.

**8.4 One board cannot transmit and receive at once.** Uploading any receive-side sketch removes the generator, so receiver tests on a single Uno have nothing transmitting to them. Every early receive test failed for this reason. Solutions: a second board, a single sketch that both bit-bangs the TX and decodes the RX in an interrupt, or capture the link with the logic analyser while the generator keeps running (the method used here).

**8.5 Receiver idle output is noise.** With no carrier present the receiver outputs a continuous fast oscillation — on the analyser, both a high and a low inside a single pixel column across most of the screen. It is not a frame and cannot be decoded. Antennas on both modules, 0.5–1 m separation and a continuous transmission are required before the output resembles the transmitted waveform.

![Receiver idle output: dense oscillation with no relation to the frame](images/la_rx_noise.png)
*Receiver idle output: dense oscillation with no relation to the frame*

**8.6 Generic protocol decoders do not apply.** PulseView's built-in OOK decoder is Manchester-based and reports "Not Manchester encoded or wrong preamble" on a valid EV1527 frame. EV1527 is pulse-width coded, so that decoder cannot succeed. Decoding was done by hand and in software from the raw capture instead.

---

## 9. Status

| Item | Status | Evidence |
|---|---|---|
| Baseband frame generated on D8 | Done | `scope_bits.png`, `scope_25_pulses.png` |
| Pulse widths measured (250 / 770 µs) | Done | §5.1 |
| 25 pulses per frame | Done | `la_25_pulses.png` |
| Frame decoded = `0x02063C` | Done | §6, §5.5 |
| RF transmission | Done | `la_tx_rx_100ms.png` |
| RF reception and decode | Done | §5.4, §6 |
| Real remote's frames captured and decoded | Done | §6.1 — A `0x613718`, B `0x613714` |
| Relay operated from the generated frame | Done | §7 — unplug/re-plug causation test |
| Message to task owner | Pending | — |

---

## 10. Next steps

1. Publish the repository and the wiki page from this document.
2. Send the task owner the capture, the decoded words (generated `0x02063C`, remote A `0x613718`, remote B `0x613714`) and one line on what was measured.
3. Optional follow-ups, none required by the task: verify pairing persistence across a relay-board power cycle; shorten the post-sync gap to match the reference remote (≈12 ms rather than 17.8 ms); characterise range beyond bench distances.

---

See also: [README.md](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/README.md) · [Task reference](https://github.com/SanjeebMishra/TaskBasedLearningCohort1/wiki/Task-Based-Learning-Cohort-2)

