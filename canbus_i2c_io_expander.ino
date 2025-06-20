//dependencies
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <mcp_can.h>

//custom progress bar characters for screen
uint8_t zero[] = { B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
uint8_t one[] = {
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000
};
uint8_t two[] = {
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000
};
uint8_t three[] = {
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100
};
uint8_t four[] = {
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110
};
uint8_t five[] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

uint8_t lambdaChar[] = {
  B00000,
  B11000,
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001
};
  
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

//canbus
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

//inputs
#define CAN0_INT 2
#define BTN0 8
#define BTN1 7
#define BTN2 6
#define BTN3 5

#define DEBOUNCE_LOOPS 16
//if input is high, these are always set to value defined in DEBOUNCE_LOOPS, and when input is low they will slowly decay to 0 = off

unsigned int DPI0in = 0;                   // storage for digital input value as number for debounce purposes
unsigned int DPI1in = 0;                   // storage for digital input value as number for debounce purposes
unsigned int DPI2in = 0;                   // storage for digital input value as number for debounce purposes
unsigned int DPI3in = 0;                   // storage for digital input value as number for debounce purposes

MCP_CAN CAN0(10);

int Lambda;
int MAP;
int IntakeTemp;
int CoolantTemp;
int RPM;
int OilPressure;
int EthanolContent;
int TEMP;
int Volt;
int FuelPressure;
bool CruiseState;
int VehicleSpeed;
int LockedCruiseSpeed;
int FuelFlow;
int FuelFlowAvg;
int FuelConsumption;
int FuelConsumptionAvg;
int EngineBayLidState;

unsigned long task0Interval = 1000; // 1s interval for screen updates
unsigned long task1Interval = 100; // 100ms interval for keep alive frame
unsigned long task2Interval = 20;  // 20ms interval for analogue value sending
unsigned long task3Interval = 20;  // 20ms interval for analogue value reading 
unsigned long task4Interval = 20;  // 20ms interval for reading digital input
unsigned long task0Millis = 0;     // storage for millis counter
unsigned long task1Millis = 0;     // storage for millis counter
unsigned long task2Millis = 0;     // storage for millis counter
unsigned long task3Millis = 0;     // storage for millis counter
unsigned long task4Millis = 0;     // storage for millis counter

char cruiseStates[7];
char chrstring4[4];

void setup()
{
  lcd.init();                      // initialize the lcd 
  lcd.setBacklight(1);
  
  lcd.createChar(0, zero);
  lcd.createChar(1, one);
  lcd.createChar(2, two);
  lcd.createChar(3, three);
  lcd.createChar(4, four);
  lcd.createChar(5, five);
  lcd.createChar(6, lambdaChar);
  lcd.home();
  lcd.setCursor(0,0);
  lcd.print("starting..        ");

  lcd.setCursor(0,1);
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    lcd.print("CANBUS OK");
  } else {
    lcd.print("CANBUS FAIL!!!!");
    delay(3000);
  } // init can bus : baudrate = 500k
  CAN0.setMode(MCP_NORMAL);
  pinMode(CAN0_INT, INPUT); // set INT pin to be an input
  digitalWrite(CAN0_INT, HIGH);  // set INT pin high to enable interna pullup


  pinMode(BTN0, INPUT);
  pinMode(BTN1, INPUT);
  pinMode(BTN2, INPUT);
  pinMode(BTN3, INPUT);

  delay(2000);
  
}

void loop()
{
  unsigned long currentMillis = millis();
  
  if(currentMillis - task0Millis >= task0Interval) {
    task0Millis = currentMillis;
    updateScreen();
  }

   // Execute task 1 every 100ms
  if (currentMillis - task1Millis >= task1Interval) {
    task1Millis = currentMillis;
    SendKeepAlive();
  }

  // Execute task 3 every 20 ms
  if (currentMillis - task3Millis >= task3Interval) {
    task3Millis = currentMillis;
    DriveDigitalPin();
  }

  // Execute task 4 every 20ms
  if (currentMillis - task4Millis >= task4Interval) {
    task4Millis = currentMillis;
    SendDPIValues();
  }

  // read can buffer when interrupted and jump to canread for processing.
  if (!digitalRead(CAN0_INT))  // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);  // Read data: len = data length, buf = data byte(s)
    canRead();
  }
}

void updateScreen() {
  lcd.setCursor(12,0);
  lcd.print("IAT");
  lcd.setCursor(12,1);
  lcd.print("Oil P.");
  lcd.setCursor(12,2);
  lcd.print("MAP");


  lcd.setCursor(0,0);
  lcd.print(((float)IntakeTemp/10), 1);
  lcd.print("   ");
  updateProgressBar(IntakeTemp+500, 1500, 0, 5, 5);
  lcd.setCursor(0,1);
  lcd.print(OilPressure/10);
  lcd.print("    ");
  updateProgressBar(OilPressure, 700, 1, 5, 5);
  lcd.setCursor(0,2);
  lcd.print(((float)MAP/10), 1);
  lcd.print("   ");
  updateProgressBar(MAP, 2400, 2, 5, 5);
  lcd.setCursor(0,3);
  sprintf(chrstring4, "%4i", RPM);
  lcd.print(chrstring4);
  lcd.print("RPM");
  lcd.print((float)Lambda/1000, 2);
  lcd.write(6);
  lcd.print(" ");
  if(CruiseState > 0) {
    sprintf(cruiseStates,
      "%i%s%i", 
      ((int)LockedCruiseSpeed/10),
      LockedCruiseSpeed > VehicleSpeed ? "-" : "+",
      abs((int)(VehicleSpeed-LockedCruiseSpeed)/10)
    );
    lcd.print(cruiseStates);
  } else {
    lcd.print("       ");
  }
  lcd.setCursor(16,2);
  dtostrf((float)FuelConsumption/10, 4, 1, chrstring4);
  //sprintf(chrstring4, "%2.1f", (float)FuelFlow/10);
  lcd.print(chrstring4);
  lcd.setCursor(19,1);
  lcd.print(EngineBayLidState > 1 ? "X" : " ");
  /*
  //lcd.setCursor(17,2);
  //lcd.print((int)VehicleSpeed/10);
  lcd.setCursor(17,3);
  if( > 200) { //dont show values under 20kph
  
    lcd.print((int)LockedCruiseSpeed/10);
  } else {
    lcd.print("   ");
  }*/
}

