#define ADC_SCALE 1023.0
#define VREF 5.0
#define DEFAULT_FREQUENCY 50

enum ACS712_type {ACS712_05B, ACS712_20A, ACS712_30A};

class ACS712 {
public:
  ACS712(ACS712_type type, uint8_t _pin);
  int calibrate();
  void setZeroPoint(int _zero);
  void setSensitivity(float sens);
  float getCurrentDC();
  float getCurrentAC(uint16_t frequency = 50);

private:
  int zero = 512;
  float sensitivity;
  uint8_t pin;
};

ACS712::ACS712(ACS712_type type, uint8_t _pin) {
  pin = _pin;

  // Different models of the sensor have their sensitivity:
  switch (type) {
    case ACS712_05B:
      sensitivity = 0.185;
      break;
    case ACS712_20A:
      sensitivity = 0.100;
      break;
    case ACS712_30A:
      sensitivity = 0.066;
      break;
  }
}

int ACS712::calibrate() {
  uint16_t acc = 0;
  for (int i = 0; i < 10; i++) {
    acc += analogRead(pin);
  }
  zero = acc / 10;
  return zero;
}

void ACS712::setZeroPoint(int _zero) {
  zero = _zero;
}

void ACS712::setSensitivity(float sens) {
  sensitivity = sens;
}

float ACS712::getCurrentDC() {
  int16_t acc = 0;
  for (int i = 0; i < 100; i++) {
    acc += analogRead(pin) - zero;
  }
  float I = (float)acc / 100.0 / ADC_SCALE * VREF / sensitivity;
  return I;
}

float ACS712::getCurrentAC(uint16_t frequency) {
  uint32_t period = 1000000 / frequency;
  uint32_t t_start = micros();

  uint32_t Isum = 0, measurements_count = 0;
  int32_t Inow;

  while (micros() - t_start < period) {
    Inow = analogRead(pin) - zero;
    Isum += Inow*Inow;
    measurements_count++;
  }

  float Irms = sqrt(Isum / measurements_count) / ADC_SCALE * VREF / sensitivity;
  return Irms;
}

/**
   The MySensors Arduino library handles the wireless radio link and protocol
   between your home built sensors/actuators and HA controller of choice.
   The sensors forms a self healing radio network with optional repeaters. Each
   repeater and gateway builds a routing tables in EEPROM which keeps track of the
   network topology allowing messages to be routed to nodes.

   Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
   Copyright (C) 2013-2015 Sensnology AB
   Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors

   Documentation: http://www.mysensors.org
   Support Forum: http://forum.mysensors.orgu

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

 *******************************

   REVISION HISTORY
   Version 1.0 - Henrik Ekblad

   DESCRIPTION
   Example sketch showing how to control physical relays.
   This example will remember relay state after power failure.
   http://www.mysensors.org/build/relay
*/

//send: 22-22-0-0 s=0,c=1,t=35,pt=4,l=4,sg=0,st=ok:54
//read: 0-0-22 s=1,c=1,t=2,pt=2,l=2,sg=0:0
//Incoming change for sensor:1, New status: 0
//send: 22-22-0-0 s=0,c=1,t=35,pt=4,l=4,sg=0,st=fail:55
//send: 22-22-0-0 s=0,c=1,t=35,pt=4,l=4,sg=0,st=ok:56
//read: 0-0-22 s=1,c=1,t=2,pt=2,l=2,sg=0:1
//Incoming change for sensor:1, New status: 1
//send: 22-22-0-0 s=0,c=1,t=35,pt=4,l=4,sg=0,st=ok:57

#define MY_DEBUG
#define MY_RADIO_NRF24
#define MY_NODE_ID 2
#define MY_RF24_SANITY_CHECK
#define MY_REPEATER_FEATURE


//#include <MySigningNone.h>
//#include <MyTransportNRF24.h>
//#include <MyTransportRFM69.h>
//#include <MyHwATMega328.h>
#include <MySensors.h>

#define LED_PIN  A0
#define LED_FEEDBACK A1

unsigned long SLEEP_TIME = 1000;

#define LED_SENSOR_NR 0
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay
#define RELAY_UNKNOWN 2

MyMessage ledMsg(LED_SENSOR_NR, V_LIGHT);
ACS712 sensor(ACS712_20A, A1);

static void registerLed();
static void printRegistrationMsg(const char* sensorName, unsigned int sensorNr);
static void checkLed();

void presentation()
{
  sendSketchInfo("Switch node", "1.0");
  present(LED_SENSOR_NR, S_LIGHT);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting switch node");

  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Switch node", "1.0");

  // Register all sensors to gw (they will be created as child devices)

  registerLed();
}

