/*
 * ============================================================================
 *  Ultrasonic Water-Level Controller
 * ============================================================================
 *
 *  Arduino Uno · HC-SR04 · 16x2 I2C LCD · L298N motor driver · DC pump
 *  Mini-project, Basic Electronics (BBEE203), AITM Bhatkal.
 *
 *  WHAT IT DOES
 *    Measures the distance from a sensor mounted above the tank down to the
 *    water surface, converts that to a percentage full, and switches a pump
 *    to keep the tank between 10% and 90%.
 *
 *  WIRING
 *    HC-SR04   TRIG -> D12      ECHO -> D10      VCC -> 5V   GND -> GND
 *    LCD       SDA  -> A4       SCL  -> A5       VCC -> 5V   GND -> GND
 *    L298N     IN1  -> D7       IN2  -> D8       ENA -> D6   GND -> GND
 *    Button    D2 to GND, INPUT_PULLUP
 *
 *    The L298N and the Arduino MUST share a ground. Two supplies with no
 *    common ground is the most common reason a motor driver "does nothing":
 *    the logic input has no reference to compare against.
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>

// ---------------------------------------------------------------- pins -----
const uint8_t TRIGGER_PIN  = 12;
const uint8_t ECHO_PIN     = 10;
const uint8_t PUMP_IN1     = 7;
const uint8_t PUMP_IN2     = 8;
const uint8_t PUMP_ENA     = 6;
const uint8_t OVERRIDE_BTN = 2;

// ------------------------------------------------------- tank geometry -----
/*
 *  >>> MEASURE THESE ON THE REAL TANK AND CHANGE THEM. <<<
 *
 *  SENSOR_TO_EMPTY : reading with the tank EMPTY — surface at the bottom, so
 *                    the echo travels furthest, so this is the LARGER number.
 *  SENSOR_TO_FULL  : reading with the tank FULL — surface nearest the sensor,
 *                    so the SMALLER number.
 *
 *  These defaults are inferred from the original 2024 sketch, which used 12.50
 *  as its reference and treated anything under 14 cm as "deep". They are a
 *  starting point, not measurements.
 */
const float SENSOR_TO_EMPTY = 14.0;   // cm
const float SENSOR_TO_FULL  =  3.0;   // cm

// ----------------------------------------------------- control settings ----
const uint8_t PUMP_ON_BELOW  = 10;    // % — start filling below this
const uint8_t PUMP_OFF_ABOVE = 90;    // % — stop filling above this

const unsigned long SAMPLE_INTERVAL_MS = 300;
const uint8_t PING_SAMPLES     = 5;   // median filter depth
const uint8_t MAX_BAD_READINGS = 5;   // consecutive dropouts before fault
const unsigned long DEBOUNCE_MS = 50;

const unsigned int MAX_DISTANCE = 200;  // cm, NewPing ceiling

// ------------------------------------------------------------ objects ------
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------------------------------------------------- state -------
/*
 *  pumpRunning is what makes hysteresis possible. Between 10% and 90% the
 *  controller makes NO new decision — it keeps doing whatever it was doing.
 *  Without a variable remembering that, there is nothing to keep.
 */
bool pumpRunning  = false;
bool overrideOn   = false;
bool sensorFailed = false;

uint8_t badReadings = 0;
int     lastLevel   = -1;          // -1 = nothing valid measured yet

unsigned long lastSampleMs    = 0;
unsigned long lastBtnChangeMs = 0;
bool lastBtnReading = HIGH;
bool btnStable      = HIGH;


// ===========================================================================
//  Distance -> percentage full.
//
//  The relationship is INVERTED: a bigger distance means LESS water, because
//  the sensor looks down at a surface that has dropped away from it. Writing
//  this the intuitive way round makes the pump do exactly the wrong thing.
//
//      distance == SENSOR_TO_EMPTY  ->    0%
//      distance == SENSOR_TO_FULL   ->  100%
//
//  Clamped, because a stray echo off a ripple or the tank wall can read
//  shorter than SENSOR_TO_FULL and would otherwise yield something like 118%.
// ===========================================================================
int distanceToPercent(float distanceCm) {
  const float span = SENSOR_TO_EMPTY - SENSOR_TO_FULL;
  if (span <= 0.0) return 0;                    // misconfigured constants

  float pct = ((SENSOR_TO_EMPTY - distanceCm) / span) * 100.0;

  if (pct <   0.0) pct =   0.0;
  if (pct > 100.0) pct = 100.0;
  return (int)(pct + 0.5);                      // round, don't truncate
}


// ===========================================================================
//  The control law.
//
//      below 10%   -> start
//      above 90%   -> stop
//      in between  -> DO NOTHING, deliberately
//
//  That empty middle branch is the whole design. With one threshold, ripples
//  push readings back and forth across it and the pump switches several times
//  a second — "chatter" — wearing out the driver and the pump. Two thresholds
//  80 points apart mean the level must travel a long way before the decision
//  can reverse, so oscillation is impossible by construction.
//
//  The 90% cutoff is checked BEFORE the override. Manual override can start
//  the pump early, but it cannot overfill the tank. An override that defeats
//  every limit isn't an override, it's a way to flood a room.
// ===========================================================================
void updatePump(int levelPercent) {
  if (sensorFailed) {                 // no trustworthy reading -> fail safe
    pumpRunning = false;
    return;
  }

  if (levelPercent >= PUMP_OFF_ABOVE) {
    pumpRunning = false;
    overrideOn  = false;              // cancel override once full
    return;
  }

  if (overrideOn) {
    pumpRunning = true;
    return;
  }

  if (levelPercent < PUMP_ON_BELOW) {
    pumpRunning = true;
  }
  // else: between thresholds. Hold state. This blank is intentional.
}


