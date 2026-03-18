#define ML_Ctrl 13
#define ML_PWM 11
#define MR_Ctrl 12
#define MR_PWM 3

const int throttlePin = 10;   // Throttle input PWM (channel 3)
const int steeringPin = 9;    // Steering input PWM (channel 1)
const int aux6Pin = 8;        // Mode switch input PWM (channel 6)

const int DEADZONE_US = 30;

unsigned long lastValidSignalTime = 0;
const unsigned long signalTimeout = 200; // ms

unsigned long lastThrottlePulse = 1500;
unsigned long lastSteeringPulse = 1500;
unsigned long lastAux6Pulse = 1500;      // Store last channel 6 pulse

// Motor speed calibration factors
float leftMotorCal = 1.1;
float rightMotorCal = 2.0;

// Left motor boost multiplier during turns
float leftTurnBoost = 1.3; // Adjust this between 1.0 and ~1.5 as needed

void setup() {
  Serial.begin(9600);
  pinMode(throttlePin, INPUT);
  pinMode(steeringPin, INPUT);
  pinMode(aux6Pin, INPUT);
  pinMode(ML_Ctrl, OUTPUT);
  pinMode(ML_PWM, OUTPUT);
  pinMode(MR_Ctrl, OUTPUT);
  pinMode(MR_PWM, OUTPUT);
}

int mapSpeed(unsigned long pulse) {
  if (pulse > (1500 - DEADZONE_US) && pulse < (1500 + DEADZONE_US))
    return 0;
  pulse = constrain(pulse, 1000, 2000);
  return map(pulse, 1000, 2000, -255, 255);
}

void driveMotor(int ctrlPin, int pwmPin, int speed, float cal) {
  speed = (int)(speed * cal);
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    digitalWrite(ctrlPin, LOW);
    analogWrite(pwmPin, speed);
  } else if (speed < 0) {
    digitalWrite(ctrlPin, HIGH);
    analogWrite(pwmPin, -speed);
  } else {
    analogWrite(pwmPin, 0);
    digitalWrite(ctrlPin, LOW);
  }
}

void stopAllMotors() {
  analogWrite(ML_PWM, 0);
  analogWrite(MR_PWM, 0);
  digitalWrite(ML_Ctrl, LOW);
  digitalWrite(MR_Ctrl, LOW);
}

void mixSteering(int throttle, int steering, int &leftSpeed, int &rightSpeed) {
  int maxInput = max(abs(throttle), abs(steering));
  int sum = throttle + steering;
  int diff = throttle - steering;

  if (throttle >= 0) {
    if (steering >= 0) {
      leftSpeed = maxInput;
      rightSpeed = diff;
    } else {
      leftSpeed = sum;
      rightSpeed = maxInput;
    }
  } else {
    if (steering >= 0) {
      leftSpeed = sum;
      rightSpeed = -maxInput;
    } else {
      leftSpeed = -maxInput;
      rightSpeed = diff;
    }
  }

  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);
}

void loop() {
  unsigned long currentMillis = millis();

  unsigned long throttlePulse = pulseIn(throttlePin, HIGH, 25000);
  unsigned long steeringPulse = pulseIn(steeringPin, HIGH, 25000);
  unsigned long aux6Pulse = pulseIn(aux6Pin, HIGH, 25000);

  if (throttlePulse >= 900 && throttlePulse <= 2100 && abs((long)throttlePulse - (long)lastThrottlePulse) > 5) {
    lastValidSignalTime = currentMillis;
    lastThrottlePulse = throttlePulse;
  }

  if (steeringPulse >= 900 && steeringPulse <= 2100 && abs((long)steeringPulse - (long)lastSteeringPulse) > 5) {
    lastValidSignalTime = currentMillis;
    lastSteeringPulse = steeringPulse;
  }

  if (aux6Pulse >= 900 && aux6Pulse <= 2100 && abs((long)aux6Pulse - (long)lastAux6Pulse) > 5) {
    lastValidSignalTime = currentMillis;
    lastAux6Pulse = aux6Pulse;
  }

  if (currentMillis - lastValidSignalTime > signalTimeout) {
    lastThrottlePulse = 1500;
    lastSteeringPulse = 1500;
    stopAllMotors();
    Serial.println("Fail-safe active: no valid signal detected.");
    delay(50);
    return;
  }

  // Determine if tank mode is active by channel 6 switch pulse width
  bool tankModeActive = (lastAux6Pulse > 1600); // adjust threshold as needed

  if (tankModeActive) {
    int throttleSpeed = mapSpeed(lastThrottlePulse);
    int steeringSpeed = mapSpeed(lastSteeringPulse);

    int leftSpeed, rightSpeed;
    mixSteering(throttleSpeed, steeringSpeed, leftSpeed, rightSpeed);

    if (steeringSpeed != 0) {
      leftSpeed = (int)(leftSpeed * leftTurnBoost);
      leftSpeed = constrain(leftSpeed, -255, 255);
    }

    if (steeringSpeed != 0 && abs(leftSpeed) < abs(rightSpeed)) {
      leftSpeed = (leftSpeed < 0 ? -1 : 1) * abs(rightSpeed);
    }

    driveMotor(ML_Ctrl, ML_PWM, leftSpeed, leftMotorCal);
    driveMotor(MR_Ctrl, MR_PWM, rightSpeed, rightMotorCal);

    Serial.print("Tank Mode ON - Throttle: ");
    Serial.print(throttleSpeed);
    Serial.print(" Steering: ");
    Serial.print(steeringSpeed);
    Serial.print(" LeftSpeed: ");
    Serial.print(leftSpeed);
    Serial.print(" RightSpeed: ");
    Serial.println(rightSpeed);

  } else {
    stopAllMotors();
    Serial.println("Tank Mode OFF");
  }

  delay(30);
}
