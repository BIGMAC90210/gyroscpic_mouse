/*****************************************************************
The LSM9DS0 is a versatile 9DOF sensor. It has a built-in
accelerometer, gyroscope, and magnetometer. Very cool! Plus it
functions over either SPI or I2C.

This Arduino sketch is a demo of the simple side of the
SFE_LSM9DS0 library. It'll demo the following:
* How to create a LSM9DS0 object, using a constructor (global
  variables section).
* How to use the begin() function of the LSM9DS0 class.
* How to read the gyroscope, accelerometer, and magnetometer
  using the readGryo(), readAccel(), readMag() functions and the
  gx, gy, gz, ax, ay, az, mx, my, and mz variables.
* How to calculate actual acceleration, rotation speed, magnetic
  field strength using the calcAccel(), calcGyro() and calcMag()
  functions.
* How to use the data from the LSM9DS0 to calculate orientation
  and heading.

Hardware setup: This library supports communicating with the
LSM9DS0 over either I2C or SPI. If you're using I2C, these are
the only connections that need to be made:
	LSM9DS0 --------- Arduino
	 SCL ---------- SCL (A5 on older 'Duinos')
	 SDA ---------- SDA (A4 on older 'Duinos')
	 VDD ------------- 3.3V
	 GND ------------- GND
(CSG, CSXM, SDOG, and SDOXM should all be pulled high jumpers on 
  the breakout board will do this for you.)
  
If you're using SPI, here is an example hardware setup:
	LSM9DS0 --------- Arduino
          CSG -------------- 9
          CSXM ------------- 10
          SDOG ------------- 12
          SDOXM ------------ 12 (tied to SDOG)
          SCL -------------- 13
          SDA -------------- 11
          VDD -------------- 3.3V
          GND -------------- GND
	
The LSM9DS0 has a maximum voltage of 3.6V. Make sure you power it
off the 3.3V rail! And either use level shifters between SCL
and SDA or just use a 3.3V Arduino Pro.	  

Development environment specifics:
	IDE: Arduino 1.0.5
	Hardware Platform: Arduino Pro 3.3V/8MHz
	LSM9DS0 Breakout Version: 1.0

This code is beerware. If you see me (or any other SparkFun 
employee) at the local, and you've found our code helpful, please 
buy us a round!

Distributed as-is; no warranty is given.
*****************************************************************/

// The SFE_LSM9DS0 requires both the SPI and Wire libraries.
// Unfortunately, you'll need to include both in the Arduino
// sketch, before including the SFE_LSM9DS0 library.
#include <SPI.h> // Included for SFE_LSM9DS0 library
#include <Wire.h>  // Use the modified version to allow for the buffer not being full
#include <SFE_LSM9DS0.h>

///////////////////////
// Example I2C Setup //
///////////////////////
// Comment out this section if you're using SPI
// SDO_XM and SDO_G are both grounded, so our addresses are:
#define LSM9DS0_XM  0x1D // Would be 0x1E if SDO_XM is LOW
#define LSM9DS0_G   0x6B // Would be 0x6A if SDO_G is LOW
// Create an instance of the LSM9DS0 library called `dof` the
// parameters for this constructor are:
// [SPI or I2C Mode declaration],[gyro I2C address],[xm I2C add.]
LSM9DS0 dof(MODE_I2C, LSM9DS0_G, LSM9DS0_XM);

///////////////////////
// Example SPI Setup //
///////////////////////
/* // Uncomment this section if you're using SPI
#define LSM9DS0_CSG  9  // CSG connected to Arduino pin 9
#define LSM9DS0_CSXM 10 // CSXM connected to Arduino pin 10
LSM9DS0 dof(MODE_SPI, LSM9DS0_CSG, LSM9DS0_CSXM);
*/

// Do you want to print calculated values or raw ADC ticks read
// from the sensor? Comment out ONE of the two #defines below
// to pick:
#define PRINT_CALCULATED
//#define PRINT_RAW

#define PRINT_SPEED 50 // 500 ms between prints

const int ledPin = 13;
const int buttonPin=11;

///////////////////////////////
// Interrupt Pin Definitions //
///////////////////////////////
const byte INT1XM =6; // INT1XM tells us when accel data is ready
const byte INT2XM = 7; // INT2XM tells us when mag data is ready
const byte DRDYG = 5;  // DRDYG tells us when gyro data is ready


int lastButtonState=LOW;
int currentButtonState=LOW;
boolean outputOn=true;
unsigned long lastButtonClickTime=0;

int averageCount=0;
double averageX=0;
double averageY=0;
double averageZ=0;