// ---------------------------------------------------------------------------
//  Drive the L298N. IN1/IN2 set direction, ENA enables the channel.
//  IN1 high + IN2 low = forward. Both low = coast.
// ---------------------------------------------------------------------------
void applyPumpOutput() {
  digitalWrite(PUMP_IN1, pumpRunning ? HIGH : LOW);
  digitalWrite(PUMP_IN2, LOW);
  digitalWrite(PUMP_ENA, pumpRunning ? HIGH : LOW);
}


// ---------------------------------------------------------------------------
//  Debounced momentary button. A mechanical contact bounces for a few ms when
//  pressed; undebounced, one press reads as several and the override toggles
//  an unpredictable number of times.
//
//  Returns true once per genuine press.
// ---------------------------------------------------------------------------
bool overridePressed() {
  bool reading = digitalRead(OVERRIDE_BTN);

  if (reading != lastBtnReading) {
    lastBtnChangeMs = millis();
    lastBtnReading  = reading;
  }

  if ((millis() - lastBtnChangeMs) > DEBOUNCE_MS && reading != btnStable) {
    btnStable = reading;
    if (btnStable == LOW) return true;         // INPUT_PULLUP: LOW = pressed
  }
  return false;
}


// ---------------------------------------------------------------------------
//  LCD update.
//
//  No lcd.clear() — clearing and redrawing every cycle makes the display
//  flicker visibly. Each line is padded to a fixed 16 characters instead, so
//  when "100%" drops to "45%" the leftover digit is overwritten by a space
//  rather than lingering as "450%".
//
//  Only writes when something changed, which also keeps the I2C bus quiet.
// ---------------------------------------------------------------------------
void updateDisplay(int levelPercent) {
  static int  shownLevel = -2;
  static bool shownPump  = false;
  static bool shownOvr   = false;
  static bool shownFail  = false;

  if (levelPercent == shownLevel && pumpRunning == shownPump
      && overrideOn == shownOvr && sensorFailed == shownFail) return;

  shownLevel = levelPercent;
  shownPump  = pumpRunning;
  shownOvr   = overrideOn;
  shownFail  = sensorFailed;

  char line[17];

  if (sensorFailed) {
    snprintf(line, sizeof(line), "%-16s", "SENSOR FAULT");
  } else {
    snprintf(line, sizeof(line), "Level: %3d%%     ", levelPercent);
  }
  lcd.setCursor(0, 0);
  lcd.print(line);

  snprintf(line, sizeof(line), "Pump: %-3s%-7s",
           pumpRunning ? "ON" : "OFF",
           overrideOn  ? "[MANUAL]" : "");
  lcd.setCursor(0, 1);
  lcd.print(line);
}


// ===========================================================================
void setup() {
  pinMode(PUMP_IN1,     OUTPUT);
  pinMode(PUMP_IN2,     OUTPUT);
  pinMode(PUMP_ENA,     OUTPUT);
  pinMode(OVERRIDE_BTN, INPUT_PULLUP);

  /*
   *  Force the pump off before anything else. On power-up an output pin's
   *  state is undefined until you set it, and "undefined" on a pin that
   *  controls a pump is not acceptable. Actuators start in their safe state.
   */
  pumpRunning = false;
  applyPumpOutput();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Water Level Ctrl");
  lcd.setCursor(0, 1);
  lcd.print("Starting...     ");

  Serial.begin(9600);
  Serial.println(F("Water level controller ready"));
  delay(1000);
}


void loop() {
  // Polled every pass so a press is never missed, independent of the slower
  // sensor sampling rate.
  if (overridePressed()) {
    overrideOn = !overrideOn;
    Serial.print(F("Override "));
    Serial.println(overrideOn ? F("ON") : F("OFF"));
  }

  /*
   *  millis() scheduling instead of delay(). delay() blocks — during a 300 ms
   *  delay the Arduino cannot notice a button press or do anything else.
   *  Comparing timestamps lets the sensor run at its own rate while the rest
   *  of the loop keeps spinning.
   */
  if (millis() - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = millis();

  /*
   *  ping_median() fires several pings and takes the middle value. A single
   *  ping off a moving surface is noisy; the median discards outliers without
   *  the lag a running average would introduce.
   */
  unsigned int us = sonar.ping_median(PING_SAMPLES);
  unsigned int distance = sonar.convert_cm(us);

  /*
   *  A reading of 0 means NO ECHO CAME BACK — out of range, bad angle, or a
   *  disconnected sensor. It does NOT mean "distance is zero, tank is full".
   *  Treating 0 as a real measurement is how a controller floods a room.
   *
   *  A few dropouts are normal on a rippling surface, so one bad reading is
   *  ignored and the last good level is kept. Only a sustained run is treated
   *  as a real fault — and then the pump stops.
   */
  if (distance == 0) {
    if (badReadings < MAX_BAD_READINGS) badReadings++;
    if (badReadings >= MAX_BAD_READINGS && !sensorFailed) {
      sensorFailed = true;
      pumpRunning  = false;
      applyPumpOutput();
      updateDisplay(lastLevel);
      Serial.println(F("SENSOR FAULT - pump stopped"));
    }
    return;
  }

  badReadings  = 0;
  sensorFailed = false;

  int level = distanceToPercent((float)distance);
  lastLevel = level;

  updatePump(level);
  applyPumpOutput();
  updateDisplay(level);

  Serial.print(F("dist="));     Serial.print(distance);
  Serial.print(F("cm level=")); Serial.print(level);
  Serial.print(F("% pump="));   Serial.print(pumpRunning ? F("ON") : F("OFF"));
  Serial.println(overrideOn ? F(" [MANUAL]") : F(""));
}
