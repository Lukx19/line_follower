#include <Arduino.h>
#include <Servo.h>

#define button_pin 2
#define LED_pin 11
#define first_infra 3
#define timeout_delay 1000
#define STOP_SPEED 1500
#define LEFT_DIR = -1
#define RIGHT_DIR = 1
Servo left;
Servo right;

bool ir_sensors[5];
bool start = false;
bool find_middle = false;
size_t timeout_ = 0;
enum Dir { LEFT, RIGHT };
Dir route_;
enum State { STANDARD, FORK, CROSS_EXPECTED, PASS_MARKER, STOP };
State state_;

void LEDOn() { digitalWrite(11, HIGH); }
void LEDOff() { digitalWrite(11, LOW); }
void LEDBlink(size_t blinks) {
  LEDOff();
  for (size_t i = 0; i < blinks; ++i) {
    LEDOn();
    delay(100);
    LEDOff();
    delay(100);
  }
}
bool buttonDown() { return !digitalRead(button_pin); }

size_t countPresses() {
  unsigned long start = millis();
  bool down = false;
  size_t presses = 0;
  while (millis() - start < 3000) {
    if (buttonDown() && down == false) {
      ++presses;
      down = true;
    } else {
      down = false;
    }
    delay(100);
  }
  return presses;
}

void forward(int speed = 200) {
  left.write(STOP_SPEED + speed);
  right.write(STOP_SPEED - speed);
}

void backward(int speed = 200) {
  left.write(STOP_SPEED - speed);
  right.write(STOP_SPEED + speed);
}

void stop() {
  left.write(STOP_SPEED);
  right.write(STOP_SPEED);
}
int getDir(Dir dir) { return dir == LEFT ? -1 : 1; }

/* inplace turning */
void turn90(Dir dir, unsigned int speed) {
  left.write((STOP_SPEED + speed) * getDir(dir));
  right.write((STOP_SPEED + speed) * getDir(dir));
}

/* turning on smoother curve */
void turnCurve(unsigned int slope, Dir dir, unsigned int speed = 50) {
  left.write((STOP_SPEED + speed + slope) * getDir(dir));
  right.write((STOP_SPEED - speed + slope) * getDir(dir));
}