void loop()
{
  //static int statusSent = 0;
  char test[32];
  /*snprintf(test, 32, "%#lx", RF24_BASE_RADIO_ID);
  Serial.print("base address ");
  Serial.print(test);
  Serial.print("\n\r");*/

  // Alway process incoming messages whenever possible
  /*if(!statusSent)
  {
    int ledStatus = digitalRead(LED_FEEDBACK);
    send(ledMsg.setSensor(LED_SENSOR_NR).setType(V_LIGHT).set(ledStatus));
    statusSent = 1;
  }*/
  checkLed();
  wait(SLEEP_TIME);
}

/*
static void lightChanged()
{
  int ledStatus = digitalRead(LED_FEEDBACK);
  send(ledMsg.set(ledStatus));
}
*/

const int sensorIn = A1;
int mVperAmp = 100; // use 100 for 20A Module and 66 for 30A Module


double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;

static int getLedStatus() {
  #if 0
  // We use 230V because it is the common standard in European countries
  // Change to your local, if necessary
  float U = 230;

  // To measure current we need to know the frequency of current
  // By default 50Hz is used, but you can specify desired frequency
  // as first argument to getCurrentAC() method, if necessary
  float I = sensor.getCurrentAC(50);

  // To calculate the power we need voltage multiplied by current
  float P = U * I;

  Serial.println(String("V = ") + I + " Volts");
  Serial.println(String("I = ") + I + " A");
  Serial.println(String("P = ") + P + " Watts");
   return P > 30;
   #endif

  float result;
  
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  
   uint32_t start_time = millis();
   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(sensorIn);
       // see if you have a new maxValue
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the maximum sensor value*/
           minValue = readValue;
       }
   }
   
   // Subtract min from max
   result = ((maxValue - minValue) * 5.0)/1024.0;
   VRMS = (result/2.0) *0.707; 
   AmpsRMS = (VRMS * 1000)/mVperAmp;
   Serial.println(String("AmpsRMS = ") + AmpsRMS);

  return AmpsRMS > 0.15;
  //return 0;
}

static void checkLed()
{
  static int previousState = RELAY_UNKNOWN;
  static bool mustAdvertiseStatus = true;
  int ledStatus = getLedStatus();
  int ledPinStatus = digitalRead(LED_PIN);
  Serial.print("Checking led: ");
  Serial.print(ledStatus);
  Serial.print(" ");
  Serial.println(ledPinStatus);
  
  if (ledStatus != previousState)
  {
    Serial.print("1 new Led status is ");
    Serial.print(ledStatus);
    Serial.print("\n\r");
    previousState = ledStatus;
    mustAdvertiseStatus = true;
    //gw.saveState(LED_SENSOR_NR, ledStatus);
  }
  if(mustAdvertiseStatus)
  {
    if(send(ledMsg.setSensor(LED_SENSOR_NR).setType(V_LIGHT).set(ledStatus)))
    {
      mustAdvertiseStatus = false;
    }
  }
}

void receive(const MyMessage &message) {
  Serial.print("Incoming change for sensor msgType ");
  Serial.print(message.type);
  Serial.print(" value ");
  Serial.print(message.getBool());
  Serial.print("\n\r");
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type == V_LIGHT) {
    // Change relay state
    Serial.print("Incoming change for sensor:");
    Serial.print(message.sensor);
    Serial.print(", New status: ");
    Serial.println(message.getBool());
    //int ledStatus = digitalRead(LED_FEEDBACK);
    int ledStatus = getLedStatus();
    Serial.print("Led status status: ");
    Serial.println(ledStatus);
    int msgContent = message.getBool() ? RELAY_ON : RELAY_OFF;
    if (ledStatus != msgContent)
    {
      int toWrite = digitalRead(LED_PIN);
      Serial.print("Current LED_PIN status: ");
      Serial.println(toWrite);
      toWrite = !toWrite;
      Serial.print("Writing into LED_PIN: ");
      Serial.println(toWrite);
      digitalWrite(LED_PIN, toWrite);
      //gw.saveState(LED_SENSOR_NR, ledStatus);
    }
    else
    {
      Serial.println("nothing to do");
    }
    // Write some debug info
  }
}

static void registerLed()
{
  pinMode(LED_PIN, OUTPUT);
  //pinMode(LED_FEEDBACK, INPUT);

  //switch off the light by default
  int ledStatus = getLedStatus();
  if(ledStatus)
  {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

static void printRegistrationMsg(const char* sensorName, unsigned int sensorNr)
{
  Serial.print("registering sensor ");
  Serial.print(sensorName);
  Serial.print(" as number: ");
  Serial.println(sensorNr);
}



