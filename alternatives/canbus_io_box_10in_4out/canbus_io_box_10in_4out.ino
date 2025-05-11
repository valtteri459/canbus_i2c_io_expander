//dependencies
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <mcp_can.h>

// Digital inputs
#define DEBOUNCE_LOOPS 4 //after activation how long to remember state, used to avoid bouncy switches causing rapid input changes
uint8_t inputs[8] = {3, 4, 5, 6, 7, 8, 9, A4};
uint8_t inputDebounceStates[8];

// Digital outputs
uint8_t outputs[4] = {A0, A1, A2, A3};
bool outputStates[4] = {0, 0, 0, 0};

// CAN inputs
#define CAN0_INT 2
MCP_CAN CAN0(10);

//canbus address configs - make sure these are unique for your use cases
#define CANBUS_HEARTBEAT 0x200
#define CANBUS_RECEIVE_OUTPUTS 0x2c5
#define CANBUS_SEND_INPUTS 0x2c3

//canbus
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

// Serial Output String Buffer
char msgString[128];

unsigned long task1Interval = 500; // 500ms interval for keep alive frame
unsigned long task2Interval = 50;  // 50ms interval for value sending
unsigned long task3Interval = 20;  // 20ms interval for driving outputs
unsigned long task4Interval = 20;  // 20ms interval for reading digital inputs

unsigned long task1Millis = 0;     // storage for millis counter
unsigned long task2Millis = 0;     // storage for millis counter
unsigned long task3Millis = 0;     // storage for millis counter
unsigned long task4Millis = 0;

void setup()
{
  Serial.begin(115200);
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT); // set INT pin to be an input
  digitalWrite(CAN0_INT, HIGH);  // set INT pin high to enable interna pullup

  for(int i = 0;i<8;i++) {
    pinMode(inputs[i], INPUT_PULLUP);
  }
  for(int i = 0;i<4;i++) {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i], LOW);
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
    DriveOutputs();
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
 if (rxId == CANBUS_RECEIVE_OUTPUTS) {
  
  for(int i = 0;i<4;i++) {
    outputStates[i] = rxBuf[i] > 0;
  }
 }
}

void SendKeepAlive() {
  byte KeepAlive[1] = { 0XFF };
  CAN0.sendMsgBuf(CANBUS_HEARTBEAT, 0, 1, KeepAlive);
}

void SendDPIValues()
{
  byte DPIdata0[8] = {
    inputDebounceStates[0] > 0 ? 255 : 0,
    inputDebounceStates[1] > 0 ? 255 : 0,
    inputDebounceStates[2] > 0 ? 255 : 0,
    inputDebounceStates[3] > 0 ? 255 : 0,
    inputDebounceStates[4] > 0 ? 255 : 0,
    inputDebounceStates[5] > 0 ? 255 : 0,
    inputDebounceStates[6] > 0 ? 255 : 0,
    inputDebounceStates[7] > 0 ? 255 : 0
  };

  CAN0.sendMsgBuf(CANBUS_SEND_INPUTS, 0, 8, DPIdata0);
}

void readDigitals() {
  for(uint8_t xx = 0;xx<8;xx++) {
    inputDebounceStates[xx] = digitalRead(inputs[xx]) == 0 ? DEBOUNCE_LOOPS + 1 : inputDebounceStates[xx];

    if(inputDebounceStates[xx] > 0) {
      inputDebounceStates[xx] = inputDebounceStates[xx]-1;
    }
  }

  sprintf(msgString, "outputs: ");
  Serial.print(msgString);
  for(int i = 0;i<4;i++) {
    sprintf(msgString, " %.1X", outputStates[i] > 0);
    Serial.print(msgString);
  }
  Serial.write(" | inputs: ");
  for(int i=0;i<8;i++){
    sprintf(msgString, " %.1X", inputDebounceStates[i] > 0);
    Serial.print(msgString);
  }
  Serial.println();
}

void DriveOutputs() {
  for(int i = 0;i<4;i++) {
    digitalWrite(outputs[i], outputStates[i]);
  }
}


