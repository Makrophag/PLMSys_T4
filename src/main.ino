// Libraries -------------------------------------------------------------------
#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include <Servo.h>

#include "BluefruitConfig.h"

#include <stdlib.h>
#include <Wire.h>
#include <Adafruit_VCNL4010.h>

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

#define FACTORYRESET_ENABLE         1
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// Global Variables ------------------------------------------------------------
// Jobs
enum jobs {
  JOB_IDLE,
  JOB_PICKUP,
  JOB_DROP_LEFT,
  JOB_DROP_RIGHT
};
int moduleJob;
int moduleJobState =0; // For Control inside Jobs

// Servopositions
int posPickup = 95;  // Position of the Servo when the Bridge is straight
int dropAngle = 30;   // Angle between pickup and drop
int servoSpeed = 10;  // delaytime in ms for Servo movements
int pickupTime = 5000;// Time to place a package before sending pickupSuccess(0)
int dropTime = 5000;  // Time to drop the package  before sending dropSuccess(0)

int posDropRight = posPickup+dropAngle;
int posDropLeft = posPickup-dropAngle;
int posServo = posPickup;
Servo myservo;

//Sensor
Adafruit_VCNL4010 vcnl;
bool package=0;

// setup ----------------------------------------------------------------------
void setup() {

  Serial.begin(9600);
  initBLE();
  ble.verbose(false);  // debug info is a little annoying after this point!

  // Wait for connection
  while (! ble.isConnected() || (Serial)) { // required for Flora & Micro
    //Serial.print("wait until connected to bluetooth. close Serial Monitor");
    delay(500);
  }
  /*
  while (ble.isConnected() || (! Serial)) { // required for Flora & Micro
    sendBLE("wait until connected to Serial. close Bluetooth");
    delay(500);
  }
  */
  initsensor();
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  moduleJob = JOB_IDLE;
  myservo.write(posPickup);


}//end of Setup


// loop -----------------------------------------------------------------------
void loop() {
  detectpackage();
  listenBLE();

  if (moduleJob != JOB_IDLE) {
    if (moduleJob == JOB_PICKUP){
      pickup();
    }
    if ((moduleJob == JOB_DROP_LEFT) || (moduleJob == JOB_DROP_RIGHT)){
      drop();
    }
  }
  delay(500);
}//end of loop()


// init -----------------------------------------------------------------------
void initBLE() {
  Serial.println(F("Adafruit Bluefruit Command Mode Example"));
  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));
  if ( ! ble.begin(VERBOSE_MODE) ){ //Bluetooth has to be Connected otherwise it stops here
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );
  if ( FACTORYRESET_ENABLE ){
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset. Performing Restart..."));
      while(1);
    }
  }
  /* Disable command echo from Bluefruit */
  ble.echo(false);
  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();
  ble.println("AT+GAPDEVNAME=BT_PLMSys_Team4"); // Sets the Name of Bluetooth Module
  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();
} // end of initBLE()

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1); //creates Watchdog Timeout and resets the System
}// end of error()

void initsensor() {
  if (!vcnl.begin()){
    Serial.println("Sensor not found");
    while (1); //creates Watchdog Timeout and resets the System
  }
  Serial.println("Found VCNL4010");
}// end of initsensor()

// Bluetooth -------------------------------------------------------------------
// Listen to incomminc commands from Bluetooth
void listenBLE() {
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if (strcmp(ble.buffer, "OK") == 0) {
    // no data
    return;
  }
  //sendBLE(ble.buffer);   // to check if received
  //Serial.println(ble.buffer);
  handleApiCommands(ble.buffer);
}//end of listenBLE()

// Send message over Bluetooth
void sendBLE(String msg) {
  ble.print("AT+BLEUARTTX=");
  ble.println(msg);
  // check response stastus
  if (! ble.waitForOK() ) {
    Serial.println(F("Failed to send?"));
  }
} //end of sendBLE


// Sensor   -------------------------------------------------------------------
void detectpackage() {
  if ((vcnl.readProximity() > 4000) && (package == 0)) {
    package = 1;
  }
  if((vcnl.readProximity() < 4000) && (package == 1)) {
    package = 0;
  }
}//end of detectpackage()


// Jobs -----------------------------------------------------------------------
// Handle API commands with string from listenBLE
void handleApiCommands(String command) {
  if (command == "pickup()") {
    moduleJob = JOB_PICKUP;
  }
  if (command == "drop(0)") {
    moduleJob = JOB_DROP_LEFT;
  }
  if (command == "drop(1)") {
    moduleJob = JOB_DROP_RIGHT;
  }
}//end of handleApiCommands

// Execute Jobs


void pickup() {
  if (moduleJobState == 0){
    servoTurn(posPickup);
    delay(pickupTime);
    moduleJobState = 1;
  }
  if (moduleJobState == 1){
    if (package == 0){
      sendBLE("pickupSuccess(0)");
      resetJob();
    }  else {
      sendBLE("pickupSuccess(1)");
      resetJob();
    }
  }
}//end of pickup()


void drop(){
  if (moduleJobState == 0){
    servoTurn(posPickup);
    moduleJobState = 1;
  }
  if ((moduleJobState == 1) && (package == 1)){
    if (moduleJob == JOB_DROP_LEFT){
    servoTurn(posDropLeft);
    delay(dropTime);
    }
    if (moduleJob == JOB_DROP_RIGHT){
    servoTurn(posDropRight);
    delay(dropTime);
    }
    detectpackage();
    moduleJobState = 2;
  }
  if ((moduleJobState == 2) && (package == 0)){
    sendBLE("dropSuccess(1)");
    resetJob();
  }
  if ((moduleJobState == 2) && (package == 1)){
    sendBLE("dropSuccess(0)");
    resetJob();
  }
}//end of drop()

void resetJob(){
  servoTurn(posPickup);
  moduleJobState=0;
  moduleJob = JOB_IDLE;
}//end of resetJob()


// Servo -----------------------------------------------------------------------
// turns Servo from actual Position to the endPosition
void servoTurn(int endPos){
  //turn clockwise
  if(endPos <= posServo || endPos == posServo){
    for (int i= posServo; i >= endPos; i--) {
      myservo.write(i);
      delay(servoSpeed);
      posServo = i;
    }
    return;
  }
  //turn counterclockwise
  if(endPos >= posServo){
    for (int i= posServo; i <= endPos; i++) {
      myservo.write(i);
      delay(servoSpeed);
      posServo = i;
    }
  }
  return;
}// end of servoTurn()