void canRead() {
 if (rxId == 0x530) { //0x530
    //Volt
    Volt = ((int)rxBuf[1] << 8) | rxBuf[0]; //scale 100x

    //IntakeTemp
    IntakeTemp = ((int)rxBuf[5] << 8) | rxBuf[4]; //scale 10x

    //CoolantTemp
    CoolantTemp = ((int)rxBuf[7] << 8) | rxBuf[6]; //scale 10x
  
  } else if (rxId == 0x537) {
    FuelPressure = ((int)rxBuf[1] << 8) | rxBuf[0]; //scale 10x
  } else if (rxId == 0x520) {
    MAP = ((int)rxBuf[5] << 8) | rxBuf[4]; //scale 10x
    Lambda = ((int)rxBuf[7] << 8) | rxBuf[6]; //scale 1000x
    RPM = ((int)rxBuf[1] << 8) | rxBuf[0];
  } else if (rxId == 0x536) {
    OilPressure = ((int)rxBuf[5] << 8) | rxBuf[4]; //scale 10x
  } else if (rxId == 0x531) {
    EthanolContent =((int)rxBuf[3] << 8) | rxBuf[2]; //scale 10x
  } else if (rxId == 0x7B) {
    CruiseState = ((int)rxBuf[5] << 8) | rxBuf[4];
    VehicleSpeed = ((int)rxBuf[1] << 8) | rxBuf[0]; 
    LockedCruiseSpeed =((int)rxBuf[3] << 8) | rxBuf[2];
  } else if (rxId == 0x7C) {
    FuelFlow = ((int)rxBuf[1] << 8) | rxBuf[0];
    FuelFlowAvg = ((int)rxBuf[3] << 8) | rxBuf[2];
    FuelConsumption = ((int)rxBuf[5] << 8) | rxBuf[4];
    FuelConsumptionAvg = ((int)rxBuf[7] << 8) | rxBuf[6];
  } else if (rxId == 0x7D) {
    EngineBayLidState = ((int)rxBuf[1] << 8) | rxBuf[0];
  }
}

void DriveDigitalPin() {

}

void SendKeepAlive() {
  byte KeepAlive[5] = { 0X10, 0x09, 0x0a, 0x00, 0x00 };
  CAN0.sendMsgBuf(0x2C7, 0, 5, KeepAlive);
}

void SendDPIValues()
{
  byte DPIdata0[8];

  DPI0in = digitalRead(BTN0) ? DEBOUNCE_LOOPS : DPI0in;
  DPI1in = digitalRead(BTN1) ? DEBOUNCE_LOOPS : DPI1in;
  DPI2in = digitalRead(BTN2) ? DEBOUNCE_LOOPS : DPI2in;
  DPI3in = digitalRead(BTN3) ? DEBOUNCE_LOOPS : DPI3in;
  
  //add/remove the /* from the following row to enable or disable button debug squares from bottom right of screen
  
  lcd.setCursor(16,0);
  lcd.write(DPI0in > 0 ? 5 : 0);
  lcd.write(DPI1in > 0 ? 5 : 0);
  lcd.write(DPI2in > 0 ? 5 : 0);
  lcd.write(DPI3in > 0 ? 5 : 0);
  //lcd.print(DPI0in);
  //lcd.print("   ");
  //**/

  if(DPI0in > 0) { 
    DPI0in--;
    DPIdata0[0] = 0xFF;
  } else { 
    DPIdata0[0] = 0;
  }
  DPIdata0[1] = 0;
  if(DPI1in > 0) { 
    DPI1in--;
    DPIdata0[2] = 0xFF;
  } else { 
    DPIdata0[2] = 0;
  }
  DPIdata0[3] = 0;
  if(DPI2in > 0) { 
    DPI2in--;
    DPIdata0[4] = 0xFF;
  } else { 
    DPIdata0[4] = 0;
  }
  DPIdata0[5] = 0;
  if(DPI3in > 0) { 
    DPI3in--;
    DPIdata0[6] = 0xFF;
  } else { 
    DPIdata0[6] = 0;
  }
  DPIdata0[7] = 0;

  CAN0.sendMsgBuf(0x2C2, 0, 8, DPIdata0);
}

void updateProgressBar(unsigned long count, unsigned long totalCount, int lineToPrintOn, int colToStartOn, int charLength) {
  int totalSteps = charLength * 5;
  int stepSize = totalCount / totalSteps;
  int printSteps = count / stepSize;
  int fullSteps = printSteps / 5;
  int remainder = printSteps % 5;

  if(fullSteps > charLength) {
    fullSteps = charLength;
    remainder = 0;
  }
  int j = 0;
  lcd.setCursor(colToStartOn, lineToPrintOn);
  while (j < fullSteps) {
    lcd.write(5);
    j++;
  }
  if(j<charLength) {
    lcd.write(remainder);
    j++;
  }
  while(j < charLength) {
    lcd.write(0);
    j++;
  }
}

