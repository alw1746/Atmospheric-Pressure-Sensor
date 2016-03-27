//Arduino 1.0+ Only

/*
 Get pressure, altitude, and temperature from the BMP085.
 Get datetime from DS3231 RTC.
 Serial.print at 9600 baud to serial monitor.
 lcd.print to 20x4 LCD display.
 Uses the highly recommended Taskscheduler library to schedule tasks.
 See https://github.com/arkhipenko/TaskScheduler

 Based on code by Jim Lindblom (Sparkfun).
 */
#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal.h>
#include <TaskScheduler.h>

#define BMP085_ADDRESS 0x77  // I2C address of BMP085

const unsigned char OSS = 0;  // Oversampling Setting

// Calibration values
int ac1;
int ac2;
int ac3;
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1;
int b2;
int mb;
int mc;
int md;

// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 
String line,dateS,timeS;
char presS[10],atmS[10],altS[10],tdS[10],tbS[10];

DS3231  rtc(SDA, SCL);     //use A4,A5 pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);    // init with the numbers of the interface pins

//Taskscheduler callback methods prototypes
void t1Callback();
//Tasks
Task t1(1000, TASK_FOREVER, &t1Callback);   //run t1 at 1 sec interval forever.
Scheduler runner;

void setup(){
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  dateS=rtc.getDateStr(FORMAT_LONG,FORMAT_BIGENDIAN,'-');
  if (dateS < "2015-12-01") { 
    rtc.setDOW(MONDAY);         // Set Day-of-Week to MONDAY
    rtc.setTime(14, 05, 0);     // Set the time to 14:05:00 (24hr format)
    rtc.setDate(28, 12, 2015);  // Set the date to 28 DEC 2015
  }
 
  lcd.begin(20, 4);     // init LCD's number of columns and rows:

  bmp085Calibration();

  Serial.println("Datetime,Pressure,Atmosphere,Altitude,Tbm,Tds");
  
  runner.init();
  runner.addTask(t1);
  t1.enable();
}

void t1Callback()
{
  lcd.setCursor(0, 0);
  // Send Day-of-Week
  //lcd.print(rtc.getDOWStr());
  //lcd.print(" ");
  
  dateS=rtc.getDateStr(FORMAT_LONG,FORMAT_BIGENDIAN,'-');
  lcd.print(dateS);
  lcd.print(" ");
  timeS=rtc.getTimeStr();
  lcd.print(timeS);
 
  float temperature = bmp085GetTemperature(bmp085ReadUT()); //MUST be called first
  dtostrf(temperature,3,1,tbS);
  long  pressure = bmp085GetPressure(bmp085ReadUP());
  sprintf(presS,"%lu",pressure);
  float atm = (float)pressure / 101325; // "standard atmosphere"
  dtostrf(atm,6,4,atmS);
  float altitude = calcAltitude(pressure); //Uncompensated caculation - in Meters 
  dtostrf(altitude,5,2,altS);

  lcd.setCursor(0, 1);
  lcd.print(presS);
  lcd.print(" pa ");

  lcd.print(atmS);
  lcd.print(" atm");

  lcd.setCursor(0, 2);
  lcd.print("Altitude ");
  lcd.print(altS);
  lcd.print(" M");

  lcd.setCursor(0, 3);
  lcd.print("Tbm ");    //temp from BMP805
  lcd.print(tbS);
  lcd.print("C ");

  float tempRTC=rtc.getTemp();
  dtostrf(tempRTC,3,1,tdS);
  lcd.print("Tds ");   //temp from DS3231
  lcd.print(tdS);
  lcd.print("C");

  line=dateS+" "+timeS+","+presS+","+atmS+","+altS+","+tbS+","+tdS;
  Serial.println(line);
}

// Stores all of the bmp085's calibration values into global variables
// Calibration values are required to calculate temp and pressure
// This function should be called at the beginning of the program
void bmp085Calibration()
{
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);
}

// Calculate temperature in deg C
float bmp085GetTemperature(unsigned int ut){
  long x1, x2;

  x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
  x2 = ((long)mc << 11)/(x1 + md);
  b5 = x1 + x2;

  float temp = ((b5 + 8)>>4);
  temp = temp /10;

  return temp;
}

// Calculate pressure given up
// calibration values must be known
// b5 is also required so bmp085GetTemperature(...) must be called first.
// Value returned will be pressure in units of Pa.
long bmp085GetPressure(unsigned long up){
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;

  b6 = b5 - 4000;
  // Calculate B3
  x1 = (b2 * (b6 * b6)>>12)>>11;
  x2 = (ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;

  // Calculate B4
  x1 = (ac3 * b6)>>13;
  x2 = (b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;

  b7 = ((unsigned long)(up - b3) * (50000>>OSS));
  if (b7 < 0x80000000)
    p = (b7<<1)/b4;
  else
    p = (b7/b4)<<1;

  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;

  long temp = p;
  return temp;
}

// Read 1 byte from the BMP085 at 'address'
char bmp085Read(unsigned char address)
{
  unsigned char data;

  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(BMP085_ADDRESS, 1);
  while(!Wire.available())
    ;

  return Wire.read();
}

// Read 2 bytes from the BMP085
// First byte will be from 'address'
// Second byte will be from 'address'+1
int bmp085ReadInt(unsigned char address)
{
  unsigned char msb, lsb;

  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(BMP085_ADDRESS, 2);
  while(Wire.available()<2)
    ;
  msb = Wire.read();
  lsb = Wire.read();

  return (int) msb<<8 | lsb;
}

// Read the uncompensated temperature value
unsigned int bmp085ReadUT(){
  unsigned int ut;

  // Write 0x2E into Register 0xF4
  // This requests a temperature reading
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();

  // Wait at least 4.5ms
  delay(5);

  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

// Read the uncompensated pressure value
unsigned long bmp085ReadUP(){

  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;

  // Write 0x34+(OSS<<6) into register 0xF4
  // Request a pressure reading w/ oversampling setting
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (OSS<<6));
  Wire.endTransmission();

  // Wait for conversion, delay time dependent on OSS
  delay(2 + (3<<OSS));

  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  msb = bmp085Read(0xF6);
  lsb = bmp085Read(0xF7);
  xlsb = bmp085Read(0xF8);

  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);

  return up;
}

void writeRegister(int deviceAddress, byte address, byte val) {
  Wire.beginTransmission(deviceAddress); // start transmission to device 
  Wire.write(address);       // send register address
  Wire.write(val);         // send value to write
  Wire.endTransmission();     // end transmission
}

int readRegister(int deviceAddress, byte address){

  int v;
  Wire.beginTransmission(deviceAddress);
  Wire.write(address); // register to read
  Wire.endTransmission();

  Wire.requestFrom(deviceAddress, 1); // read a byte

  while(!Wire.available()) {
    // waiting
  }

  v = Wire.read();
  return v;
}

float calcAltitude(float pressure){

  float A = pressure/101325;
  float B = 1/5.25588;
  float C = pow(A,B);
  C = 1 - C;
  C = C /0.0000225577;

  return C;
}

void loop () {
  runner.execute();
}
