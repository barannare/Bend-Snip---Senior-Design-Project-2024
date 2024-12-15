#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin definitions for the joystick
const int joyX = A0;  // X-axis (horizontal)
const int joyY = A1;  // Y-axis (vertical)
const int joySwitch = 22; // Digital pin for joystick button


// Variables for zero calibration
int zeroX = 0;
int zeroY = 0;

// Variables for wire length and wire count
int wireLength = 5; // Start within the range for wire length (5-130 mm)
int wireCount = 1;  // Start within the range for wire count (1-240)

// Threshold to detect movement
const int threshold = 300;

// Timing variables for continuous increment/decrement
unsigned long lastActionTime = 0;
const unsigned long actionInterval = 200; // Time in milliseconds between increments/decrements

// Mode flag: true for setting wire length, false for wire count
bool settingWireLength = true;

// Define the number of steps per revolution for your stepper motor
const int stepsPerRevolution = 200; // Adjust this based on your motor's specification

// Define pins for power and stop buttons
const int startbutton = 18;
const int stopbutton = 19;

// Create instances of the Stepper class for each motor
Stepper myStepper1(stepsPerRevolution, 6, 7, 8, 9);
Stepper myStepper2(stepsPerRevolution, 10, 11, 12, 13);
Stepper myStepper3(stepsPerRevolution, 2, 3, 4, 5);

// Conversion factor: Steps per millimeter
const int stepsPerMillimeter = 7; //(7.11 steps / 1 mm)

// Variables for user input of desired length and number of wires
int desiredLength = 0;
int stepsToMove = 0;
int desireCount = 0;
// Variable to control Voltage spikes
int skipstart = 0;


// State variables for button controls
volatile byte isMotorOn = false; // Tracks whether the motor is running
volatile byte startButtonPressed = false;
volatile byte stopButtonPressed = false;
volatile int myStepper2Position = 0; // Tracks position relative to zero (in steps)

void setup() {
  myStepper1.setSpeed(200);    // Set speed (in RPM) for myStepper1
  myStepper2.setSpeed(200);    // Set speed (in RPM) for myStepper2
  myStepper3.setSpeed(200);    // Set speed (in RPM) for myStepper3
  Serial.begin(9600);          // Initialize serial communication

  lcd.init();
  lcd.backlight();

  pinMode(joySwitch, INPUT_PULLUP); // Set joystick switch pin as input with pull-up resistor
  pinMode(startbutton, INPUT_PULLUP); // Set button pins as inputs with pull-up resistors
  pinMode(stopbutton, INPUT_PULLUP);

  // Attach interrupts for start and stop buttons
  attachInterrupt(digitalPinToInterrupt(startbutton), togglestart, FALLING);
  attachInterrupt(digitalPinToInterrupt(stopbutton), togglestop, FALLING);

  // Calibrate zero positions
  zeroX = analogRead(joyX);
  zeroY = analogRead(joyY);


  // Initialize display
  lcd.clear();
  updateDisplay();
}

