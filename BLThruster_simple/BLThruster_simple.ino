#include <Servo.h>

Servo esc;

void setup() {
  // put your setup code here, to run once:
  esc.attach(2);
  esc.writeMicroseconds(1500);
  delay(5000);
}

void loop() {
  // put your main code here, to run repeatedly:
  esc.writeMicroseconds(1600);
  delay(1000);
  esc.writeMicroseconds(1500);
  delay(6000);
}
