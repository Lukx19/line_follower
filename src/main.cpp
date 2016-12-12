#include <Arduino.h>
#include <Servo.h>

#define button_pin 2
#define first_infra 3
#define timeout_delay 10
Servo left;
Servo right;

bool ir_sensors[5];
bool start = false;
bool find_middle = false;
size_t timeout_ = 0;
int dir_;
enum State { STANDARD, FORK, CROSS_EXPECTED };
State state_;

bool buttonDown() { return !digitalRead(button_pin); }

void forward(int speed = 200) {
  left.write(1500 + speed);
  right.write(1500 - speed);
}

void backward(int speed = 200) {
  left.write(1500 - speed);
  right.write(1500 + speed);
}

void stop() {
  left.write(1500);
  right.write(1500);
}

/* inplace turning */
void turn90(int slope) {
  left.write(1500 + slope);
  right.write(1500 + slope);
}

/* turning on smoother curve */
void turnCurve(int slope, int speed = 200) {
  left.write(1500 + speed + slope);
  right.write(1500 - speed + slope);
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
  if (ir_sensors[2] && ir_sensors[4] || ir_sensors[3] && ir_sensors[4]) {
    return true;
  }
  return false;
}

bool markIsLeft() {
  if (ir_sensors[2] && ir_sensors[0] || ir_sensors[3] && ir_sensors[0]) {
    return true;
  }
  return false;
}

bool crossRoads() {
  size_t active_sensors = 0;
  for (size_t i = 0; i < 5; ++i) {
    if (ir_sensors[i]) {
      ++active_sensors;
    }
  }
  if (active_sensors > 2)
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

// /* indicates if any of the sensors is on the line */
// bool lineAhead() {
//   bool res = 0;
//   for (int i = 0; i < sizeof(ir_sensors); ++i) {
//     res |= !ir_sensors[i];
//   }
//   return res;
// }

void setup() {
  // initialize serial and wait for port to open:
  Serial.begin(115200);
  left.attach(12);
  right.attach(13);
  // button
  pinMode(button_pin, INPUT_PULLUP);

  stop();
  // setup infa array
  for (int i = first_infra; i < first_infra + sizeof(ir_sensors); ++i) {
    pinMode(i, INPUT);
  }
}

void stateStandard() {
  if (find_middle) {
    if (ir_sensors[2])
      find_middle = false;
    else
      return;
  }

  if (ir_sensors[0]) {
    turn90(-140);
    find_middle = true;
    delay(50);
  } else if (ir_sensors[4]) {
    turn90(140);
    find_middle = true;
    delay(50);
  } else if (ir_sensors[1]) {
    turnCurve(-30, 50);
  } else if (ir_sensors[3]) {
    turnCurve(30, 50);
  } else if (ir_sensors[2]) {
    forward(150);
  } else {
    // finding line
    // turn90(-30);
    forward(50);
  }
}

void stateExpectingCrossroads() {
  timeout_ = timeout_delay;
  stateStandard();
}

void stateOnFork() { stop(); }

void loop() {
  readInfra();
  Serial.println(buttonDown());
  debugInfra();

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
      dir_ = -1;
      state_ = CROSS_EXPECTED;
      stateExpectingCrossroads();
    } else if (markIsRight()) {
      timeout_ = timeout_delay;
      dir_ = 1;
      state_ = CROSS_EXPECTED;
      stateExpectingCrossroads();
    } else {
      state_ = STANDARD;
      stateStandard();
    }
    break;
  case CROSS_EXPECTED:
    if (crossRoads()) {
      state_ = FORK;
      // move straight and
      forward(150);

      stateOnFork();
    } else {
      stateExpectingCrossroads();
    }
    break;
  case FORK:
    if (crossRoads()) {
      state_ = STANDARD;
      stateStandard();
    } else {
      stateOnFork();
    }
    break;
  }

  delay(10);
}
