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
#include <BME280_MOD-1022.h>
#include <TaskScheduler.h>

DS3231  rtc(SDA, SCL);     //use A4,A5 pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);    // init with the numbers of the interface pins

//Taskscheduler callback methods prototypes
void t1Callback();
//Tasks
Task t1(1000, TASK_FOREVER, &t1Callback);   //run t1 at 1 sec interval forever.
Scheduler runner;

String printFormattedFloat(float x, uint8_t precision) {
char buffer[10];

  dtostrf(x, 5, precision, buffer);
  return buffer;
}

void setup(){
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();
  lcd.begin(20, 4);     // init LCD's number of columns and rows:
  BME280.readCompensationParams();
  BME280.writeStandbyTime(tsb_0p5ms);        // tsb = 0.5ms
  BME280.writeFilterCoefficient(fc_8);      // IIR Filter coefficient 16
  BME280.writeOversamplingPressure(os8x);    // pressure x16
  BME280.writeOversamplingTemperature(os2x);  // temperature x2
  BME280.writeOversamplingHumidity(os1x);     // humidity x1
  BME280.writeMode(smNormal);  

  Serial.println("Datetime,Pressure,Humidity,Altitude,Tbm,Tds");  
  runner.init();
  runner.addTask(t1);
  t1.enable();
}

void printCompensatedMeasurements(void) {
String line,dateS,timeS;
String presS,humS,altS,tdS,tbS;
//char presS[10],humS[10],altS[10],tdS[10],tbS[10];
float temp, humidity,  pressure,altitude,tempRTC;

  dateS=rtc.getDateStr(FORMAT_LONG,FORMAT_BIGENDIAN,'-');
  timeS=rtc.getTimeStr();
  temp      = BME280.getTemperature();
  humidity  = BME280.getHumidity();
  pressure  = BME280.getPressure();
  tempRTC=rtc.getTemp();
  altitude = calcAltitude(pressure*100,20.0,102150);

  tbS=printFormattedFloat(temp, 1); 
  humS=printFormattedFloat(humidity, 2);
  presS=printFormattedFloat(pressure, 2);
  tdS=printFormattedFloat(tempRTC, 1); 
  altS=printFormattedFloat(altitude, 2);

  line=dateS+" "+timeS+","+presS+","+humS+","+altS+","+tbS+","+tdS;
  Serial.println(line);

  lcd.setCursor(0, 0);
  lcd.print(dateS);
  lcd.print(" ");
  lcd.print(timeS);
  
  lcd.setCursor(0, 1);
  lcd.print(presS);
  lcd.print("pa  ");

  lcd.print(humS);
  lcd.print("%");

  lcd.setCursor(0, 2);
  lcd.print("Altitude ");
  lcd.print(altS);
  lcd.print("m");

  lcd.setCursor(0, 3);
  lcd.print("Tbm");    //temp from BMP805
  lcd.print(tbS);
  lcd.print("C ");

  lcd.print("Tds");   //temp from DS3231
  lcd.print(tdS);
  lcd.print("C");
}

void updateTime()
{
  String dateS,timeS;
  timeS=rtc.getTimeStr();
  dateS=rtc.getDateStr(FORMAT_LONG,FORMAT_BIGENDIAN,'-');
  if (dateS == "2016-04-17" && timeS == "22:17:00") { 
    //rtc.setDOW(SUNDAY);         // Set Day-of-Week to MONDAY
    //rtc.setDate(28, 12, 2015);  // Set the date to 28 DEC 2015
    rtc.setTime(21, 17, 00);     // Set the time to 14:05:00 (24hr format)
  }
}

void t1Callback()
{
  //updateTime();
  // Send Day-of-Week
  //lcd.print(rtc.getDOWStr());
  //lcd.print(" ");

  while (BME280.isMeasuring()) {
    delay(50);
  }

  BME280.readMeasurements();
  printCompensatedMeasurements();
}

//calculate altitude given pressure,sealevel temp,sealevel pressure using
//hypsometric equation. Default ISA standard for sealevel temp and pressure
//are 15C, 101325 Pa. Max altitude 20,000m only, returns -1000 on error.
//See http://www.mide.com/pages/air-pressure-at-altitude-calculator for details.
//
float calcAltitude(float pressure,float seatemp,float seapressure)
{
  float kelvin=273.15+seatemp;
  float M=0.0289644;
  float g=9.80665;
  float R=8.31432;
  if((seapressure/pressure)<(101325/22632.1))  //up to 11,000m
  {
    float d=-0.0065;
    float e=0;
    float j=pow((pressure/seapressure),(R*d)/(g*M));
    return e+((kelvin*((1/j)-1))/d);
  }
  else
  {
    if((seapressure/pressure)<(101325/5474.89))  //up to 20,000m
    {
      float e=11000;
      float b=kelvin-71.5;
      float f=(R*b*(log(pressure/seapressure)))/((-g)*M);
      float l=101325;
      float c=22632.1;
      float h=((R*b*(log(l/c)))/((-g)*M))+e;
      return h+f;
    }
  }
  return -1000;    //invalid altitude
}

void loop () {
  runner.execute();
}
