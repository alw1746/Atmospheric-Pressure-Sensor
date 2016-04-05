# Atmospheric-Pressure-Sensor
Datalogger using Arduino Pro Mini,BMP085 sensor, DS3231 RTC to display timestamp,pressure,altitude,temperature on a 20x4 LCD
display and log CSV data to serial port. Hardware schematic available on   https://easyeda.com/alw1746/Atmospheric_Pressure_Datalogger-PVvRdywgC

Uses Taskscheduler to poll data from the sensor.  
(https://github.com/arkhipenko/TaskScheduler)  
Taskscheduler is very useful and easy to use, see this skeleton code:

	#include <TaskScheduler.h>
	//Taskscheduler callback methods prototypes
	void t1Callback();
	void t2Callback();
	//Tasks
	Task t1(1000, TASK_FOREVER, &t1Callback);   //run t1Callback at 1 sec interval forever.
	Task t2(3000, 10, &t2Callback);             //run t2Callback at 3 sec interval 10 times.
	Scheduler runner;

	void t1Callback() {
		//code here executes at 1 sec interval.
	}

	void t2Callback() {
		//code here executes at 3 sec interval 10 times.
	}

	void setup() {
		runner.init();
		t1.enable();
		t2.enable();
	}

	void loop () {
		runner.execute();
	}
