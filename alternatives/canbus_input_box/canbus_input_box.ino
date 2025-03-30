//dependencies
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <mcp_can.h>

#include <FastLED.h>

//led configs
#define LED_PIN     4
#define NUM_LEDS    6
#define BRIGHTNESS  48
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
CRGB leds[NUM_LEDS];

// Digital inputs
#define DEBOUNCE_LOOPS 4 //after activation how long to remember state, used to avoid bouncy switches causing rapid input changes
uint8_t inputs[9] = {5, 6, 7, 8, 9, A3, A0, A1, A2}; //D10 -> A3 because A10 crashes if used in conjunction with ISP - change these to what order you want your inputs to be
uint8_t inputDebounceStates[9];


// CAN inputs
#define CAN0_INT 2
MCP_CAN CAN0(3);

//canbus address configs - make sure these are unique for your use cases
#define CANBUS_HEARTBEAT 0x200
#define CANBUS_RECEIVE_COLORS 0x2c5 //+2c6 and 2c7 because each message only contains 2 leds, 2c5 leds 0-1, 2c6 2-3 and 2c7 4-5 bytes go in order of R G B R G B for each led associated
#define CANBUS_RECEIVE_LIGHTMODE 0x2c4
#define CANBUS_SEND_BUTTONS 0x2d1

//canbus
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

//leds
uint8_t lightModes[NUM_LEDS] = {0};
CRGB desiredColors[NUM_LEDS] = {CRGB::Blue};
uint8_t blinkTimer;


unsigned long task1Interval = 500; // 200ms interval for keep alive frame
unsigned long task2Interval = 50;  // 50ms interval for value sending
unsigned long task3Interval = 20;  // 20ms interval for driving LEDs
unsigned long task4Interval = 20;  // 20ms interval for reading digital inputs

unsigned long task1Millis = 0;     // storage for millis counter
unsigned long task2Millis = 0;     // storage for millis counter
unsigned long task3Millis = 0;     // storage for millis counter
unsigned long task4Millis = 0;

void setup()
{
  FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    flashAll(CRGB::Green);
  } else {
    flashAll(CRGB::Red);
  } // init can bus : baudrate = 500k
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT); // set INT pin to be an input
  digitalWrite(CAN0_INT, HIGH);  // set INT pin high to enable interna pullup

  for(int i = 0;i<9;i++) {
    pinMode(inputs[i], INPUT_PULLUP);
  }
  
}
void flashAll(CRGB color) {
   // Turn the LED on, then pause
  for(int i = 0; i < 3; i++) {
    leds[0] = color;
    leds[1] = color;
    leds[2] = color;
    leds[3] = color;
    leds[4] = color;
    leds[5] = color;
    FastLED.show();
    delay(100);
    // Now turn the LED off, then pause
    leds[0] = CRGB::Black;
    leds[1] = CRGB::Black;
    leds[2] = CRGB::Black;
    leds[3] = CRGB::Black;
    leds[4] = CRGB::Black;
    leds[5] = CRGB::Black;
    FastLED.show();
    delay(100);
  }
}
void loop()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - task1Millis >= task1Interval) {
    task1Millis = currentMillis;
    SendKeepAlive();
  }

  if (currentMillis - task2Millis >= task2Interval) {
    task2Millis = currentMillis;
    SendDPIValues();
  }

  if (currentMillis - task3Millis >= task3Interval) {
    task3Millis = currentMillis;
    DriveLEDs();
  }

  if (currentMillis - task4Millis >= task4Interval) {
    task4Millis = currentMillis;
    readDigitals();
  }

  // read can buffer when interrupted and jump to canread for processing.
  if (!digitalRead(CAN0_INT))  // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);  // Read data: len = data length, buf = data byte(s)
    canRead();
  }
}

void canRead() {
 if (rxId == 0x2c4) { //led states
  for(int i = 0;i<NUM_LEDS;i++) {
    lightModes[i] = rxBuf[i];
  }
 }
 if (rxId == 0x2c5) { //leds 0, 1
  desiredColors[0] = CRGB(rxBuf[0], rxBuf[1], rxBuf[2]);
  desiredColors[1] = CRGB(rxBuf[3], rxBuf[4], rxBuf[5]);
 }
 if (rxId == 0x2c6) { //leds 2,3
  desiredColors[2] = CRGB(rxBuf[0], rxBuf[1], rxBuf[2]);
  desiredColors[3] = CRGB(rxBuf[3], rxBuf[4], rxBuf[5]);
 }
 if (rxId == 0x2c7) { //leds 4,5
  desiredColors[4] = CRGB(rxBuf[0], rxBuf[1], rxBuf[2]);
  desiredColors[5] = CRGB(rxBuf[3], rxBuf[4], rxBuf[5]);
 }
}

void SendKeepAlive() {
  byte KeepAlive[1] = { 0XFF };
  CAN0.sendMsgBuf(CANBUS_HEARTBEAT, 0, 1, KeepAlive);
}

void SendDPIValues()
{
  byte DPIdata0[5];
  
  DPIdata0[0] = (inputDebounceStates[0] > 0 ? 240 : 0) | (inputDebounceStates[1] > 0 ? 15 : 0);
  DPIdata0[1] = (inputDebounceStates[2] > 0 ? 240 : 0) | (inputDebounceStates[3] > 0 ? 15 : 0);
  DPIdata0[2] = (inputDebounceStates[4] > 0 ? 240 : 0) | (inputDebounceStates[5] > 0 ? 15 : 0);
  DPIdata0[3] = (inputDebounceStates[6] > 0 ? 240 : 0) | (inputDebounceStates[7] > 0 ? 15 : 0);
  DPIdata0[4] = (inputDebounceStates[8] > 0 ? 240 : 0);

  CAN0.sendMsgBuf(CANBUS_SEND_BUTTONS, 0, 5, DPIdata0);
}

void readDigitals() {
  for(uint8_t xx = 0;xx<9;xx++) {
    inputDebounceStates[xx] = digitalRead(inputs[xx]) == 0 ? DEBOUNCE_LOOPS + 1 : inputDebounceStates[xx];

    if(inputDebounceStates[xx] > 0) {
      inputDebounceStates[xx] = inputDebounceStates[xx]-1;
    }
  }
}

void DriveLEDs() {
  if(blinkTimer > 80) {
    blinkTimer = 0;
  } else {
    blinkTimer = blinkTimer+1;
  }
  for(uint8_t xd=0;xd<NUM_LEDS;xd++) {
    if(lightModes[xd] == 1) {
      leds[xd] = desiredColors[xd];
    } else if(lightModes[xd] == 2) {
      leds[xd] = blinkTimer <= 40 ? desiredColors[xd] : CRGB::Black;
    } else if(lightModes[xd] == 3) {
      leds[xd] = (blinkTimer % 20 <= 10) ? desiredColors[xd] : CRGB::Black;
    } else {
      leds[xd] = CRGB::Black;
    }
  }
  FastLED.show();
}