void loop() {
  // Read raw joystick positions
  int xVal = analogRead(joyX);
  int yVal = analogRead(joyY);
  int switchState = digitalRead(joySwitch); // Read joystick switch state

  // Toggle mode when the switch is pressed
  if (switchState == LOW) { // Switch is active when LOW
    settingWireLength = !settingWireLength; // Toggle mode
    delay(300); // Debounce delay
    updateDisplay(); // Update display to reflect mode change
  }

  // Calculate differences from zero
  int deltaX = abs(xVal - zeroX);
  int deltaY = abs(yVal - zeroY);

  // Check if the joystick is moved beyond the threshold
  if (abs(deltaX) > threshold || abs(deltaY) > threshold) {
    if (millis() - lastActionTime > actionInterval) {
      if (deltaY > threshold) {
        // Increment based on the current mode
        if (settingWireLength) {
          wireLength++;
          if (wireLength > 130) wireLength = 5; // Wrap wire length
        } else {
          wireCount++;
          if (wireCount > 240) wireCount = 1; // Wrap wire count
        }
      } else if (deltaY < -threshold) {
        // Decrement based on the current mode
        if (settingWireLength) {
          wireLength--;
          if (wireLength < 5) wireLength = 130; // Wrap wire length
        } else {
          wireCount--;
          if (wireCount < 1) wireCount = 240; // Wrap wire count
        }
      }

      // Update last action time
      lastActionTime = millis();

      // Update the display
      Serial.print(settingWireLength ? "Wire Length: " : "Wire Count: ");
      Serial.println(settingWireLength ? wireLength : wireCount);
      updateDisplay();
    }
  } else {
    lastActionTime = 0; // Reset the timer if the joystick is in the neutral position
  }

  // Check if power button was pressed
  if ((startButtonPressed) && (skipstart != 0)) {
    startButtonPressed = false; // Reset flag
    isMotorOn = !isMotorOn;     // Toggle motor state
    if (isMotorOn) {
      Serial.println("Motor sequence started!");
    }
  }
  if ((startButtonPressed) && (skipstart == 0)) {
    skipstart++;
    startButtonPressed = false; // Reset flag
    Serial.println("False start");
    delay(200);
  }



  // Execute the motor sequence only if the start button has been pressed and skipstart != 0 
  if (isMotorOn) {
    for (int w = 1; w <= wireCount; w++) {
      // 1. Move myStepper1 first (wireLength * stepsPerMillimeter steps)
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, 75, false);

      // 2. Move myStepper2 down (458 steps)
      Serial.println("Moving Stepper 2 down");
      moveStepper(myStepper2, 503, false);  // Move down

      // 3. Move myStepper2 up (458 steps)
      Serial.println("Moving Stepper 2 up");
      moveStepper(myStepper2, 503, true); // Move up

      // 4. Move myStepper1 back (100 steps)
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, 100, true);

      // 5. Move myStepper1 forward
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, wireLength * stepsPerMillimeter + 100 +75, false);

      // 6. Move myStepper2 down  (458 steps)
      Serial.println("Moving Stepper 2 down");
      moveStepper(myStepper2, 503, true);  // Move down

      // 7. Move myStepper2 up (458 steps)
      Serial.println("Moving Stepper 2 up");
      moveStepper(myStepper2, 503, fasle); // Move up

      // 8. Move myStepper1 back
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, wireLength * stepsPerMillimeter, true);

      // 9. Spin myStepper3 forward
      Serial.println("Moving Stepper 3");
      moveStepper(myStepper3, 200, false);

      // 10. Move myStepper1 forward
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, 100, false);

      // 11. Spin myStepper3 forward
      Serial.println("Moving Stepper 3");
      moveStepper(myStepper3, 200, false);

      // 12. Move myStepper2 to cut
      Serial.println("Moving Stepper 2 down");
      moveStepper(myStepper2, 650, true); // Move forward

      // 13. Move myStepper2 to go up after cut
      Serial.println("Moving Stepper 2 up");
      moveStepper(myStepper2, 650, false); // Move backward

      // 14. Move myStepper1 forward
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, 200, true);

      // 15. Move myStepper1 forward
      Serial.println("Moving Stepper 1");
      moveStepper(myStepper1, 200, false);

    // Adjust after 30 runs
      if (w >= 30) {
        Serial.println("Moving Stepper 2 down");
        moveStepper(myStepper2, 15, false); // Move up
        Serial.println("Motors adjusted after 30 runs.");
      } else {
        Serial.print("Motor sequence completed ");
        Serial.print(w);
        Serial.println(" times.");
      }
      if (stopButtonPressed) {
        stopButtonPressed = false; // Reset flag
        Serial.println("Motor sequence stopped!");
        break; 
  }
    }
      isMotorOn = false;
      Serial.println("Moving Stepper 2 up");
      Serial.println("Motor sequence completed after desired amount is reached.");

    }
  }

// Function to update the LCD display
void updateDisplay() {
  lcd.clear();
  // Display wire length on the top line
  lcd.setCursor(0, 0);
  lcd.print("Wire (mm): ");
  lcd.print(wireLength);

  // Display wire count on the bottom line
  lcd.setCursor(0, 1);
  lcd.print("# Wires: ");
  lcd.print(wireCount);

  // Highlight current mode
  if (settingWireLength) {
    lcd.setCursor(15, 0); // Add indicator to the top line
    lcd.print("*");
  } else {
    lcd.setCursor(15, 1); // Add indicator to the bottom line
    lcd.print("*");
  }
}

// Function to move a stepper motor a specific number of steps in a given direction
void moveStepper(Stepper &stepper, int steps, bool forward) {
  int stepDirection = forward ? 1 : -1;
  for (int i = 0; i < steps; i++) {
    stepper.step(stepDirection);
    delay(5); // Short delay for smoother movement

    // Update position tracking for myStepper2
    if (&stepper == &myStepper2) {
      myStepper2Position += forward ? 1 : -1;
    }
  }
}

void togglestop() {
  // Debounce delay in the ISR
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 500) { // Debounce interval (200 ms)
    stopButtonPressed = true;
    isMotorOn = false;        // Stop motor sequence
    Serial.println("stop button pressed");
  }
}

void togglestart() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 500) {
    startButtonPressed = true;
    Serial.println("start button pressed");
  }
  lastInterruptTime = interruptTime;
}