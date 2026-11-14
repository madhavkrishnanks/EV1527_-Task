# EV1527 · 433.92 MHz RF Link

An Arduino Uno generates a 24-bit EV1527-style baseband waveform, verified with a digital oscilloscope and logic analyser. The signal is transmitted via an FS1000A-class 433.92 MHz OOK transmitter, demodulated by an XD-RF-5V receiver, captured on a 2 MHz logic analyser, and decoded in software and by hand. The transmitted code was then learned and actuated by a commercial 12 V learning relay receiver. Measured values are identified where applicable; calculated and nominal values are stated separately.

Detailed work log: [`docs/EV1527_Work_Log.md`](docs/EV1527_Work_Log.md) · Wiki: [EV1527 Baseband, RF Link, Decode and Relay](https://github.com/madhavkrishnanks/EV1527_RF_Link/wiki/EV1527-Baseband,-433.92-MHz-Link,-Decode-and-Relay)

---

## Objective

1. Generate an EV1527 pulse-width-modulated baseband frame on an ATmega328P (Arduino Uno) using GPIO bit-banging.
2. Verify pulse and frame timing against protocol specifications using an oscilloscope and logic analyser.
3. Transmit the frame across a 433.92 MHz OOK wireless link, capture the demodulated signal, and decode the received data.
4. Compare the generated frame against a commercial EV1527 handheld remote and actuate a 12 V learning relay.

---

## Hardware

| Component | Model / Type | Role in Experiment |
|---|---|---|
| Microcontroller | Arduino Uno (ATmega328P, 16 MHz, 5 V logic) | Baseband pattern generator (GPIO D8) |
| Oscilloscope | Rigol DS1054Z (50 MHz, 1 GSa/s, 10X probe) | Pulse width, amplitude, and duty-cycle verification |
| RF Transmitter | FS1000A-class (433.92 MHz SAW resonator, OOK/ASK) | Modulates D8 baseband onto 433.92 MHz RF carrier |
| RF Receiver | XD-RF-5V (433.92 MHz superheterodyne receiver) | Demodulates RF carrier to digital baseband (DATA pin) |
| Logic Analyser | 8-channel USB analyser (Saleae-compatible, 2 MHz) | Dual-channel digital capture of TX (D8) and RX (D7) |
| Antennas | 2 × 17.3 cm solid-core wire (quarter-wave monopoles) | Soldered to transmitter and receiver ANT pads |
| Relay Receiver | 12 V single-channel learning receiver (Songle SRD-12VDC-SL-C) | Commercial decoder and relay actuation target |
| Handheld Remote | 2-button EV1527 key fob (Buttons A and B) | Reference hardware for frame and timing comparison |

---

## Signal Flow

```
Arduino Uno (D8) ─── Baseband ───► FS1000A TX (433.92 MHz OOK)
       │                                     │
   Scope CH1                               Air link (17.3 cm wire antennas)
   LA Probe 0 ("TX_D8")                     │
                                            ▼
                              XD-RF-5V RX (DATA out)
                                     │
                        ┌────────────┴────────────┐
                        ▼                         ▼
             LA Probe 1 ("RX_D7")         12 V Learning Relay
             (Capture & Decode)           (Learn & Actuate)
```

---

## Protocol / Timing

EV1527 uses pulse-width modulation over On-Off Keying (OOK). Each frame consists of 25 pulses: 1 synchronization pulse followed by 24 data bits (20-bit address and 4-bit data), transmitted MSB first.

| Element | Level Encoding | Nominal Duration ($t = 250\ \mu\text{s}$) | Measured Value | Category |
|---|---|---|---|---|
| Pulse unit ($t$) | — | 250 µs | 250.0 µs (+Width on D8) | Measured |
| Bit 0 | 1t HIGH, 3t LOW | 250 µs HIGH, 750 µs LOW | 250.0 µs HIGH, 770.0 µs LOW | Measured |
| Bit 1 | 3t HIGH, 1t LOW | 750 µs HIGH, 250 µs LOW | 752–758 µs HIGH, 254–260 µs LOW | Measured |
| Bit Period | 4t total | 1000 µs (1.000 ms) | 1020 µs (1.020 ms) | Measured |
| Sync Pulse | 1t HIGH, 31t LOW | 250 µs HIGH, 7750 µs LOW | 252 µs HIGH, 7750 µs nominal | Measured / Nominal |
| Active Frame Length | 24 bits × 4t + 32t = 128t | 32.00 ms (calculated at nominal $t$) | ~32.25 ms (calculated from 1.020 ms bit period) | Calculated |
| Inter-repeat Delay | Firmware delay | 10.00 ms (`delay(10)`) | 10.00 ms | Implemented |
| Total Post-sync Gap | 31t LOW + 10 ms delay | 17.75 ms | 17.81 ms (17 810–17 816 µs) | Measured |
| Frame Repetition Period | Active frame + post-sync gap | 42.00 ms nominal | ~42.4 ms | Measured |

*Note: The 20 µs excess on the LOW duration is due to AVR `digitalWrite()` instruction overhead and `delayMicroseconds()` loop resolution.*

---

## Implementation

The generator firmware is located at [`firmware/ev1527_generator.ino`](firmware/ev1527_generator.ino).

- **Data word:** Configured as `0x3802063C`. The transmission loop sends the lower 24 bits (`0x02063C`), corresponding to 20-bit address `0x02063` and 4-bit data `1100` (`0xC`).
- **Bit order:** Transmitted MSB first using `for (int i = 23; i >= 0; i--)`.
- **Timing:** Implemented with `delayMicroseconds()` based on $T = 250\ \mu\text{s}$.
- **Bursts:** Emits 4 repeated frames per cycle with a 10 ms delay between frames.

---

## Validation Results

| Test Parameter | Intended / Reference | Measured / Observed Result | Validation Method |
|---|---|---|---|
| Baseband pulse width ($t$) | 250 µs | 250.0 µs HIGH, 770.0 µs LOW (bit 0) | Rigol DS1054Z auto-measure (500 µs/div) |
| Pulses per frame | 25 (1 sync + 24 data) | 25 pulses | Logic analyser edge count (2 MHz) |
| Transmitted word | `0x02063C` | `0x02063C` | Logic analyser TX_D8 capture decode |
| Received word (RF link) | `0x02063C` | `0x02063C` | Logic analyser RX_D7 capture decode |
| RX signal reproduction | Match TX pulses | 25 pulses replicated; ~25–30 µs latency; HIGH stretched 25–35 µs | Dual-channel capture (TX_D8 vs RX_D7) |
| Remote Button A | 24-bit frame | `0x613718` (Address: `0x61371`, Data: `1000`, $t \approx 400\ \mu\text{s}$) | Logic analyser capture (`remote_A.sr`) |
| Remote Button B | 24-bit frame | `0x613714` (Address: `0x61371`, Data: `0100`, $t \approx 400\ \mu\text{s}$) | Logic analyser capture (`remote_B.sr`, 5 runs identical) |
| Relay actuation | Responds to learned code | Relay paired in learning mode and latched upon transmission | Observed physical contact switching |

---

## Debugging

- **Sampling Aliasing:** Initial polling of receiver DATA with `digitalRead()` every 50 ms produced aliased random bits because the sampling rate was far below the 1 kHz baseband. Switched to pulse-width capture using a 2 MHz logic analyser.
- **Probe Attenuation Mismatch:** Oscilloscope read 5 V logic as 0.6 V because the probe was physically set to 10X while the scope channel setting was 1X. Corrected probe setting in the scope menu to 10X.
- **Screen Quantisation:** Auto-measurements at 10 ms/div quantised pulse widths to screen pixel boundaries (200 µs steps). Resolved by zooming the horizontal timebase to 500 µs/div for pulse measurements.
- **Frame Length Clarification:** The supplied reference sketch looped over 32 bits (`i = 31` down to 0), generating 33 pulses. Inspection with the logic analyser revealed 32 data pulses. I adapted the loop to 24 bits (`i = 23` down to 0) to comply with standard 24-bit EV1527 framing.
- **Single-MCU Contention:** Attempting to run transmitter and receiver code on a single Uno failed because receive sketches halted the transmission loop. Resolved by letting the Uno transmit continuously while capturing the receiver output with a standalone logic analyser.
- **Receiver Idle Noise:** In the absence of a 433 MHz carrier, the superheterodyne receiver AGC raised gain, producing continuous hash. Adding quarter-wave antennas and placing modules ~0.5–1 m apart established stable baseband reproduction.

---

## Evidence & Files

- **Complete Work Log:** [`docs/EV1527_Work_Log.md`](docs/EV1527_Work_Log.md) — Full technical notes, raw timing tables, and setup details.
- **Detailed Wiki Page:** [`docs/wiki/EV1527-Baseband-Link-Decode-Relay.md`](docs/wiki/EV1527-Baseband-Link-Decode-Relay.md) — Complete experimental writeup.
- **Firmware:** [`firmware/ev1527_generator.ino`](firmware/ev1527_generator.ino) — Active 24-bit generator sketch.
- **Reference Code:** [`reference/Arduino_EV1527_1.txt`](reference/Arduino_EV1527_1.txt) — Task reference sketch.
- **PulseView Captures:** [`captures/`](captures/) — Native `.sr` files (`RX_TX_of_both_boards.sr`, `tx_rx_run1.sr`, `tx_rx_run2.sr`, `tx_rx_run3.sr`, `remote_A.sr`, `remote_B.sr`).
- **Instrument Images:** [`images/`](images/) — Oscilloscope screenshots, logic analyser traces, and hardware photos.

---

## Limitations / Notes

- **Receiver Edge Distortion:** The superheterodyne receiver stretches HIGH pulses by 25–35 µs and exhibits a 25–30 µs propagation delay. Decoders must use threshold-based discrimination (e.g. threshold at 500 µs) rather than exact nominal widths.
- **Post-sync Gap Length:** The firmware adds a 10 ms delay to the 31t sync LOW, resulting in an inter-frame gap of ~17.8 ms (compared to the remote's 10.3–12.4 ms). The test relay accepted this timing; decoders enforcing strict 31t ± 20% validation would require reducing the delay to `delay(4)`.
- **Benchtop Distance Only:** RF testing was conducted at bench distances (0.5–1 m). Sensitivity, maximum range, and multi-path fading were not evaluated.
- **RF Spectrum:** No RF spectrum analyser was available; carrier frequency accuracy, harmonic suppression, and modulation bandwidth were not measured.
- **Scope:** Validates baseband generation, wireless demodulation, software decoding, and relay operation. Does not constitute RF compliance certification.

---

## Conclusion

I generated a 24-bit EV1527 baseband signal on an Arduino Uno at $t = 250\ \mu\text{s}$ and verified pulse timings on an oscilloscope. The signal was successfully transmitted across a 433.92 MHz ASK/OOK link, captured with a logic analyser, and decoded to `0x02063C`. Comparing against a commercial remote confirmed the 20-bit address and 4-bit data framing. Finally, a commercial 12 V learning relay was taught the generated code and responded reliably to the transmission.
