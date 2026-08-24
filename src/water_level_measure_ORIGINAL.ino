#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>

// Define the pins for the ultrasonic sensor
#define TRIGGER_PIN 12
#define ECHO_PIN 10
#define MAX_DISTANCE 200 // Maximum distance we want to measure (in centimeters)

// Initialize the ultrasonic sensor
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

// Initialize the LCD with I2C address 0x27 and size 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variable to store the adjusted distance
float adjustedDistance;

void setup() {
  // Initialize the LCD
  lcd.init();
  // Turn on the LCD backlight
  lcd.backlight();
  // Initialize serial communication for debugging
  Serial.begin(9600);
}

void loop() {
  // Read the distance from the ultrasonic sensor
  unsigned int distance = sonar.ping_cm();
  
  // Print the distance to the Serial Monitor for debugging
  Serial.print("Distance: ");
  Serial.println(distance);
  
  // Check if the distance is less than 14 cm
  if (distance > 0 && distance < 14) {
    // Calculate the adjusted distance
    adjustedDistance = 12.50 - distance;

    // Clear the LCD display
    lcd.clear();

    // Print "Deep" on the first line of the LCD
    lcd.setCursor(0, 0);
    lcd.print("Deep");

    // Print the adjusted distance on the second line of the LCD
    lcd.setCursor(0, 1);
    lcd.print(adjustedDistance);

    // Print the adjusted distance to the Serial Monitor for debugging
    Serial.print("Adjusted Distance: ");
    Serial.println(adjustedDistance);
  }
  
  // Delay to control the refresh rate of the loop
  delay(150);
}
