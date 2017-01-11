#include <Arduino.h>
#include <Servo.h>

#define button_pin 2
#define LED_pin 11
#define first_infra 3
#define timeout_delay 1000
#define STOP_SPEED 1500
#define DIST_PASS_MARK 500
#define DIST_PASS_MERGE 900
#define DIST_PASS_FORK 900
#define COMPENSATION_DELAY 50
#define SPLIT_CURVE 60
#define SPLIT_SPEED 30
#define MOVE_CURVE 100
#define MOVE_SPEED 80
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
bool turn_based_on_marks = true;
Dir last_dir = LEFT;

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
bool mergeRoads() {
  if ((ir_sensors[1] && ir_sensors[3]) || (ir_sensors[2] && ir_sensors[4]) ||
      (ir_sensors[2] && ir_sensors[0]))
    //  if ((ir_sensors[1] && ir_sensors[3]))
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

bool shakeOnMergeRoads() {
  if (ir_sensors[0] || ir_sensors[4]) {
    unsigned long start = millis();
    while (millis() - start < 200) {
      turnCurve(100, RIGHT);
      readInfra();
      if (mergeRoads())
        return true;
    }
    start = millis();
    while (millis() - start < 400) {
      turnCurve(100, LEFT);
      readInfra();
      if (mergeRoads())
        return true;
    }
    start = millis();
    while (millis() - start < 200) {
      turnCurve(100, RIGHT);
      readInfra();
      if (mergeRoads())
        return true;
    }
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
  turn_based_on_marks = true;
  if (presses == 2) {
    turn_based_on_marks = false;
  }
}
void execStandard() {
  if (find_middle) {
    if (ir_sensors[2])
      find_middle = false;
    else
      return;
  }
  if (ir_sensors[1]) {
    turnCurve(MOVE_CURVE, LEFT, MOVE_SPEED);
    delay(COMPENSATION_DELAY);
    last_dir = LEFT;
  } else if (ir_sensors[3]) {
    turnCurve(MOVE_CURVE, RIGHT, MOVE_SPEED);
    last_dir = RIGHT;
    delay(COMPENSATION_DELAY);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
           //  if (last_dir == LEFT)
    turn90(LEFT, 30);
    //  else
    //    turn90(RIGHT, 30);
  }
}

void execStandardLeft() {
  readInfra();
  if (ir_sensors[3]) {
    turnCurve(MOVE_CURVE, RIGHT, MOVE_SPEED);
    last_dir = RIGHT;
    delay(COMPENSATION_DELAY);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
    turn90(LEFT, 30);
    last_dir = LEFT;
  }
}

void execStandardRight() {
  readInfra();
  if (ir_sensors[1]) {
    turnCurve(MOVE_CURVE, LEFT, MOVE_SPEED);
    last_dir = LEFT;
    delay(COMPENSATION_DELAY);
  } else if (ir_sensors[2]) {
    forward(50);
  } else { // finding line
    turn90(RIGHT, 30);
    last_dir = RIGHT;
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
  if (mergeRoads()) {
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
      doAction(stop, 2000);
      // select route based on marker side or oposite if it is configured in
      // setup
      // phase
      if (turn_based_on_marks)
        route_ = LEFT;
      else
        route_ = RIGHT;
      state_ = PASS_MARKER;
      doAction(execStandardSensorRead, DIST_PASS_MARK);
    } else if (markIsRight()) {
      doAction(stop, 2000);
      // select route based on marker side or oposite if it is configured in
      // setup
      // phase
      if (turn_based_on_marks)
        route_ = RIGHT;
      else
        route_ = LEFT;
      state_ = PASS_MARKER;
      doAction(execStandardSensorRead, DIST_PASS_MARK);
    } else {
      state_ = STANDARD;
      execStandard();
    }
    break;
  case CROSS_EXPECTED:
    if (splitRoads()) {
      doAction(stop, 2000);
      if (route_ == LEFT) {
        rotateWhileSensor(false, 4, SPLIT_CURVE, LEFT, SPLIT_SPEED);
        rotateWhileSensor(true, 4, SPLIT_CURVE, LEFT, SPLIT_SPEED);
        rotateWhileSensor(false, 4, SPLIT_CURVE, LEFT, SPLIT_SPEED);
        rotateWhileSensor(false, 2, 100, RIGHT);
      } else {
        rotateWhileSensor(false, 0, SPLIT_CURVE, RIGHT, SPLIT_SPEED);
        rotateWhileSensor(true, 0, SPLIT_CURVE, RIGHT, SPLIT_SPEED);
        rotateWhileSensor(false, 0, SPLIT_CURVE, RIGHT, SPLIT_SPEED);
        rotateWhileSensor(false, 2, 100, LEFT);
      }
      doAction(execStandardSensorRead, DIST_PASS_FORK);
      doAction(stop, 2000);

      state_ = FORK;
    } else {
      state_ = CROSS_EXPECTED;
      execExpectingCrossroads();
    }
    break;
  case FORK:
    // this state is used when the robot detected crossroad and is on one of the
    // two possible paths (forks)
    if (mergeRoads()) {
      // two paths are merging together.
      if (route_ == RIGHT) {
        doAction(execStandardLeft, DIST_PASS_MERGE);
      } else {
        doAction(execStandardRight, DIST_PASS_MERGE);
      }
      doAction(stop, 2000);
      state_ = STANDARD;
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
