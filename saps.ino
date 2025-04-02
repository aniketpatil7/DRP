// Motor Control Pins (Avoiding conflicts)
#define ENA 9   // PWM for Motor A
#define ENB 10  // PWM for Motor B
#define IN1 5   // Changed from original 7 to avoid conflict
#define IN2 6
#define IN3 7   // Changed from original 5
#define IN4 4

// Spray Control Pins
#define sprayPin 8  // Changed from 7 to avoid motor conflict
#define relayPin 13

// Timing Control
#define RUN_TIME 500     // Movement duration (ms)
#define STOP_TIME 4000   // Stop/spray window (ms)
#define SPRAY_DURATION 1000  // Spray active time (ms)

// State Management
enum SystemState { MOVING, STOPPED };
SystemState currentState = MOVING;
unsigned long stateStartTime = 0;
bool hasSprayed = false;

void setup() {
  // Motor control setup
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Spray control setup
  pinMode(sprayPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Start with motors moving
  startMoving();
}

void loop() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - stateStartTime;

  switch(currentState) {
    case MOVING:
      if(elapsedTime >= RUN_TIME) {
        stopMotors();
        currentState = STOPPED;
        stateStartTime = currentTime;
        hasSprayed = false;  // Reset spray flag
      }
      break;

    case STOPPED:
      if(!hasSprayed && digitalRead(sprayPin) == HIGH) {
        activateSpray();
        hasSprayed = true;
        stateStartTime = currentTime; // Reset timer after spraying
      }
      
      if(elapsedTime >= (hasSprayed ? SPRAY_DURATION : 0) + STOP_TIME) {
        startMoving();
        currentState = MOVING;
        stateStartTime = currentTime;
      }
      break;
  }
}

void startMoving() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 60);  // 60% speed
  analogWrite(ENB, 60);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void activateSpray() {
  digitalWrite(relayPin, HIGH);
  delay(SPRAY_DURATION);  // Blocks only during spraying
  digitalWrite(relayPin, LOW);
}