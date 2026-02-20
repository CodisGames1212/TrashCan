#include <ESP32Servo.h>

const int trigPin = 2;
const int echoPin = 3;
const int servoPin = 10;

Servo myServo;

const int thresholdDistance = 20; // Unsa ka duol...e Adjust lang...hehehehe
const int openAngle = 90;         // Abli
const int closeAngle = 0;         // Sirado

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  myServo.attach(servoPin);
  myServo.write(closeAngle);
}

void loop() {
  long duration, distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration * 0.034) / 2;

  if (distance > 0 && distance < thresholdDistance) {
    Serial.println("Object Detected! Opening...");
    myServo.write(openAngle);
    delay(3000); // unsa ka dugay nga nag- abli ang takob
  } else {
    myServo.write(closeAngle);
  }
  
  delay(100);
}