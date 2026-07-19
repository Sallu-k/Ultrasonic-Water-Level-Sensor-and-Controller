# Ultrasonic Water-Level Sensor & Controller

An Arduino-based system that measures water level in a tank with an **HC-SR04 ultrasonic sensor** and shows it live on an I²C LCD — the sensing core of a water-management system designed to cut waste from tank overflow.

Mini-project (Basic Electronics, BBEE203) at AITM Bhatkal. Team of 4, presented under the guidance of Prof. Shrishail Bhat.

---

## The problem it addresses

Tank overflow is a large, avoidable source of water waste. The project's motivation (from the seminar): a huge volume of water is lost every year to overhead-tank and reservoir overflow, and real-time level monitoring is the simplest way to prevent it. Full write-up in [`docs/report/seminar_deck.pptx`](docs/report/).

## What the code does

The included sketch ([`src/water_level_measure.ino`](src/water_level_measure.ino)):

- Fires the **HC-SR04** and reads distance in cm using the `NewPing` library.
- Converts distance-to-surface into a level reading and prints it to an **I²C 16×2 LCD** (`LiquidCrystal_I2C`, address `0x27`).
- Displays a `Deep` warning with the computed offset when the water surface is within a set threshold.
- Streams the raw and adjusted readings over serial for debugging.

**Pin mapping:** Trigger → D12, Echo → D10, LCD → I²C (A4/SDA, A5/SCL).

## Designed full system

The seminar documents the complete controller design the code is part of — an Arduino Uno reading the ultrasonic sensor, driving a **relay/motor-driver-controlled pump**, an LCD unit, and a manual override button, so the pump switches automatically on level. Block diagram and methodology are in the deck.

```
 HC-SR04 ──► Arduino Uno ──► Relay / Motor driver ──► Pump
                  │
                  ▼
              I²C LCD (level + status)
```

> **Honest status:** the code in this repo covers the **sensing + LCD display** stage that was built and demonstrated (see `docs/video/demo_presentation.mp4`). The relay/pump-actuation logic is described in the design deck; wiring it into this sketch is a straightforward next step (read level → compare to setpoint with hysteresis → toggle relay pin).

## Repository layout

```
ultrasonic-water-level-controller/
├── src/water_level_measure.ino     ← Arduino sketch (sensing + LCD)
└── docs/
    ├── report/seminar_deck.pptx    ← full project seminar (methodology, block diagram, results, refs)
    └── video/demo_presentation.mp4 ← working demo
```

## Hardware

- Arduino Uno
- HC-SR04 ultrasonic sensor
- 16×2 I²C LCD (`0x27`)
- (Full system) relay module + motor driver + pump

Libraries: `NewPing`, `Wire`, `LiquidCrystal_I2C`.

## Team

Salsabeel Kobattey · Shayan Ahmed · Nawwaf Peshmam · Ruwaif Fakardey
Guide: Prof. Shrishail Bhat, Dept. of ECE, AITM Bhatkal.
