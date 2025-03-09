/* 
This program is designed to automate making rock candy.
It handles the dispensing of water and sugar into a pot. Verifies the amount of water pumped.
Then heats the combined sugar water while stirring once the boiling point (heaterActivationTemp) is reached.
At this temperature, a 20 minute timer begins and the temperature is maintained within a range to not turn the relay on and off rapidly.
Once the countdown ends, a valve opens to drain the pot, the valve closes, and the program suspends to declare the program finished.
*/

#include <Servo.h>
#include "max6675.h"

// 48.8mL per cm
// FLOWRATE = 20ml/s
// Reserve water tabk height from ultrasonic sensor = 10.71cm

// Pin Assignments
const int servoPin = 9;            // Servo control pin
const int pumpRelayPin = A5;       // Pump relay pin 
const int heaterRelayPin = A0;     // Heater relay pin
const int valveRelayPin = A1;      // Valve relay pin
const int stirMotorPWN = 3;        // Motor PWM control pin
const int thermoCLK = 6;           // Thermocouple CLK pin
const int thermoCS = 5;            // Thermocouple CS pin
const int thermoDO = 4;            // Thermocouple DO pin
const int trigPin = 7;             // TRIG pin
const int echoPin = 8;             // ECHO pin
const int vibMotorPin = 10;

// Objects and Variables
Servo servo;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
const int servoStartPos = 85;            // Servo starting position (degrees)
const int servoEndPos = 130;             // Servo ending position (degrees)
const int servoMovements = 0;            // Number of servo movements
const int servoSpeedDelay = 0.5;          // Adjustable servo speed (ms per degree)
const int servoPauseDelay = 5000;         // Delay at end of rotation (ms)
const int CalcVolume_mL = 0;               // Pump runtime in secondsU
const int heaterActivationTemp = 100;     // Heater activation temperature (Celsius)
const int heaterDeactivationTemp = 110;   // Heater deactivation temperature (Celsius)
const int motorPWMValue = 0;             // Initial motor PWM value
const int masterDelay = 5000;            // Delay for every process in the heating loop
float countdownTime = 60*20; // Change right digit to change cooking time in minutes
int valveOpenTime = 60*3;                  //Change right digit to change time that valve stays open in minutes
float duration_us, measuredVolume_mL, distance_cm; //Ultrasonic sensor variables

//Initial off states of global bool definitions
bool heaterActivated = false;
bool servoTaskCompleted = false;
bool pumpTaskCompleted = false;
bool heatTaskCompleted = false;
bool heating = false;
bool heatTimerBegin = false;
bool heatTimerFinished = false;
bool countdownMilllisRecorded = false;

