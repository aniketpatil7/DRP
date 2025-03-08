#define ENA 9
#define ENB 10
#define IN1 7
#define IN2 6
#define IN3 5
#define IN4 4

void setup() {
    pinMode(ENA, OUTPUT);
    pinMode(ENB, OUTPUT);
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);

    analogWrite(ENA, 80); // Set speed (0-255)
    analogWrite(ENB, 80);

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
}

void loop() {
    // Robot will keep moving forward
}
//hello