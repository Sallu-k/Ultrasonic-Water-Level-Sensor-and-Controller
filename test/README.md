# Logic tests

The control functions from `src/water_level_controller.ino` are duplicated here with the Arduino
calls stripped out, so the decision logic can be compiled and run on a PC without hardware.

This catches the failures that are painful to find on a bench: an inverted percentage calculation,
a hysteresis band that doesn't actually hold state, an override that ignores the full-tank cutoff.

```bash
g++ -Wall -O1 -o logic_test logic_test.cpp && ./logic_test
```

Covers: percentage conversion and clamping · switching points on fill and drain · chatter
resistance under a jittering mid-band reading · override precedence · fail-safe on sensor fault.

Note this tests the *logic only*. It is not a substitute for running the sketch on the Arduino —
the sensor, the I²C display and the L298N are all unexercised here.
