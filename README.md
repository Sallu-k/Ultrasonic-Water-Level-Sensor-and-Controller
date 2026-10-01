# Ultrasonic Water-Level Sensor & Controller

A closed-loop tank controller: an **HC-SR04** measures the water surface from above, an **Arduino
Uno** converts that distance into a percentage full, and an **L298N** switches a submersible pump
to keep the tank between 10 % and 90 %.

Mini-project for Basic Electronics (BBEE203) at AITM Bhatkal. Team of four, under the guidance of
Prof. Shrishail Bhat. I led the team, split the work and ran the demo.

![Finished build](docs/images/final_build.jpeg)

---

## Overview

Overhead tanks overflow because nobody is watching them. In India that's a meaningful share of
avoidable domestic water loss — a tank left filling is water straight to the drain, and the person
responsible usually finds out from the sound. The cheapest fix isn't a better valve, it's knowing
the level and acting on it automatically.

## Features

- Non-contact level sensing (HC-SR04 in the lid, 5-ping median filter)
- Distance → percentage-full conversion, clamped to 0–100 %
- Automatic pump control with 10 % / 90 % hysteresis (no chatter)
- Manual override button — starts the pump early, but can never overfill (90 % cut-off wins)
- Sensor-fault detection — 5 consecutive missing echoes stop the pump and show `SENSOR FAULT`
- Pump forced off at power-up
- Flicker-free 16×2 I²C LCD: level % and pump state
- Serial log at 9600 baud: `dist=…cm level=…% pump=ON|OFF [MANUAL]`
- PC-side logic tests (16 checks)

---

## Architecture

![System block diagram](docs/images/system_block_diagram.png)

The sensor sits above the water, not in it. Nothing wetted, nothing to corrode, and the tank can be
opened and cleaned without disturbing the electronics.

```
HC-SR04 ──echo──► Arduino Uno ──IN1/IN2/ENA──► L298N ──12 V──► pump ──► tank
                    ▲     │                                             │
         button D2 ─┘     └──I²C──► LCD          (water level read back by HC-SR04)
```

Every 300 ms the sketch runs: sense → convert to % → decide → drive the L298N → update LCD → log.

### Control law

```
level < 10 %   →   pump ON
level ≥ 90 %   →   pump OFF
10 % … 90 %    →   pump holds its current state
```

The middle band is the entire design idea. A single threshold — "turn on below 50 %" — sounds
simpler and fails immediately: the water surface ripples, readings jitter across the setpoint, and
the pump switches on and off several times a second. That's called chatter, and it destroys relays
and motor drivers.

Two thresholds with an 80-point gap between them means the level has to travel a long way before
the decision reverses. The pump can't oscillate because there's no state it can sit in where the
next reading flips it back.

![Hysteresis cycle](docs/images/hysteresis_cycle.svg)

Decision priority: sensor fault → OFF · level ≥ 90 % → OFF (and cancel override) · override → ON ·
level < 10 % → ON · otherwise hold.

### The failsafe is mechanical, not software

The overflow outlet is plumbed so that if the pump does run too long, water leaves the tank through
a route that goes nowhere near the electronics.

This is the part of the project I'd defend hardest. The control logic above is only as good as the
sensor feeding it — an HC-SR04 that gets a bad echo, or a wire that comes loose, and the software
is confidently wrong. So the system is arranged so that the *worst* thing bad software can do is
waste water, not destroy the board or create a shock hazard. Software failsafes protect against
bugs you thought of. Physical ones protect against the bugs you didn't.

---

## Project Structure

```
├── src/
│   ├── water_level_controller.ino          ← the controller (current firmware)
│   ├── water_level_measure_ORIGINAL.ino    ← original sensing-only sketch, kept as-is
│   └── water_level_measure.ino             ← identical copy of the original
├── test/
│   ├── logic_test.cpp                      ← PC-side tests for the control logic
│   └── README.md
├── docs/
│   ├── images/       block diagram, hysteresis diagram, finished build
│   ├── report/       PROJECT_REPORT (md + pdf), seminar deck
│   └── video/        demo presentation
├── CODEMAP.md / CODEMAP.pdf                ← architecture & learning map
└── LICENSE
```

---

## Requirements

**Hardware**

| Part | Role |
|---|---|
| Arduino Uno | Level calculation and control |
| HC-SR04 | Ultrasonic distance to water surface |
| 16×2 I²C LCD (0x27) | Level percentage and pump state |
| L298N | Motor driver switching the 12 V pump rail |
| Submersible DC pump | Fills the tank |
| Rocker switch | Supply isolation |
| Tact button | Manual pump override |
| 12 V DC adapter | Supply |

**Software**

- Arduino IDE (or any toolchain that builds `.ino` for the Uno)
- Libraries: `NewPing`, `LiquidCrystal_I2C` (Library Manager); `Wire` is built in.
  Versions are not pinned.
- For the tests: any C++ compiler (`g++`).

## Installation

