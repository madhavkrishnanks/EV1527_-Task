# EV1527 433.92 MHz link — generate, verify, decode, actuate

An Arduino generates the EV1527 baseband frame for a 24-bit code, the timing is verified on a Rigol DS1054Z, the frame is transmitted over a 433.92 MHz OOK link with an FS1000A-class pair, received, and decoded back to the same 24-bit word on a logic analyser. The decoded word then operates a commercial EV1527 learning relay receiver.

The interesting part is not that it works. It is that every number in this repository was measured, and that five real faults were found and fixed along the way — including one that is present in the task's own reference sketch.

**Status:** all technical stages complete and demonstrated. Full work log: [`docs/EV1527_Work_Log.md`](docs/EV1527_Work_Log.md).

---

## Results at a glance

| Quantity | Value |
|---|---|
| Pulse unit t | 250 µs |
| Bit 0 / bit 1 | 250 / 770 µs HIGH, 770 / 250 µs LOW |
| Bit period | 1.020 ms (4t) |
| Pulses per frame | 25 (1 sync + 24 data) |
| Transmitted word | `0x02063C` |
| Word decoded from the received frame | `0x02063C` |
| Remote's own frames | A `0x613718`, B `0x613714`, address `0x61371` |
| Relay | operates from the generated frame |

## Signal chain

```
   Arduino Uno ── D8 ──┬── Rigol DS1054Z CH1        (timing, amplitude)
                       ├── logic analyser CH0        ("TX_D8")
                       └── FS1000A transmitter ── 433.92 MHz OOK ──┐
                                                                    │
   EV1527 learning relay receiver  ◄── XD-RF-5V receiver ◄──────────┘
            │                                │
          relay contact                DATA ─┴─ Arduino D7 / analyser CH1 ("RX_D7")
```

## Protocol

EV1527 is fixed-code OOK. One frame is 25 pulses; 24 bits are sent MSB first, and every bit occupies exactly 4t regardless of value, so the frame length is constant (128t).

| Element | HIGH | LOW | At t = 250 µs |
|---|---|---|---|
| sync | 1t | 31t | 250 / 7750 µs |
| bit 0 | 1t | 3t | 250 / 750 µs |
| bit 1 | 3t | 1t | 750 / 250 µs |

The 24-bit word is a 20-bit transmitter address plus a 4-bit data field that selects the button. That split was confirmed by measurement, not assumed — see the two remote frames below.

## The two decoded words

| Source | 24-bit word | Address (C19..C0) | Data (D3..D0) |
|---|---|---|---|
| Arduino, generated | `0x02063C` | `0x02063` | `1100` |
| Remote, button A | `0x613718` | `0x61371` | `1000` |
| Remote, button B | `0x613714` | `0x61371` | `0100` |

The address is identical across both remote buttons and only the 4-bit data field changes — the packet structure, demonstrated rather than quoted.

The generated word is the low 24 bits of the task's `0x3802063C`, which is a 32-bit constant for a 24-bit field.

## Repository contents

| Path | Contents |
|---|---|
| `firmware/ev1527_generator.ino` | bit-banged generator, t = 250 µs, 24 bits |
| `reference/Arduino_EV1527_1.txt` | the task's reference sketch, unmodified |
| `captures/RX_TX_of_both_boards.sr` | sigrok session, 2 MHz, 200 000 samples, TX and RX — reopen in PulseView and re-derive every timing below |
| `docs/EV1527_Work_Log.md` | full work log: method, measurements, defects, status |
| `images/` | scope and analyser captures, hardware photographs |

## Reproducing it

1. Flash `firmware/ev1527_generator.ino` (Arduino Uno, output on D8).
2. Scope CH1 on D8, 10X probe **and** 10X in the channel menu: 1 V/div, 500 µs/div for pulse widths, 10 ms/div for frame structure, rising-edge trigger at ~1.5 V, Normal sweep.
3. Transmitter DATA to D8, receiver DATA to the analyser, both at 5 V with a 17.3 cm wire on each ANT pad; keep the boards 0.5–1 m apart.
4. Capture at 2 MHz. Find the frame boundary at the long LOW, then read the HIGH width of each of the 24 pulses: over 500 µs is a 1.
5. For the relay: 12 V into the learning receiver, put it in learn mode, and transmit from the Arduino.

## What was measured, and how

| Measurement | Instrument | Result |
|---|---|---|
| Pulse widths | DS1054Z, 500 µs/div, auto-measure | 250.0 / 770.0 µs, 1.020 ms period, 24.51 % duty |
| Pulses per frame | Logic analyser, 2 MHz | 25 |
| TX vs RX | Simultaneous 2-channel capture | 50 pulses each over two frames; RX lags ~25–30 µs and each HIGH stretches 25–35 µs |
| Frame period | Software decode of the raw capture | 42.4 ms (24.5 ms data + 7.75 ms sync + 10 ms inter-frame delay) |
| Decode | Hand, then software | `0x02063C` on both TX and RX |

## Five faults found by measurement

| Fault | How it showed up | Fix |
|---|---|---|
| 32-bit loop in the reference sketch | 32 pulses per frame instead of 25 | loop `i = 23`, sending the low 24 bits |
| 10X probe, 1X channel setting | 5 V logic read as 0.6 V | set the probe attenuation in the channel menu |
| Auto-measurements quantised to one pixel | `+Wid 200 µs, Rise 200 µs` at 10 ms/div | measure widths at 500 µs/div or with cursors |
| Sampling a 1 ms signal every 50 ms | random 1/0 stream, 60 % ones, no repeating window | measure widths, not levels |
| One board cannot transmit and receive at once | every receive test decoded noise | observe the link with the analyser while the generator keeps running |

A sixth, still open by design: the firmware's post-sync gap is 17.8 ms against the reference remote's 10.3–12.4 ms, because a 10 ms delay is added to 31t. The relay accepts it; a stricter decoder may not. Documented in §4.1 of the work log with the one-line fix.

## Limitations

- No photograph or video of the relay stage exists; it is recorded in writing in §7 of the work log.
- Pairing persistence across a power cycle of the relay board itself was not separately verified.
- Range was not characterised; the link was tested at bench distances.
- The PulseView built-in OOK decoder is Manchester-based and does not apply to EV1527 pulse-width coding. Decoding was done by hand and in software from the raw capture.

## Reference

Task: Sanjeeb Mishra, Task-Based Learning Cohort 2 — *EV1527 waveform pattern generation using AFG and Arduino*.
`github.com/SanjeebMishra/TaskBasedLearningCohort1/wiki/Task-Based-Learning-Cohort-2`

K.S. Madhavkrishnan · November 2026
