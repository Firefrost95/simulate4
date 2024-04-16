#include <Arduino.h>
const int trigPin = 7; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21; // Echo Pin of Ultrasonic Sensor

long initialDistance = -1; // Variable to store the first measured distance

void setup() {
 Serial.begin(9600); // Starting Serial Terminal
}

void loop() {
 long duration, inches, cm; // Variables to make the calculation for measured distance.

 pinMode(trigPin, OUTPUT);
 digitalWrite(trigPin, LOW);
 delayMicroseconds(2);
 digitalWrite(trigPin, HIGH);
 delayMicroseconds(10);
 digitalWrite(trigPin, LOW);

 pinMode(echoPin, INPUT);
 duration = pulseIn(echoPin, HIGH);
 inches = microsecondsToInches(duration);
 cm = microsecondsToCentimeters(duration);

 // Check if this is the first measurement
 if (initialDistance == -1) {
    initialDistance = cm; // Store the first measured distance
 } else {
    // Compare the current measurement to the initial measurement
    if (cm == initialDistance) {
      // Trigger a ping if the measurement matches the initial measurement
      Serial.println("Ping!");
    }
 }

 Serial.print(inches);
 Serial.print(" in, ");
 Serial.print(cm);
 Serial.print(" cm");
 Serial.println();
 delay(100); // Delay 100ms before next reading.
}

long microsecondsToInches(long microseconds) {
 return microseconds / 74 / 2;
}

long microsecondsToCentimeters(long microseconds) {
 return microseconds / 29 / 2;
}
