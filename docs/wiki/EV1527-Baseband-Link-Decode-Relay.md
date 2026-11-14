# EV1527 Baseband, 433.92 MHz Link, Decode and Relay

Status: Hardware validation completed and documented.

**Author:** K.S. Madhavkrishnan  
**Task Reference:** [Task-Based Learning Cohort 2 — EV1527 Waveform Pattern Generation](https://github.com/SanjeebMishra/TaskBasedLearningCohort1/wiki/Task-Based-Learning-Cohort-2) (Mentor: Sanjeeb Mishra)  
**Period Covered:** 11–13 November 2026  

---

## 1. Objective

I implemented an EV1527 pulse-width-modulated baseband generator on an Arduino Uno to transmit a 24-bit data word over a 433.92 MHz OOK link. The goal was to verify the baseband timing with an oscilloscope and logic analyser, capture and demodulate the RF transmission, decode the received frame back to the original word, compare the timing against a commercial handheld remote, and actuate a 12 V learning relay.

Measured values are identified where applicable; calculated and nominal values are stated separately.

---

## 2. Hardware

I used the standard 433.92 MHz transmitter/receiver pair specified for the task alongside bench instruments:

| Item | Model / Specification | Function in Experiment |
|---|---|---|
| Microcontroller | Arduino Uno (ATmega328P, 16 MHz, 5 V logic) | Baseband pattern generator (GPIO D8) |
| Oscilloscope | Rigol DS1054Z (50 MHz, 1 GSa/s, 10X probe) | Pulse width, amplitude, and duty-cycle verification |
| Transmitter Module | FS1000A-class (433.92 MHz SAW oscillator, 5 V) | OOK modulator driven by pin D8 |
| Receiver Module | XD-RF-5V (433.92 MHz superregenerative, 5 V) | Demodulator with digital DATA output |
| Logic Analyser | 8-channel USB analyser (Saleae-compatible, 2 MHz) | Dual-channel digital capture of TX (D8) and RX (D7) |
| Antennas | 2 × 17.3 cm solid copper wire | Quarter-wave wire whips for 433.92 MHz ($300 / 433.92 / 4 \approx 17.28\ \text{cm}$) |
| Learning Relay | 12 V single-channel board (Songle SRD-12VDC-SL-C) | EV1527 decoder and relay actuation target |
| Handheld Remote | 2-button EV1527 key fob (Buttons A and B) | Reference commercial frame source |

<p align="center">
  <img src="images/hw_modules.png" width="400" alt="FS1000A transmitter and XD-RF-5V receiver" />
</p>
<p align="center"><em>FS1000A-class transmitter and XD-RF-5V receiver as used</em></p>

<p align="center">
  <img src="images/hw_relay.png" width="400" alt="EV1527 12 V learning relay receiver" />
</p>
<p align="center"><em>EV1527 12 V learning relay receiver (Songle SRD-12VDC-SL-C)</em></p>

<p align="center">
  <img src="images/hw_remote.png" width="400" alt="Two-button EV1527 remote" />
</p>
<p align="center"><em>Two-button EV1527 remote supplied with the relay board</em></p>

---

## 3. Wiring / Connections

### Signal Chain
```
   Arduino Uno
        │
      Pin D8 (baseband GPIO output)
        ├──────────────────────► Rigol DS1054Z CH1 (10X probe)
        ├──────────────────────► Logic Analyser CH0 ("TX_D8")
        ▼
   FS1000A Transmitter (DATA pin)
        │
      17.3 cm wire antenna ──► 433.92 MHz OOK Wireless Link
                                      │
   XD-RF-5V Receiver ◄────────────────┘
        │
      DATA pin
        ├──────────────────────► Arduino Pin D7
        ├──────────────────────► Logic Analyser CH1 ("RX_D7")
        ▼
   EV1527 Learning Relay (Internal RF / demodulator input)
        │
   Relay Contacts (NO / COM / NC)
```

### Pin Connections

| From | To | Purpose |
|---|---|---|
| Arduino D8 | FS1000A DATA | Baseband transmit line (also probed by scope CH1 and analyser CH0) |
| Arduino 5 V / GND | FS1000A VCC / GND | Transmitter module power supply |
| Arduino 5 V / GND | XD-RF-5V +5V / GND | Receiver module power supply (common ground across all devices) |
| XD-RF-5V DATA | Arduino D7 & Analyser CH1 | Receiver baseband output labelled `RX_D7` |
| Transmitter & Receiver ANT | 17.3 cm wire | Quarter-wave whip antennas |
| External 12 V DC Supply | Relay Board DC+ / DC- | Relay operating power (current draw < 100 mA) |

---

## 4. EV1527 Baseband

The EV1527 protocol encodes data using pulse-width modulation over On-Off Keying (OOK). A single frame consists of 25 pulses: 1 synchronization pulse followed by 24 data bits.

### Timing Structure

| Element | HIGH Duration | LOW Duration | Total Period | Nominal Duration ($t = 250\ \mu\text{s}$) |
|---|---|---|---|---|
| Sync Pulse | 1t | 31t | 32t | 250 µs HIGH / 7750 µs LOW |
| Bit 0 | 1t | 3t | 4t | 250 µs HIGH / 750 µs LOW |
| Bit 1 | 3t | 1t | 4t | 750 µs HIGH / 250 µs LOW |

- **Constant Bit Length:** Each data bit occupies exactly $4t$ ($1000\ \mu\text{s}$ at $t = 250\ \mu\text{s}$), regardless of whether it is a 0 or 1.
- **Active Frame Duration:** 24 data bits ($24 \times 4t = 96t$) plus sync ($32t$) equals $128t = 32.0\ \text{ms}$ (nominal).
- **Packet Structure:** The 24 bits comprise a 20-bit address field ($C_{19} \dots C_0$) and a 4-bit data field ($D_3 \dots D_0$).
- **Transmitted Code:** The task specification listed `0x3802063C`. Because EV1527 uses 24 bits, I transmitted the lower 24 bits: `0x02063C` (`0000 0010 0000 0110 0011 1100`).

---

## 5. Arduino Implementation

The firmware is located at [`firmware/ev1527_generator.ino`](firmware/ev1527_generator.ino).

### Key Implementation Details
- **Bit Transmission:** Transmitted MSB first, from bit 23 down to bit 0:
  ```cpp
  void sendCode(uint32_t code) {
    for (int i = 23; i >= 0; i--) {
      sendBit((code >> i) & 1);
    }
  }
  ```
- **Pulse Generation:** Generated using `digitalWrite()` and `delayMicroseconds()`:
  - Bit 0: HIGH for $T$ (250 µs), LOW for $3T$ (750 µs).
  - Bit 1: HIGH for $3T$ (750 µs), LOW for $T$ (250 µs).
  - Sync: HIGH for $T$ (250 µs), LOW for $31T$ (7750 µs).
- **Burst Repetition:** The transmission loop emits 4 repeats per burst:
  ```cpp
  for (int r = 0; r < repeats; r++) {
    sendCode(code);
    sendSync();
    delay(10);
  }
  ```
- **Framing Note:** In this implementation, the loop executes `sendCode()` before `sendSync()`, followed by a 10 ms delay. The resulting post-sync gap is $31t + 10\ \text{ms} = 17.75\ \text{ms}$ nominal (measured at ~17.8 ms). In comparison, the handheld remote measured an inter-frame gap of 10.3–12.4 ms. The learning relay accepts the 17.8 ms gap; decoders requiring strict $31t \pm 20\%$ validation would need the delay shortened (e.g. `delay(4)`).

---

## 6. Oscilloscope Verification

I probed pin D8 directly using a Rigol DS1054Z (50 MHz bandwidth, 1 GSa/s, probe set to 10X).

### Measured Pulse Parameters (at 500 µs/div)

| Parameter | Nominal Value | Measured Value | Difference / Notes |
|---|---|---|---|
| Bit 0 HIGH (+Width) | 250 µs | 250.0 µs | 0.0% difference |
| Bit 0 LOW (-Width) | 750 µs | 770.0 µs | +20.0 µs (+2.7%) |
| Bit Period | 1000 µs (1.000 ms) | 1020 µs (1.020 ms) | +20.0 µs (+2.0%) |
| Duty Cycle | 25.0% / 75.0% | 24.51% / 75.49% | Matches pulse widths |
| Signal Amplitude | 5.0 V | 5.28 V (Top), 29 mV (Base) | Normal 5 V CMOS logic level |

The measured +20 µs on the LOW interval is caused by AVR instruction execution overhead in `digitalWrite()` and the return overhead of `delayMicroseconds()`. Because the HIGH delay completes before the pin is pulled LOW, the loop and function overhead extends the LOW duration before the subsequent bit begins.

<p align="center">
  <img src="images/scope_bits.png" width="400" alt="Oscilloscope pulse timing measurement at 500 us/div" />
</p>
<p align="center"><em>Scope CH1 on D8 at 500 µs/div: +Width 250.0 µs, −Width 770.0 µs, period 1.020 ms</em></p>

### Frame Structure Verification

At wider horizontal timebases, the frame burst and pulse count were observed:
- At 10 ms/div, repeated frame bursts and the sync gap between frames were clearly visible.
- At 5 ms/div, counting the individual pulses within one frame confirmed exactly 25 pulses (1 sync + 24 data bits).

<p align="center">
  <img src="images/scope_frame.png" width="400" alt="Oscilloscope frame bursts at 10 ms/div" />
</p>
<p align="center"><em>Scope CH1 on D8 at 10 ms/div: frame bursts separated by sync gaps</em></p>

<p align="center">
  <img src="images/scope_25_pulses.png" width="400" alt="Oscilloscope 25 pulses at 5 ms/div" />
</p>
<p align="center"><em>Scope CH1 on D8 at 5 ms/div: 25 pulses in one frame</em></p>

---

## 7. RF Transmission and Reception

I connected pin D8 to the FS1000A transmitter DATA pin and powered both the transmitter and the XD-RF-5V receiver from the 5 V rail of the Arduino Uno. Each module had a 17.3 cm wire soldered to its antenna pad, and they were placed approximately 0.5–1 m apart on the bench.

### Observations
1. **RF Modulation:** The transmitter radiated an ASK/OOK modulated 433.92 MHz carrier whenever D8 was HIGH.
2. **Demodulated Output:** The XD-RF-5V receiver demodulated the carrier and reproduced the baseband waveform on its DATA pin.
3. **Carrier Detection:** When the transmitter was actively broadcasting, the receiver output tracked the transmitted pulses. When the transmitter was powered off, the receiver output floated into random high-frequency noise due to the internal automatic gain control (AGC).

---

## 8. Logic Analyser Capture

I connected an 8-channel USB logic analyser running PulseView at a sample rate of 2 MHz (0.5 µs resolution):
- Channel 0: Connected to Arduino pin D8 (`TX_D8`)
- Channel 1: Connected to XD-RF-5V DATA pin (`RX_D7`)

Three separate dual-channel runs were recorded over 100 ms windows (200 000 samples) and archived in `captures/tx_rx_run1.sr`, `run2.sr`, and `run3.sr`. The native capture session is also preserved in `captures/RX_TX_of_both_boards.sr`. These files can be opened directly in PulseView to verify the raw timings independently.

### Timing Comparison: Transmitter vs Receiver

| Measurement | TX (`TX_D8`) | RX (`RX_D7`) | Observed Effect |
|---|---|---|---|
| Pulses per complete frame | 25 | 25 | Frame structure preserved |
| Complete frames in 100 ms | 2 complete (+ 1 partial) | 2 complete (+ 1 partial) | Both frames captured cleanly |
| Bit 0 HIGH duration | 252 µs | 277–290 µs | RX HIGH stretched by 25–38 µs |
| Bit 1 HIGH duration | 752–758 µs | 764–798 µs | RX HIGH stretched by 12–40 µs |
| Bit 0 LOW duration | 764–769 µs | Correspondingly shorter | Total bit period conserved (~1020 µs) |
| Bit 1 LOW duration | 254–260 µs | Correspondingly shorter | Total bit period conserved (~1020 µs) |
| Post-sync LOW gap | 17 810–17 816 µs | 17 810–17 816 µs | Maintained across RF link |
| Total frame period | ~42.4 ms | ~42.4 ms | Calculated: 24.5 ms data + 7.75 ms sync + 10 ms delay |
| Edge latency | Reference (0 µs) | ~25–30 µs delay | RF demodulation and receiver filter delay |

<p align="center">
  <img src="images/la_tx_rx_100ms.png" width="400" alt="Logic analyser TX vs RX over 100 ms" />
</p>
<p align="center"><em>TX_D8 and RX_D7 at 2 MHz over 100 ms: two complete frames, 25 pulses each on both channels</em></p>

<p align="center">
  <img src="images/la_tx_rx_zoom.png" width="400" alt="Logic analyser TX vs RX zoom" />
</p>
<p align="center"><em>Zoomed view of TX_D8 and RX_D7: receiver tracks transmitter pulses with ~25–30 µs edge delay</em></p>

### Transmission Redundancy Observation
In `captures/tx_rx_run3.sr`, two of the received frames decoded cleanly to `0x02063C`, while one frame decoded to `0x02031E` due to an RF noise glitch that stretched a short HIGH pulse past the decision threshold. Because the EV1527 protocol contains no checksum or error-correcting code, repeating transmission across multiple frames increases the probability that the receiver detects a valid frame.

---

## 9. Decoding

The captured waveforms were decoded using both manual inspection and software analysis.

### Decoding Rule
1. Locate the frame boundary by identifying the long post-sync LOW gap (> 3 ms).
2. The 24 pulses immediately following the long gap are the data bits, evaluated MSB first:
   - If HIGH width < 500 µs: decode as bit `0`.
   - If HIGH width > 500 µs: decode as bit `1`.
3. The 25th pulse preceding the next long gap is the sync pulse (1t HIGH, 31t LOW); evaluated against the 500 µs threshold, its 250 µs HIGH appears as a trailing bit '0'.

### Manual Decode
Reading the 24 pulse widths from the logic analyser trace yields:
```
Binary:   0000  0010  0000  0110  0011  1100
Hex:        0     2     0     6     3     C
```
The decoded value is `0x02063C`.

*Note on 25th pulse:* Reading 25 pulses sequentially yields a 25th character (trailing 0) because the sync pulse of that frame is a 250 µs HIGH followed by a long LOW gap, which resembles a bit 0 under a 500 µs threshold. Frame boundaries must always be established by identifying the long sync gap.

### Programmatic Decode
A Python script was used to process the raw 200 000 samples from the `.sr` files:
- Found frames by splitting on LOW intervals > 3 ms.
- Evaluated HIGH widths against a 500 µs threshold.
- Both `TX_D8` and `RX_D7` channels decoded to `0x02063C` across all complete frames in runs 1 and 2.

---

## 10. Remote Comparison

To examine real-world EV1527 characteristics, I silenced the Arduino transmitter and captured transmissions from the commercial 2-button remote supplied with the relay board. Presses of Button A and Button B were captured at 2 MHz on the XD-RF-5V receiver DATA pin.

### Decoded Remote Frames

| Button | 24-bit Word | Address ($C_{19} \dots C_0$) | Data ($D_3 \dots D_0$) | Raw Capture File |
|---|---|---|---|---|
| Button A | `0x613718` | `0x61371` | `1000` (`0x8`) | `captures/remote_A.sr` |
| Button B | `0x613714` | `0x61371` | `0100` (`0x4`) | `captures/remote_B.sr` (5 identical frames) |

<p align="center">
  <img src="images/la_remote_A.png" width="400" alt="Remote Button A logic analyser capture" />
</p>
<p align="center"><em>Remote Button A, one frame zoomed: decoded 0x613718 (Address 0x61371, Data 1000)</em></p>

<p align="center">
  <img src="images/la_remote_B.png" width="400" alt="Remote Button B logic analyser capture" />
</p>
<p align="center"><em>Remote Button B, one frame zoomed: decoded 0x613714 (Address 0x61371, Data 0100)</em></p>

### Timing Differences Between Remote and Generator

| Parameter | Arduino Generator | Handheld Remote |
|---|---|---|
| Pulse unit ($t$) | 250 µs (implemented) | $\approx 400\ \mu\text{s}$ (measured) |
| Short HIGH | 250.0 µs | $\approx 440\ \mu\text{s}$ |
| Long HIGH | 752–758 µs | $\approx 1250\ \mu\text{s}$ |
| Short LOW | 254–260 µs | $\approx 370\ \mu\text{s}$ |
| Long LOW | 764–770 µs | $\approx 1180\ \mu\text{s}$ |
| Post-sync Gap | 17.8 ms | 10.3–12.4 ms |
| Address | `0x02063` | `0x61371` |

### Key Observations
1. **Address vs Data Fields:** Both remote buttons shared the same 20-bit address (`0x61371`) and differed only in the lower 4 data bits (`1000` vs `0100`).
2. **Independent Addresses:** The remote address differs from the Arduino's generated address (`0x02063`), which is expected since EV1527 addresses are set per device.
3. **Oscillator Tolerance:** The remote operated at $t \approx 400\ \mu\text{s}$, while the Arduino ran at $t = 250\ \mu\text{s}$. This demonstrates the oscillator variation inherent in low-cost resistor-set EV1527 oscillators.

---

## 11. Relay Demonstration

The relay stage was tested using the 12 V single-channel EV1527 learning receiver module (Songle SRD-12VDC-SL-C).

### Procedure and Observations

| Step | Action | Observed Result |
|---|---|---|
| 1 | Applied 12 V DC power to board DC+ / DC- terminals | Power LED illuminated; board initialised |
| 2 | Pressed Remote Button A | Relay clicked and closed contacts (factory paired) |
| 3 | Set board to learning mode (pressed onboard button); held Arduino transmitter ~30 cm away broadcasting `0x02063C` | Learning LED flashed and extinguished; relay clicked closed |
| 4 | Disconnected USB power from Arduino | Transmission ceased; relay opened |
| 5 | Reconnected USB power to Arduino | Transmission resumed; relay clicked closed and held |

The learning relay successfully learned the generated code (`0x02063C`) and responded when the generated frame was transmitted again. The board operated in momentary mode with retriggering: repeated frames arriving every ~42 ms continuously retriggered the internal dropout timer, holding the relay closed without contact chatter between frames, and opening when transmission ceased. Disconnecting and reconnecting transmitter power verified that the relay actively responds to the generated baseband transmission. Both the remote and the Arduino remained stored in the relay's memory and could actuate the board independently.

---

## 12. Debugging / Failed Experiments

During the development of this setup, several initial approaches failed before being corrected:

### 1. Frame Length Clarification
- **What was tried:** The supplied reference sketch (`reference/Arduino_EV1527_1.txt`) contained `for (int i = 31; i >= 0; i--)` to transmit `code = 0x3802063C`.
- **What was observed:** Logic analyser edge counting showed 32 data pulses (33 pulses total per frame), as shown in `la_32_bits_before_fix.png`.
- **Why it was invalid:** Standard EV1527 frames contain 24 data bits (25 pulses total).
- **What was changed:** I adapted the loop to `for (int i = 23; i >= 0; i--)`, sending the low 24 bits (`0x02063C`).
- **What worked afterwards:** The logic analyser confirmed exactly 25 pulses per frame (`la_25_pulses.png`).

<p align="center">
  <img src="images/la_32_bits_before_fix.png" width="400" alt="Logic analyser showing 32 data bits before fix" />
</p>
<p align="center"><em>Logic analyser on D8 before fix: 32 data bits per frame</em></p>

<p align="center">
  <img src="images/la_25_pulses.png" width="400" alt="Logic analyser showing 25 pulses after fix" />
</p>
<p align="center"><em>Logic analyser on D8 after fix: 25 pulses per frame</em></p>

### 2. Probe Attenuation Mismatch
- **What was tried:** Probed pin D8 with an oscilloscope probe.
- **What was observed:** Logic HIGH displayed as 0.6 V instead of 5 V.
- **Why it was invalid:** The probe hardware switch was set to 10X, but the oscilloscope channel attenuation was set to 1X.
- **What was changed:** Changed the oscilloscope CH1 menu setting to 10X.
- **What worked afterwards:** Amplitude displayed correctly as 5.28 V.

### 3. Sampling Aliasing
- **What was tried:** Tested receiver DATA with a test sketch calling `digitalRead()` every 50 ms.
- **What was observed:** Output was a random sequence of 1s and 0s (~60% ones) with no discernible pattern.
- **Why it was invalid:** The bit period is 1.02 ms. Sampling every 50 ms captured random instantaneous states across arbitrary bit positions.
- **What was changed:** Replaced discrete polling with pulse-width timing and logic analyser capture.
- **What worked afterwards:** Clean baseband waveforms were captured and decoded.

### 4. Idle Receiver Noise
- **What was tried:** Captured receiver DATA pin while no transmitter was running.
- **What was observed:** Continuous dense high-frequency switching on the logic analyser:
  ```text
  H=741 us    L=11734 us
  H=4195 us   L=6289 us
  H=704 us    L=4149 us
  ```
- **Why it was invalid:** In the absence of an RF carrier, the unsquelched receiver and adaptive data slicer amplify background thermal noise into continuous digital switching.
- **What was changed:** Added 17.3 cm antennas and powered the transmitter during all capture tests.
- **What worked afterwards:** Receiver reproduced clean frames matching the transmitter output.

<p align="center">
  <img src="images/la_rx_noise.png" width="400" alt="Receiver idle noise on logic analyser" />
</p>
<p align="center"><em>Receiver idle output: dense noise switching without carrier</em></p>

### 5. Single-Board Transmit/Receive Limitation
- **What was tried:** Attempted to test both transmission and software reception on a single Arduino Uno.
- **What was observed:** Uploading receiver sketches stopped the generator; running both in software created timing jitter.
- **What was changed:** Ran the Arduino as a dedicated transmitter and used a dedicated 2 MHz USB logic analyser to capture and decode the receiver output.
- **What worked afterwards:** Transmitter ran with consistent timing while the logic analyser captured raw traces simultaneously.

---

## 13. Results Summary

| Metric | Target / Specification | Measured / Verified Value | Method |
|---|---|---|---|
| Baseband pulse unit ($t$) | 250 µs | 250.0 µs (+Width on D8) | Rigol DS1054Z (500 µs/div) |
| Bit 0 HIGH / LOW | 250 µs / 750 µs | 250.0 µs / 770.0 µs | Rigol DS1054Z |
| Bit period | 1000 µs (1.000 ms) | 1020 µs (1.020 ms) | Rigol DS1054Z |
| Pulses per frame | 25 (1 sync + 24 data) | 25 pulses | Logic analyser edge count |
| Generated code | `0x02063C` (low 24 bits) | `0x02063C` | Logic analyser TX decode |
| Received code | `0x02063C` | `0x02063C` | Logic analyser RX decode |
| RX edge latency | — | ~25–30 µs | Dual-channel capture |
| RX HIGH stretch | — | 25–35 µs | Dual-channel capture |
| Remote Button A | 24-bit frame | `0x613718` ($t \approx 400\ \mu\text{s}$) | Logic analyser capture |
| Remote Button B | 24-bit frame | `0x613714` ($t \approx 400\ \mu\text{s}$) | Logic analyser capture |
| Relay operation | Actuation on valid code | Learned `0x02063C` and actuated | Observed physical switching |

---

## 14. Limitations

1. **Receiver Distortion:** The XD-RF-5V demodulator introduces a 25–30 µs delay and stretches HIGH pulses by 25–35 µs. Software decoders must evaluate pulse widths against a mid-point threshold (~500 µs) rather than strict nominal values.
2. **Post-Sync Gap:** The generator firmware implements a 10 ms inter-repeat delay, resulting in a post-sync gap of ~17.8 ms (compared to the handheld remote's 10.3–12.4 ms). The learning relay accepted this frame, but decoders enforcing strict 31t bounds may reject it unless the delay is reduced.
3. **Benchtop Distance:** Experiments were conducted at 0.5–1 m range. Receiver sensitivity, multipath degradation, and maximum operating distance were not characterised.
4. **RF Spectrum Characteristics:** No spectrum analyser was used. Transmitter frequency stability, spurious emissions, and RF output power were not measured.
5. **Relay Persistence:** Pairing persistence across a full power cycle of the relay board itself was not tested. Re-actuation after disconnecting and reconnecting the transmitter was confirmed.

---

## 15. Files / Evidence

| File | Description |
|---|---|
| [`README.md`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/README.md) | Repository summary and overview |
| [`docs/EV1527_Work_Log.md`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/docs/EV1527_Work_Log.md) | Original detailed technical work log |
| [`firmware/ev1527_generator.ino`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/firmware/ev1527_generator.ino) | Arduino generator firmware (24-bit implementation) |
| [`reference/Arduino_EV1527_1.txt`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/reference/Arduino_EV1527_1.txt) | Original task reference sketch |
| [`captures/RX_TX_of_both_boards.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/RX_TX_of_both_boards.sr) | Primary dual-channel PulseView session (TX and RX) |
| [`captures/tx_rx_run1.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/tx_rx_run1.sr) | Dual-channel run 1 |
| [`captures/tx_rx_run2.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/tx_rx_run2.sr) | Dual-channel run 2 |
| [`captures/tx_rx_run3.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/tx_rx_run3.sr) | Dual-channel run 3 (contains single-bit stretch error) |
| [`captures/remote_A.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/remote_A.sr) | Remote Button A PulseView capture |
| [`captures/remote_B.sr`](https://github.com/madhavkrishnanks/EV1527_RF_Link/blob/main/captures/remote_B.sr) | Remote Button B PulseView capture (5 consecutive frames) |
| [`images/`](https://github.com/madhavkrishnanks/EV1527_RF_Link/tree/main/images) | Complete directory of scope, analyser, and hardware photos |

---

See also: [Task Reference (Cohort 2)](https://github.com/SanjeebMishra/TaskBasedLearningCohort1/wiki/Task-Based-Learning-Cohort-2)
