// Line Follower with 5 IR Sensors - Works with or without ESP32-CAM

#define FAR_LEFT_SENSOR   A0
#define LEFT_SENSOR       4
#define MIDDLE_SENSOR     7
#define RIGHT_SENSOR      9
#define FAR_RIGHT_SENSOR  8

#define LEFT_MOTOR        5
#define RIGHT_MOTOR       6

#define TRIG_PIN         A1
#define ECHO_PIN         A2

#define LEFT_ENCODER     2  // Interrupt 0
#define RIGHT_ENCODER    3  // Interrupt 1

#define ESP_RED_SIGNAL   12  // Pin to receive red detection signal from ESP32-CAM

// Speed settings (PWM: 0-255)
int baseSpeed = 85;
int turnSpeed = 00;
int slightTurn = 40;
int extremeTurn = 85;

// Obstacle detection
const int obstacleDistance = 10;  // Stop if obstacle is 10cm or closer (in cm)

// Encoder variables
volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;
const int pulsesPerRevolution = 20;
const float wheelCircumference = 21.0; // cm

// ESP32-CAM connection status
bool espConnected = false;
unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 5000; // Check every 5 seconds

void setup() {
  // Sensor setup
  pinMode(FAR_LEFT_SENSOR, INPUT);
  pinMode(LEFT_SENSOR, INPUT);
  pinMode(MIDDLE_SENSOR, INPUT);
  pinMode(RIGHT_SENSOR, INPUT);
  pinMode(FAR_RIGHT_SENSOR, INPUT);
  
  // Motor setup
  pinMode(LEFT_MOTOR, OUTPUT);
  pinMode(RIGHT_MOTOR, OUTPUT);
  
  // Ultrasonic setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Encoder setup
  pinMode(LEFT_ENCODER, INPUT_PULLUP);
  pinMode(RIGHT_ENCODER, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LEFT_ENCODER), leftEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENCODER), rightEncoderISR, CHANGE);
  
  // ESP32-CAM communication setup
  pinMode(ESP_RED_SIGNAL, INPUT_PULLUP); // Use pullup in case ESP is disconnected
  
  Serial.begin(115200);
  stopMotors();
  
  // Initial ESP connection check
  checkESPConnection();
}

void loop() {
  // Periodically check ESP connection status
  if (millis() - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    checkESPConnection();
    lastConnectionCheck = millis();
  }

  // Only check for red signal if ESP is connected
  if (espConnected) {
    if (digitalRead(ESP_RED_SIGNAL) == HIGH) {
      perform180Turn();
      delay(1000); // Wait after turning
      return; // Skip rest of loop for this iteration
    }
  }

  // Obstacle detection (works regardless of ESP connection)
  if (checkObstacle()) {
    stopMotors();
    delay(100);
    return;
  }
  
  // Line following logic (independent of ESP)
  bool farLeft = !digitalRead(FAR_LEFT_SENSOR);
  bool left = !digitalRead(LEFT_SENSOR);
  bool middle = !digitalRead(MIDDLE_SENSOR);
  bool right = !digitalRead(RIGHT_SENSOR);
  bool farRight = !digitalRead(FAR_RIGHT_SENSOR);

  if (farLeft && !farRight) {
    extremeRight();
  }
  else if (farRight && !farLeft) {
    extremeLeft();
  }
  else if (left && !right) {
    turnRight();
  }
  else if (right && !left) {
    turnLeft();
  }
  else if (middle) {
    if (left && right) {
      moveForward();
    }
    else if (left) {
      slightRight();
    }
    else if (right) {
      slightLeft();
    }
    else {
      moveForward();
    }
  }
  else if (farLeft && farRight) {
    moveForward();
    delay(200);
  }
  else {
    stopMotors();
  }
  
  // Only try to send data if ESP is connected
  if (espConnected) {
    sendEncoderData();
  }
  
  delay(10);
}

// Check if ESP32-CAM is connected
void checkESPConnection() {
  // Try to communicate with ESP
  Serial.println("<PING>"); // Send ping message
  
  // Wait for response with timeout
  unsigned long startTime = millis();
  bool receivedResponse = false;
  
  while (millis() - startTime < 200) { // 200ms timeout
    if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      if (response.startsWith("<PONG>")) {
        receivedResponse = true;
        break;
      }
    }
  }
  
  espConnected = receivedResponse;
  
  if (espConnected) {
    Serial.println("<STATUS,ESP Connected>");
  } else {
    Serial.println("<STATUS,ESP Not Connected>");
    // Ensure signal pin is in safe state
    digitalWrite(ESP_RED_SIGNAL, LOW);
  }
}

// Encoder ISRs
void leftEncoderISR() {
  leftEncoderCount++;
}

void rightEncoderISR() {
  rightEncoderCount++;
}

// Send encoder data to ESP-CAM
void sendEncoderData() {
  static unsigned long lastSendTime = 0;
  const unsigned long SEND_INTERVAL = 100; // ms
  
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    Serial.print("<ENCODER,");
    Serial.print(leftEncoderCount);
    Serial.print(",");
    Serial.print(rightEncoderCount);
    Serial.print(",");
    Serial.print(millis());
    Serial.println(">");
    lastSendTime = millis();
  }
}

// 180-degree turn
void perform180Turn() {
  // If ESP is disconnected, don't perform turn
  if (!espConnected) return;
  
  analogWrite(LEFT_MOTOR, extremeTurn);
  analogWrite(RIGHT_MOTOR, 0);
  delay(500); // Adjust based on your robot's turning speed
  stopMotors();
  
  // Send turn completion message
  if (espConnected) {
    Serial.println("<TURN,180>");
  }
}

// Obstacle detection
bool checkObstacle() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2;
  
  return (distance > 0 && distance <= obstacleDistance);
}

// Motor control functions
void moveForward() {
  analogWrite(LEFT_MOTOR, baseSpeed);
  analogWrite(RIGHT_MOTOR, baseSpeed);
}

void slightLeft() {
  analogWrite(LEFT_MOTOR, slightTurn);
  analogWrite(RIGHT_MOTOR, baseSpeed);
}

void slightRight() {
  analogWrite(LEFT_MOTOR, baseSpeed);
  analogWrite(RIGHT_MOTOR, slightTurn);
}

void turnLeft() {
  analogWrite(LEFT_MOTOR, 0);
  analogWrite(RIGHT_MOTOR, baseSpeed);
}

void turnRight() {
  analogWrite(LEFT_MOTOR, baseSpeed);
  analogWrite(RIGHT_MOTOR, 0);
}

void extremeLeft() {
  analogWrite(LEFT_MOTOR, 0);
  analogWrite(RIGHT_MOTOR, extremeTurn);
}

void extremeRight() {
  analogWrite(LEFT_MOTOR, extremeTurn);
  analogWrite(RIGHT_MOTOR, 0);
}

void stopMotors() {
  analogWrite(LEFT_MOTOR, 0);
  analogWrite(RIGHT_MOTOR, 0);
}