1. Install the two libraries above via the Arduino IDE Library Manager.
2. Wire the hardware per the [pin map](#hardware).
3. Open `src/water_level_controller.ino`, select **Arduino Uno** and the port, and upload.

## Configuration

All settings are `const` values at the top of `src/water_level_controller.ino`. No secrets or
environment variables.

| Constant | Default | Notes |
|---|---|---|
| `SENSOR_TO_EMPTY` | 14.0 cm | Reading with the tank **empty** (larger number). **Calibrate.** |
| `SENSOR_TO_FULL` | 3.0 cm | Reading with the tank **full** (smaller number). **Calibrate.** |
| `PUMP_ON_BELOW` / `PUMP_OFF_ABOVE` | 10 / 90 % | Hysteresis thresholds — keep a wide gap |
| `SAMPLE_INTERVAL_MS` | 300 | Sensor sample period |
| `PING_SAMPLES` | 5 | Median filter depth |
| `MAX_BAD_READINGS` | 5 | Consecutive dropouts before fault |
| `DEBOUNCE_MS` | 50 | Button debounce |

The geometry defaults are inferred from the original 2024 sketch, not measured. To calibrate: open
the Serial Monitor (9600 baud), note `dist=` with the tank empty and again full, put those values
into the two constants, and re-upload.

## Usage

- Power on with the rocker switch. The LCD shows `Water Level Ctrl / Starting...`, then
  `Level: NN%` and `Pump: ON|OFF`.
- The pump runs automatically below 10 % and stops at 90 %.
- Press the button to top up early — the LCD shows `[MANUAL]`; the override clears itself at 90 %.
- If the sensor stops returning echoes, the pump stops and the LCD shows `SENSOR FAULT`. It
  recovers automatically on the next good reading.

## Hardware

**Pin map**

```
HC-SR04   TRIG → D12      ECHO → D10      VCC → 5V   GND → GND
LCD       SDA  → A4       SCL  → A5       VCC → 5V   GND → GND
L298N     IN1  → D7       IN2  → D8       ENA → D6   GND → Arduino GND
Button    D2 → GND (INPUT_PULLUP)
```

The L298N and the Arduino **must share a ground** — without it the driver's logic inputs have no
reference and the pump never runs.

**Accuracy:** roughly half a centimetre in practice, which is about what an HC-SR04 gives you. That
figure is from observation during testing, not a characterised measurement — treat it as an
estimate. Note the current sketch converts to whole centimetres (`convert_cm()`), so on the default
11 cm span it resolves in ~9 % steps.

## API / Interfaces

No network API. Interfaces are the LCD, the override button, and a 9600-baud serial log:

```
dist=<cm>cm level=<n>% pump=<ON|OFF>[ [MANUAL]]
SENSOR FAULT - pump stopped
Override ON|OFF
```

## Development

Edit `src/water_level_controller.ino`. The decision logic lives in two pure functions —
`distanceToPercent()` and `updatePump()` — kept separate from pin I/O (`applyPumpOutput()`) so they
can be tested off-target. If you change either function, copy the change into
`test/logic_test.cpp`; the test file holds a duplicate, not a shared header.

## Testing

The control logic is duplicated in `test/logic_test.cpp` with the Arduino calls removed, so it can
be compiled and run on a PC:

```bash
cd test && g++ -Wall -O1 -o logic_test logic_test.cpp && ./logic_test
```

Sixteen checks covering the percentage conversion and its clamping, the switching points on a fill
and a drain, chatter resistance against a jittering mid-band reading, override precedence against
the full-tank cutoff, and the fail-safe path. All pass. The exit code is the number of failures.

This tests the decision logic only — the sensor, the display and the L298N are not exercised here,
so it does not replace running the sketch on real hardware.

## Deployment

Upload to the Uno from the Arduino IDE. There is no other deployment step.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Pump never runs | No common ground between L298N and Arduino; IN2 not held LOW; ENA jumper/wiring |
| LCD blank | I²C address is `0x3F` rather than `0x27` — run an I²C scanner and change it |
| `SENSOR FAULT` | HC-SR04 disconnected, mis-aimed, or out of range |
| Level reads wrong / inverted | `SENSOR_TO_EMPTY` / `SENSOR_TO_FULL` not calibrated or swapped |
| `[MANUAL]` shows without the closing bracket | Known cosmetic issue: the line is 17 chars on a 16-char display |

## Project Status

The system was built and demonstrated, pump included, driven through the L298N.

**The original controller sketch did not survive.** `water_level_measure_ORIGINAL.ino` is the
earlier sensing-and-display stage — it reads the sensor and prints to the LCD, with no pump logic.
The complete version was lost.

`water_level_controller.ino` is therefore a **rewrite from the documented design**, not a recovered
original, and it is labelled that way deliberately. Its logic is verified by the tests above; it
has not yet been re-run on the hardware.

The original sketch is kept unmodified, faults included — it only refreshed the display below
14 cm, so outside that window it held a stale reading indefinitely. Leaving it visible is more
useful than quietly deleting it.

**Known limitations:** whole-centimetre resolution; tank geometry not yet calibrated; override is
not cleared by a sensor fault (still bounded by the 90 % cut-off); no dry-run protection for the
pump. Full list in [docs/report/PROJECT_REPORT.md §15](docs/report/PROJECT_REPORT.md).

## Roadmap

From the seminar deck:

- Wireless monitoring/control (Wi-Fi or Bluetooth, smartphone app)
- Additional sensors (e.g. water flow)
- Solar / renewable power

Immediate next step: flash the rewrite, calibrate, and re-run the demo on hardware.

## License

MIT — see [LICENSE](LICENSE). © 2025 Salsabeel Kobattey.

## Documentation

- [docs/report/PROJECT_REPORT.md](docs/report/PROJECT_REPORT.md) (and `.pdf`) — detailed engineering report
- [docs/report/seminar_deck.pptx](docs/report/seminar_deck.pptx) — original seminar: methodology, components, references
- [docs/video/demo_presentation.mp4](docs/video/demo_presentation.mp4) — demo
- [test/README.md](test/README.md) — test scope

## CodeMap Learning

This project includes [CODEMAP.md](CODEMAP.md) and [CODEMAP.pdf](CODEMAP.pdf), which provide a
structured architecture and learning map for use with CodeMap Learning.

---

## Team

Salsabeel Kobattey · Shayan Ahmed · Nawwaf Peshmam · Mohammed Ruwaif
Guide: Prof. Shrishail Bhat, Dept. of ECE, AITM Bhatkal