void setup()
{
  // wait a minute before starting to allow the LSM9DS0 
delay(1000);
  //  Serial.begin(115200); // Start serial at 115200 bps
  // Use the begin() function to initialize the LSM9DS0 library.
  // You can either call it with no parameters (the easy way):
  uint16_t status = dof.begin();
  // Or call it with declarations for sensor scales and data rates:  
  //uint16_t status = dof.begin(dof.G_SCALE_2000DPS, 
  //                            dof.A_SCALE_6G, dof.M_SCALE_2GS);
  
  // begin() returns a 16-bit value which includes both the gyro 
  // and accelerometers WHO_AM_I response. You can check this to
  // make sure communication was successful.
  //Serial.print("LSM9DS0 WHO_AM_I's returned: 0x");
  //Serial.println(status, HEX);
  //Serial.println("Should be 0x49D4");
  //Serial.println();
  

  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  // Set up interrupt pins as inputs:
  pinMode(INT1XM, INPUT);
  pinMode(INT2XM, INPUT);
  pinMode(DRDYG, INPUT);

  digitalWrite(ledPin, HIGH);   // turn the LED on (HIGH is the voltage level)
  
  //ignore the first 10 readings - they are normally not so good

  for (int i=0;i<10;i++){
      recordAverageGyro();
      delay(PRINT_SPEED);
  }
        averageX=0;
        averageY=0;
        averageZ=0;
        averageCount=0;
        Mouse.begin();
}

void loop()
{
  // check if the button is toggled to turn the mouse function on and off. When turning off, the stable position will be reset
  currentButtonState = digitalRead(buttonPin);
  if ((currentButtonState!=lastButtonState && ((millis()-lastButtonClickTime)>100)) ){
    if(currentButtonState==LOW ){
      outputOn=!outputOn;
      lastButtonClickTime=millis();
      if(outputOn){
        digitalWrite(ledPin, HIGH);   // turn the LED on (HIGH is the voltage level)
      } else {
        digitalWrite(ledPin, LOW);   // turn the LED on (HIGH is the voltage level)
        averageX=0;
        averageY=0;
        averageZ=0;
        averageCount=0;
      }
    }
    lastButtonState=currentButtonState;
  }

  if(outputOn){
    
    // calculate the average gyroscope reading of the stationary device - it's probably not 0, so we need to factor that out.
    if(averageCount<30){
      recordAverageGyro();
      delay(PRINT_SPEED);
    } else if (averageCount==30){
      averageX=averageX/30;
      averageY=averageY/30;
      averageZ=averageZ/30;
      averageCount=100;
  //Serial.print("average=");
  //Serial.print(averageX, 2);
  //Serial.print(", ");
  //Serial.print(averageY, 2);
  //Serial.print(", ");
  //Serial.println(averageZ, 2);
    } else {
    
  //printGyro();  // Print "G: gx, gy, gz"
  mouseMoveGyro();
//  Serial.println();
  
 // delay(PRINT_SPEED);
    }
  
  
  }
}

/*
void printGyro()
{
  // To read from the gyroscope, you must first call the
  // readGyro() function. When this exits, it'll update the
  // gx, gy, and gz variables with the most current data.
  dof.readGyro();
  dof.readAccel();

  // Now we can use the gx, gy, and gz variables as we please.
  // Either print them as raw ADC values, or calculated in DPS.
  Serial.print("G: ");
#ifdef PRINT_CALCULATED
  // If you want to print calculated values, you can use the
  // calcGyro helper function to convert a raw ADC value to
  // DPS. Give the function the value that you want to convert.
  Serial.print((dof.calcGyro(dof.gx)-averageX), 2);
  Serial.print(", ");
  Serial.print((dof.calcGyro(dof.gy)-averageY), 2);
  Serial.print(", ");
  Serial.print(dof.calcGyro(dof.gz)-averageZ, 2);
  Serial.print ("X= ");
  Serial.println((dof.calcGyro(dof.gx)-averageX)*dof.calcAccel(dof.ax) +(dof.calcGyro(dof.gy)-averageY)*dof.calcAccel(dof.ay) , 2);
#elif defined PRINT_RAW
//  Serial.print(dof.gx);
//  Serial.print(", ");
 // Serial.print(dof.gy);
  //Serial.print(", ");
 // Serial.println(dof.gz);
#endif
}*/

void mouseMoveGyro()
{
  
// wait until there is both an accelerometer and gyroscope reading
  if ( (digitalRead(INT1XM)) &&  (digitalRead(DRDYG)))
  {
    
    // read the values

  dof.readGyro();
  dof.readAccel();
  
  // move the mouse left and right depending on gyroscope movement on the X and Y axis, scaled by the accelerometer reading to allow it to point in any direction.
  // move the mouse up and down based on the z axis rotation. Designed for the chip facing right. Reveerse the Z calculation if facing left.
  Mouse.move((averageX-dof.calcGyro(dof.gx))*dof.calcAccel(dof.ax) +(averageY-dof.calcGyro(dof.gy))*dof.calcAccel(dof.ay), averageZ-dof.calcGyro(dof.gz));
    
  }
}

void recordAverageGyro(){
  // add another gyroscope reading to build up an average.
    dof.readGyro();
averageX+=dof.calcGyro(dof.gx);
averageY+=dof.calcGyro(dof.gy);
averageZ+=dof.calcGyro(dof.gz);
averageCount=averageCount+1;
}