void debugInfra() {
  for (int i = 0; i < sizeof(ir_sensors); ++i) {
    Serial.print(!ir_sensors[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void readInfra() {
  for (int i = 0; i < sizeof(ir_sensors); ++i) {
    ir_sensors[i] = !digitalRead(first_infra + i);
  }
}

bool markIsRight() {
  if (ir_sensors[4] && !ir_sensors[1] && !ir_sensors[3]) {
    return true;
  }
  return false;
}

bool markIsLeft() {
  if (ir_sensors[0] && !ir_sensors[1] && !ir_sensors[3]) {
    return true;
  }
  return false;
}
// slope 100 - 90deg turning
void rotateWhileSensor(bool on_line, int sensor_id, unsigned int slope, Dir dir,
                       unsigned int speed = 50) {
  while (ir_sensors[sensor_id] == on_line) {
    if (slope == 100)
      turn90(dir, speed);
    else
      turnCurve(slope, dir, speed);
    delay(5);
    readInfra();
  }
  stop();
}
void forwardWhileSensor(bool on_line, int sensor_id, int speed) {
  while (ir_sensors[sensor_id] == on_line) {
    forward(speed);
    delay(5);
    readInfra();
  }
  stop();
}

void doAction(void fce(), unsigned int ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    fce();
  }
}

bool splitRoads() {
  if (ir_sensors[0] || ir_sensors[4])
    return true;
  else
    return false;
}
bool mergeRoads(Dir dir) {
  // if (ir_sensors[0] && dir == RIGHT)
  //   return true;
  // if (ir_sensors[4] && dir == LEFT)
  //   return true;
  // return false;
  if ((ir_sensors[1] && ir_sensors[3]))
    return true;
  return false;
}
bool isTimeout() {
  if (timeout_ > 0) {
    --timeout_;
    return true;
  }
  return false;
}

void setup() {
  // initialize serial and wait for port to open:
  Serial.begin(115200);
  left.attach(12);
  right.attach(13);
  // button
  pinMode(button_pin, INPUT_PULLUP);
  // LED
  pinMode(LED_pin, OUTPUT);
  stop();
  // setup infa array
  for (int i = first_infra; i < first_infra + sizeof(ir_sensors); ++i) {
    pinMode(i, INPUT);
  }

  LEDOn();
  size_t presses = countPresses();
  Serial.println(presses);
  LEDOff();
  delay(300);
  LEDBlink(presses);
}
void execStandard() {
  if (find_middle) {
    if (ir_sensors[2])
      find_middle = false;
    else
      return;
  }

  // if (ir_sensors[0]) {
  //   turn90(-140);
  //   find_middle = true;
  //   delay(50);
  // } else if (ir_sensors[4]) {
  //   turn90(140);
  //   find_middle = true;
  //   delay(50);
  // } else
  if (ir_sensors[1]) {
    turnCurve(100, LEFT, 80);
    delay(50);
  } else if (ir_sensors[3]) {
    turnCurve(100, RIGHT, 80);
    delay(50);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
    turn90(LEFT, 30);
    // forward(50);
  }
}

void execStandardLeft() {
  readInfra();
  if (ir_sensors[3]) {
    turnCurve(100, RIGHT, 80);
    delay(50);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
    turn90(LEFT, 30);
    // forward(50);
  }
}

void execStandardRight() {
  readInfra();
  if (ir_sensors[1]) {
    turnCurve(100, LEFT, 80);
    delay(50);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
    turn90(LEFT, 30);
    // forward(50);
  }
}
void execStandardSensorRead() {
  readInfra();
  execStandard();
}
void execExpectingCrossroads() { forward(150); }

void execOnFork() { execStandard(); }
void execPassMarker() { execStandard(); }

void loop() {
  readInfra();
  // Serial.println(buttonDown());
  // debugInfra();
  if (mergeRoads(LEFT)) {
    LEDOn();
  } else {
    LEDOff();
  }
  if (buttonDown()) {
    start = true;
  }
  if (!start) {
    stop();
    return;
  }

  switch (state_) {
  case STANDARD:
    if (markIsLeft()) {
      timeout_ = timeout_delay;
      route_ = LEFT;
      state_ = PASS_MARKER;
      doAction(execStandard, 500);
    } else if (markIsRight()) {
      timeout_ = timeout_delay;
      route_ = RIGHT;
      state_ = PASS_MARKER;
      doAction(execStandard, 500);
    } else {
      state_ = STANDARD;
      execStandard();
    }
    break;
  case CROSS_EXPECTED:
    if (splitRoads()) {
      if (route_ == LEFT) {
        rotateWhileSensor(false, 4, 80, LEFT);
        rotateWhileSensor(true, 4, 80, LEFT);
        // rotateWhileSensor(false, 4, 80, LEFT);
        rotateWhileSensor(false, 2, 100, RIGHT);
      } else {
        rotateWhileSensor(false, 0, 80, RIGHT);
        rotateWhileSensor(true, 0, 80, RIGHT);
        // rotateWhileSensor(false, 0, 80, RIGHT);
        rotateWhileSensor(false, 2, 100, LEFT);
      }
      doAction(execStandardSensorRead, 1000);
      state_ = FORK;
    } else {
      state_ = CROSS_EXPECTED;
      execExpectingCrossroads();
    }
    break;
  case FORK:
    if (mergeRoads(route_)) {
      doAction(stop, 2000);
      if (route_ == RIGHT) {
        doAction(execStandardLeft, 500);
      } else {
        doAction(execStandardRight, 500);
      }
      // forwWhileSensor(true, 0, 100);  sssssdf

      // forwardWhileSensor(true, 4, 100);
      // doAction(forward, 300);
      doAction(stop, 2000);
      state_ = STANDARD;
      // if (route_ == LEFT) {
      //   forwardWhileSensor(false, 4, 150);
      //   rotateWhileSensor(false, 4, 100, LEFT);
      //   rotateWhileSensor(false, 2, 100, RIGHT);
      // } else {
      //   forwardWhileSensor(false, 0, 150);
      //   rotateWhileSensor(false, 0, 100, RIGHT);
      //   rotateWhileSensor(false, 2, 100, LEFT);
      // }
    } else {
      state_ = FORK;
      execOnFork();
    }
    break;

  case PASS_MARKER:
    if (markIsLeft() || markIsRight()) {
      state_ = PASS_MARKER;
      execStandard();
    } else {
      state_ = CROSS_EXPECTED;
      execExpectingCrossroads();
    }
    break;
  default:
    stop();
    break;
  }

  delay(10);
}