void setup() {
  Serial.begin(9600);

  //Pins
  pinMode(stirMotorPWN, OUTPUT);
  pinMode(pumpRelayPin, OUTPUT);
  pinMode(heaterRelayPin, OUTPUT);
  pinMode(valveRelayPin, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(trigPin, OUTPUT);  // Configure the trigger pin to output mode
  pinMode(echoPin, INPUT);   // Configure the echo pin to input mode


  //These relays are normally closed and while unpowered, change this if the relay is normally open
  digitalWrite(pumpRelayPin, HIGH); 
  digitalWrite(A4, HIGH); //This relay is on a 2 piece module with the pump relay, but does nothing in the project, so it is permamently off

  //Servo
  Serial.println("Enter how many times the servo should move back and forth:");
  servo.attach(servoPin);
  servo.write(servoStartPos);  // Prevents servo jerking on upload (sometimes)

}

void loop() {
  //Sequence based tasks, each act one at a time except heatTask and heatTimerTask act in series once temp > activationTemp
  //The end of every void function has a bool to end or start a task
  if (!pumpTaskCompleted) {pumpTask();}
  if (!servoTaskCompleted) {servoTask();}
  if (!heatTaskCompleted) {heatTask();}
  if (heatTimerBegin) {heatTimerTask();}
  if (heatTaskCompleted) {dispenseHotSugarWater();}
}

//Prints the countdown in the serial monitor and activates dispenseHotSugarWater() when countdownTime = 0
void heatTimerTask(){
  if (countdownTime >= masterDelay*2/1000) { //the *2/1000 is necessary to have the countdown initiate at countdownTime = 0 due to the delay
  countdownTime = countdownCalc();
  Serial.print("Seconds until valve opens: ");
  Serial.println(countdownTime); 
  } else {
   heatTaskCompleted = true;
   heatTimerBegin = false;
}}

//Does the iterative math for heatTimerTask()
int countdownCalc() {
  int x = countdownTime - masterDelay/1000;
  return x;
} 

//Opens valve for valveOpenTime seconds, disengages stir motor, closes valve when countdown is over, and suspends program indefinitely
void dispenseHotSugarWater (){
    Serial.println("Heating complete. Water is dispensing.");
    analogWrite(stirMotorPWN, 0);
    digitalWrite(valveRelayPin, HIGH);
      for (int i = 1; i <= valveOpenTime; i++) { // Serial monitor pump time progress
      delay(1000);
      char valveProgress[35];
      sprintf(valveProgress, "Valve Progress: %d / %d seconds", i, valveOpenTime);
      Serial.println(valveProgress);
      }
      //Finish program
        digitalWrite(valveRelayPin, LOW);
        Serial.println("Eat up fool");
        while (Serial.available() == 0) {  } //Suspend program indefinitely after completion (unless input given) 
        Serial.println("Congrats you activated the infinite valve loop!");
}

//Controls heating element and prints thermocouple data
void heatTask(){
  //Thermocouple reading
  float temp = thermocouple.readCelsius();
  Serial.print("Temperature (C) = ");
  Serial.print(temp);
  //Temperature set-reset latch for heater
  if (temp < heaterActivationTemp) {
    digitalWrite(heaterRelayPin, HIGH);
    heating = true;
    } else if (temp >= heaterDeactivationTemp) {
    digitalWrite(heaterRelayPin, LOW);
     heating = false;
   }
  //Heater status serial print (cannot be in set-reset latch loop because it won't print correctly)
  if (heating) {
    Serial.println(" - Heater ON");
  } else if (!heating) {
    Serial.println(" - Heater OFF");
  }
  delay (masterDelay);

  //Timer and stirMotor trigger when heaterActivationTemp < temp
  if (!heatTimerBegin && heaterActivationTemp < temp ){
    Serial.println("Valve exit countdown begin");
    analogWrite(stirMotorPWN, 100);
  heatTimerBegin = true;
}}

//Prompts user on how many times they want the servo to rotate back and fourth, each rotation should be 0.25fl oz in theory.
//Last rotation has the vibration motor go on for 30 seconds to clear the slide of the remaining sugar
void servoTask() {
  int servoMovements = getUserInput("Enter the number of rotations (0.25oz each):");
  if (servoMovements > -1) { //Must be a positive integer or else this skips
    Serial.print("Servo will move back and forth ");
    Serial.print(servoMovements);
    Serial.println(" times.");
    analogWrite(vibMotorPin, 255); //Vibrate the slide to help sugar move

    //Serial monitor servo movement progress
    for (int i = 0; i < servoMovements; i++) {

      //Servo movements serial print
      char servoMovePrint[30];
      sprintf(servoMovePrint, "Servo movements: %d / %d", i + 1, servoMovements);
      Serial.println(servoMovePrint);

      //Back and forth servo movements
      for (int pos = servoStartPos; pos <= servoEndPos; pos++) {
        servo.write(pos);
        delay(servoSpeedDelay);
      }
      delay(servoPauseDelay); //Pause inbetween each rotation to let sugar fall down
      for (int pos = servoEndPos; pos >= servoStartPos; pos--) {
        servo.write(pos);
        delay(servoSpeedDelay); //Pause inbetween...
      }
      delay(servoPauseDelay);
    }
    Serial.println("Vibrating to move remaining sugar...");
    delay(30000); //30 second final vibration time

  }
  // End task and start heatTask
  analogWrite(vibMotorPin, 0); // Stop vibrating slide
  Serial.println("Servo task completed.");
  servoTaskCompleted = true;
}

//Beginning task, prompts user to pump water into cooking pot by choice of mL wanted
void pumpTask() {
  //Measure amount of water in reserve
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration_us = pulseIn(echoPin, HIGH);
  distance_cm = 0.017 * duration_us; //speed of sound to cm
  measuredVolume_mL = 10.71*48.8 - distance_cm*48.8; //Height of rectangular water tank is 10.71cm, area is 48.8cm^2, bigger distance_cm means less water measured  
  Serial.print("Reserve Water Volume: ");
  Serial.print(measuredVolume_mL);
  Serial.print(" ml");
  //Begin pump sequence
  int CalcVolume_mL = getUserInput(" Enter desired volume (mL)"); //Prompt user for amount of water

  if (CalcVolume_mL > -1) { //Must be a positive integer or else this skips
    delay(1000);
    Serial.print("Pump will input ");
    Serial.print(CalcVolume_mL);
    Serial.println(" mL.");
    digitalWrite(pumpRelayPin, LOW); // Activate pump (active on LOW because the relay is freaky like that)

    for (int i = 0; i <= CalcVolume_mL; i++) { // Serial monitor pump time progress
      delay(50);
      char pumpProgress[35];
      sprintf(pumpProgress, "Pump Progress: %d / %u mL", i, CalcVolume_mL);
      Serial.println(pumpProgress);
    }
  }
  digitalWrite(pumpRelayPin, HIGH); // Deactivate pump
  //Begin sequence to compare calculated pumped volume with measured water removed (same as above but more steps)
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration_us = pulseIn(echoPin, HIGH);
  distance_cm = 0.017 * duration_us;
  float measuredVolume2_mL = 11.69*48.8 - distance_cm*48.8;
  float measuredVolDifference_mL = measuredVolume_mL - measuredVolume2_mL;
  float volumeDifference = ((measuredVolDifference_mL-CalcVolume_mL)/CalcVolume_mL)*100; //Should be an absolute value
  //I don't know why, but sprintf outputs the wrong variables so I have to do this the bad way
  //This information is mainly for trouble shooting anyway but I decided to leave it
  Serial.print("Volume error is: ");
  Serial.print(volumeDifference);
  Serial.print("%");
  Serial.print(" New measured volume: ");
  Serial.print(measuredVolume2_mL);
  Serial.print(" Measured volume difference: ");
  Serial.print(measuredVolDifference_mL);
  Serial.print(" mL");
  //Compares the calculated volume from the known pump flowrate with the measured volume from the ultrasonic sensor, error tolerance is 40%
  //Serial messages changes depending whether or the difference is within tolerance or not
  if (measuredVolDifference_mL < CalcVolume_mL*1.40 && measuredVolDifference_mL > CalcVolume_mL/1.40) {
    Serial.print(" - Error is tolerable.");
  }
  else {Serial.print(" - Error is not within tolerance, continuing anyways.");
}
  //Start next task
  pumpTaskCompleted = true;
}

//Function to suspend pump and servo task until user input's an integer
int getUserInput(const char* prompt) {
  Serial.println(prompt);
  while (Serial.available() == 0) {} //Wait for input
  int value = Serial.parseInt();  //Paste input into getUserInput
  // Clear the serial buffer after reading input
  while (Serial.available() > 0) {
    Serial.read();
  }
  return value;
}