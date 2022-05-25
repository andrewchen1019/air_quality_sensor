#include <CircularBuffer.h>
#include <PMS.h>

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>


// Screen dimensions
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128 

PMS pms(Serial);
PMS::DATA data;

extern volatile unsigned long timer0_millis;  //timer0_millis is the variable that keeps track of system time.  the odd variable assignment is because of something because of arduino stuff

float                 totalAQI = 0;
float                 currentReading[2] = {0, 0}; //{2.5uM, 10.0uM}
bool                  currentReadingStored = false;
int                   readingCount = 0;
volatile float        measureSize = 2.5;  //not sure why this is volatile

int                   interruptCount = 0;

bool                  buttonInterrupt = false;

bool                  aqiTenMinHasPrinted  = false;
bool                  aqiOneHourHasPrinted = false;
bool                  aqiOneDayHasPrinted  = false;

bool                  aqiTenMinHasDisplayed  = false;
bool                  aqiOneHourHasDisplayed = false;
bool                  aqiOneDayHasDisplayed  = false;

int                   tenMinMin  = 10;   //for testing: 3;//actual values: 10;
int                   oneHourMin = 60;   //for testing: 6;//actual values: 60;
int                   oneDayMin  = 1440; //for testing: 9;//actual values: 1440;

//Total dealy is about 90 seconds.  If this is changed, then the CircularBuffer size must be changed as well
int                   wakeUpDelaySec = 30;
int                   sleepDelaySec  = 60;

int                   wakeUpDelayMs = 30000;
int                   sleepDelayMs  = 60000;
 
int                   aqiTenMin[2]  = {0,0};
int                   aqiOneHour[2] = {0,0};
int                   aqiOneDay[2]  = {0,0};

int                   recentReading = -1;

const int             buttonPin = 3;
volatile int          buttonState = 0;
int                   buttonPressedTime = 0;

long int              currentSleepSec  = 0;
long int              previousSleepSec = 0;
long int              currentWakeSec   = 0;
long int              previousWakeSec  = 0;

bool                  is12pt = false;
bool                  isNotFilled = false;  // not sure what isNotFilled does

//AQIXY = {1 char cursor position, 2 char cursor position, 3 char cursor position]
const int             recentAQIXY[3][2]    = {{55, 46}, {47, 46}, {38, 46}};
const int             recentRectXY[4]      = {0,  13, 128, 46};
const int             recentRectBlackXY[4] = {2,  15, 120, 40};

const int             tenMinAQIXY[3][2]    = {{14, 113}, { 7, 113},{ 1, 113}};
const int             tenMinRectXY[4]      = {0,  83, 43, 45};

const int             oneHourAQIXY[3][2]   = {{58, 113}, {51, 113}, {44, 113}};
const int             oneHourRectXY[4]     = {43, 83, 43, 45};

const int             oneDayAQIXY[3][2]    = {{100, 113}, { 94, 113},{ 88, 113}};
const int             oneDayRectXY[4]      = {86, 83, 43, 45};

const int             headerBlackRectXY[4] = {0,  0, 128, 13};

CircularBuffer<float,7> tenMinQueue_2_5;    // 600 sec / 90 sec (the delay time) = 6.667
CircularBuffer<float,7> oneHourQueue_2_5;   // Queue is updated 6 times in one hour, so +1 just in case
CircularBuffer<float,25> oneDayQueue_2_5;   // Queue is updated 24 times in one day, so +1 just in case

CircularBuffer<float,7> tenMinQueue_10_0;    // 600 sec / 90 sec (the delay time) = 6.667
CircularBuffer<float,7> oneHourQueue_10_0;   // Queue is updated 6 times in one hour, so +1 just in case
CircularBuffer<float,25> oneDayQueue_10_0;   // Queue is updated 24 times in one day, so +1 just in case



// The SSD1351 is connected like this (plus VCC plus GND)
const uint8_t   OLED_pin_scl_sck        = 13;
const uint8_t   OLED_pin_sda_mosi       = 11;
const uint8_t   OLED_pin_cs_ss          = 10;
const uint8_t   OLED_pin_res_rst        = 9;
const uint8_t   OLED_pin_dc_rs          = 8;

// The colors we actually want to use
const uint16_t  OLED_Color_Black        = 0x0000;
const uint16_t  OLED_Color_White        = 0xFFFF;

const uint16_t  OLED_Good_Color         = 0x04A1;
const uint16_t  OLED_Moderate_Color     = 0xDF00;
const uint16_t  OLED_UnhealthySG_Color  = 0xEB20;
const uint16_t  OLED_Unhealthy_Color    = 0xE800;
const uint16_t  OLED_VUnhealthy_Color   = 0xA800;
const uint16_t  OLED_Hazardous_Color    = 0x7800;

uint16_t        OLED_Text_Color         = OLED_Color_White;
uint16_t        OLED_Background_Color   = OLED_Color_Black;

// declare the display
Adafruit_SSD1351 oled =
  Adafruit_SSD1351(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &SPI,
    OLED_pin_cs_ss,
    OLED_pin_dc_rs,
    OLED_pin_res_rst
  );

// assume the display is off until configured in setup()
bool isDisplayVisible = false;


float average(int reading){
  totalAQI = totalAQI + reading;
  return (totalAQI/readingCount);
}

float getTenMinAverage(CircularBuffer<float,7> &timeQueue){
  float tempTotalAqi = 0;
  int queueLength = timeQueue.size();
  
  while(queueLength > 0){
    tempTotalAqi = tempTotalAqi + timeQueue[queueLength];
    queueLength--;
  }
  return (tempTotalAqi/timeQueue.size());
}

float getOneHourAverage(CircularBuffer<float,7> &timeQueue){
  float tempTotalAqi = 0;
  int queueLength = timeQueue.size();
  
  while(queueLength > 0){
    tempTotalAqi = tempTotalAqi + timeQueue[queueLength];
    queueLength--;
  }
  return (tempTotalAqi/timeQueue.size());
}

float getOneDayAverage(CircularBuffer<float,25> &timeQueue){
  float tempTotalAqi = 0;
  int queueLength = timeQueue.size();
  
  while(queueLength > 0){
    tempTotalAqi = tempTotalAqi + timeQueue[queueLength];
    queueLength--;
  }
  return (tempTotalAqi/timeQueue.size());
}

int msToSec(){
  float msConversion = 1000;
  return (millis()/msConversion);
}

int msToMin(){
  float msConversion = 60000;
  return (millis()/msConversion);
}

void setCursorXY(int coordXY[2]){
  oled.setCursor(coordXY[0], coordXY[1]);
}

int aqi2_5Calc(float averageReading){
  averageReading = ((float )((int)(averageReading * 10))) / 10;
  int aqi = -1;
  bool updateReading = true;
  
  if (averageReading >= 0.0 && averageReading <= 12.0){
    aqi = ((50 - 0)/(12.0 - 0)) * (averageReading - 0) + 0; 
  }
  else if (averageReading >= 12.1 && averageReading <= 35.4){
    aqi = ((100 - 51)/(35.4 - 12.1)) * (averageReading - 12.1) + 51;
  }
  else if (averageReading >= 35.5 && averageReading <= 55.4){
    aqi = ((150 - 101)/(55.4 - 35.5)) * (averageReading - 35.5) + 101;
  }
  else if (averageReading >= 55.5 && averageReading <= 150.4){
    aqi = ((200 - 151)/(150.4 - 55.5)) * (averageReading - 55.5) + 151;
  }
  else if (averageReading >= 150.5 && averageReading <= 250.4){
    aqi = ((300 - 201)/(250.4 - 150.5)) * (averageReading - 150.5) + 201;
  }
  else if (averageReading >= 250.5 && averageReading <= 350.4){
    aqi = ((400 - 301)/(350.4 - 250.5)) * (averageReading - 250.5) + 301;
  }
  else if (averageReading >= 350.5 && averageReading <= 500.4){
    aqi = ((500 - 401)/(500.4 - 350.5)) * (averageReading - 350.5) + 401;
  }
  else{
    updateReading = false;
  }
  return aqi;
}  


int aqi10_0Calc(float averageReading){
  averageReading = ((float )((int)(averageReading * 10))) / 10;
  int aqi = -1;
  bool updateReading = true;
  
  if (averageReading >= 0.0 && averageReading <= 54){
    aqi = ((50 - 0)/(54 - 0)) * (averageReading - 0) + 0; 
  }
  else if (averageReading >= 55 && averageReading <= 154){
    aqi = ((100 - 51)/(154 - 55)) * (averageReading - 55) + 51;
  }
  else if (averageReading >= 155 && averageReading <= 254){
    aqi = ((150 - 101)/(254 - 155)) * (averageReading - 155) + 101;
  }
  else if (averageReading >= 255 && averageReading <= 354){
    aqi = ((200 - 151)/(354 - 255)) * (averageReading - 255) + 151;
  }
  else if (averageReading >= 355 && averageReading <= 424){
    aqi = ((300 - 201)/(424 - 355)) * (averageReading - 355) + 201;
  }
  else if (averageReading >= 425 && averageReading <= 504){
    aqi = ((400 - 301)/(504 - 425)) * (averageReading - 425) + 301;
  }
  else if (averageReading >= 505 && averageReading <= 604){
    aqi = ((500 - 401)/(604 - 505)) * (averageReading - 505) + 401;
  }
  else{
    updateReading = false;
  }
  return aqi;
}  

uint16_t aqiColorSet(int aqi){
  if (aqi >= 0 && aqi <= 50){
    return OLED_Good_Color;
  }
  else if (aqi >= 51 && aqi <= 100){
    return OLED_Moderate_Color; 
  }
  else if (aqi >= 101 && aqi <= 150){
    return OLED_UnhealthySG_Color;
  }
  else if (aqi >= 151 && aqi <= 200){
    return OLED_Unhealthy_Color;
  }
  else if (aqi >= 201 && aqi <= 300){
    return OLED_VUnhealthy_Color;
  }
  else if (aqi >= 301 && aqi <= 500){
    return OLED_Hazardous_Color;
  }
}

void oneDayTimeReset(){
  timer0_millis = 0;
  currentSleepSec  = msToSec();
  previousSleepSec = msToSec();
  currentWakeSec   = msToSec();
  previousWakeSec  = msToSec();
}

void updateTenMinQueue(float reading[2]){
  if(msToMin() < tenMinMin){
    tenMinQueue_2_5.push(reading[0]);
    tenMinQueue_10_0.push(reading[1]);
  }
  else{
    tenMinQueue_2_5.shift();
    tenMinQueue_10_0.shift();
    tenMinQueue_2_5.push(reading[0]);
    tenMinQueue_10_0.push(reading[1]);
  }
}

void updateOneHourQueue(int reading[2]){
  if(msToMin() < oneHourMin){
    oneHourQueue_2_5.push(reading[0]);
    oneHourQueue_10_0.push(reading[1]);
  }
  else{
    oneHourQueue_2_5.shift();
    oneHourQueue_10_0.shift();
    oneHourQueue_2_5.push(reading[0]);
    oneHourQueue_10_0.push(reading[1]);
  }
}

void updateOneDayQueue(int reading[2]){
  if(msToMin() < oneDayMin){
    oneDayQueue_2_5.push(reading[0]);
    oneDayQueue_10_0.push(reading[1]);
  }
  else{
    oneDayQueue_2_5.shift();
    oneDayQueue_10_0.shift();
    oneDayQueue_2_5.push(reading[0]);
    oneDayQueue_10_0.push(reading[1]);
  }
} 

/*
 * This controls 
 */
void aqiOverTime(){ 
  int averageReading[2] = {0, 0};
  if (msToMin() >= 1 && (msToMin() % tenMinMin) == 0){
    if (!aqiTenMinHasPrinted){
      Serial.println("in 10 min print");
      averageReading[0] = getTenMinAverage(tenMinQueue_2_5);
      averageReading[1] = getTenMinAverage(tenMinQueue_10_0);

      aqiTenMin[0] = aqi2_5Calc(averageReading[0]);   
      aqiTenMin[1] = aqi10_0Calc(averageReading[1]);
      
      is12pt = true;

      if(measureSize = 2.5){
        displayRect(tenMinRectXY, aqiColorSet(aqiTenMin[0]));
        displayAQI(tenMinAQIXY, aqiTenMin[0]); 
        aqiTenMinHasDisplayed = true;
      }
      else if(measureSize = 10){
        displayRect(tenMinRectXY, aqiColorSet(aqiTenMin[1]));
        displayAQI(tenMinAQIXY, aqiTenMin[1]); 
        aqiTenMinHasDisplayed = true;
      }      

      updateOneHourQueue(aqiTenMin);
      aqiTenMinHasPrinted = true;
    }
  }
  else{
    aqiTenMinHasPrinted = false;
  }

  if (msToMin() >= 1 && (msToMin() % oneHourMin) == 0){
    if (!aqiOneHourHasPrinted){
      averageReading[0] = getOneHourAverage(oneHourQueue_2_5);
      averageReading[1] = getOneHourAverage(oneHourQueue_10_0);
      
      aqiOneHour[0] = aqi2_5Calc(averageReading[0]);
      aqiOneHour[1] = aqi10_0Calc(averageReading[1]);

      is12pt = true;
      
      if(measureSize = 2.5){
        displayRect(oneHourRectXY, aqiColorSet(aqiOneHour[0]));      
        displayAQI(oneHourAQIXY, aqiOneHour[0]); 
        aqiOneHourHasDisplayed = true;
      }
      else if(measureSize = 10){
        displayRect(oneHourRectXY, aqiColorSet(aqiOneHour[1]));      
        displayAQI(oneHourAQIXY, aqiOneHour[1]); 
        aqiOneHourHasDisplayed = true;
      }
            
      updateOneDayQueue(aqiTenMin);

      aqiOneHourHasPrinted = true;
    }
  }
  else{
    aqiOneHourHasPrinted = false;
  }

  
  if (msToMin() >= 1 && (msToMin() % oneDayMin) == 0){
    if (!aqiOneDayHasPrinted){
      averageReading[0] = getOneDayAverage(oneDayQueue_2_5);
      averageReading[1] = getOneDayAverage(oneDayQueue_10_0);

      aqiOneDay[0] = aqi2_5Calc(averageReading[0]);
      aqiOneDay[1] = aqi10_0Calc(averageReading[1]);
      
      is12pt = true;  

      if(measureSize = 2.5){
        displayRect(oneDayRectXY, aqiColorSet(aqiOneDay[0]));      
        displayAQI(oneDayAQIXY, aqiOneDay[0]); 
        aqiOneDayHasDisplayed = true;
      }
      else if(measureSize = 10){
        displayRect(oneDayRectXY, aqiColorSet(aqiOneDay[1]));      
        displayAQI(oneDayAQIXY, aqiOneDay[1]); 
        aqiOneDayHasDisplayed = true;
      }
        
      aqiOneDayHasPrinted = true;
      oneDayTimeReset();
    }
  }
  else{
    aqiOneDayHasPrinted = false;
  }
}

void displayAQI(int aqiXY[3][2], int newInputAQI){
    if(is12pt){
      oled.setFont(&FreeSansBold12pt7b);      
    }
    else{
      oled.setFont(&FreeSansBold18pt7b);  
    }
    is12pt = false;
    oled.setTextColor(OLED_Text_Color);
    setCursorXY(aqiXY[displaySizeProcess(newInputAQI)]);
    oled.print(newInputAQI);
}

int displaySizeProcess(int newInputAQI){
  if(newInputAQI >= 0 && newInputAQI < 10){  
    return 0;
  }
   else if( newInputAQI >= 10 && newInputAQI < 100){ 
    return 1;
  }
  else if(newInputAQI >= 100){ 
    return 2;
  }  
}

void displayRect(int rectXY[4], uint16_t rectColor){
  if(isNotFilled){
    oled.drawRect(rectXY[0], rectXY[1], rectXY[2], rectXY[3], rectColor);
  }
  else{
    oled.fillRect(rectXY[0], rectXY[1], rectXY[2], rectXY[3], rectColor);
  }
  isNotFilled = false;
}

void changeHeader(){
  if(measureSize == 2.5){
    displayRect(headerBlackRectXY , OLED_Color_Black);
    oled.setCursor(5,0);
    oled.print("Latest 2.5uM Reading");
  }
  else if(measureSize == 10){
    displayRect(headerBlackRectXY , OLED_Color_Black);
    oled.setCursor(8,0);
    oled.print("Latest 10uM Reading");
  }
  
}

class pmsSensor
{
  /*
   * This is to control the following with the PMS Sensor:
   *  - Initialization
   *  - Reading from the sensor
   *  - Waking up the sensor
   *  - Putting the sensor to sleep
   * 
   */
  bool wakeUpStatus;
  bool updateAQI = true;

  public:
  void init(){
    bool wakeUpStatus = false;
    pms.passiveMode();
    pms.sleep();
  }

  /*
   * This controls when when to wake up the sensor, when the read from the sensor, and when the sensor should sleep
   * 
   * It also controls when the particle reading is stored
   */
  void DoReading()
  { 
    if ((currentSleepSec - previousSleepSec) >= sleepDelaySec){
      wakeUpPms();       

      if (currentWakeSec - previousWakeSec >= wakeUpDelaySec){
        readFromPms();
        delay(250);
        goToSleepPms();   
      }
      else{
        //this else updates the currentWakeSec to count how long the PMS has been in the wake state
        currentWakeSec = msToSec();
      }
      
    }
    else{  
      if (currentReadingStored){
        updateTenMinQueue(currentReading);
        delay(100);

        oled.setFont(&FreeSansBold18pt7b);
        isNotFilled = true;
        
        if(measureSize = 2.5){
          recentReading = data.PM_AE_UG_2_5;
          displayRect(recentRectXY, aqiColorSet(aqi2_5Calc(recentReading)));
          displayRect(recentRectBlackXY, OLED_Background_Color);
          displayAQI(recentAQIXY, aqi2_5Calc(recentReading)); 
        }
        else if(measureSize = 10){
          recentReading = data.PM_AE_UG_10_0;
          displayRect(recentRectXY, aqiColorSet(aqi10_0Calc(recentReading)));
          displayRect(recentRectBlackXY, OLED_Background_Color);
          displayAQI(recentAQIXY, aqi10_0Calc(recentReading)); 
        }           
                
        currentReadingStored = false;
      }      
      currentSleepSec = msToSec();
    }
  }

  /*
   * This tells the sensor to wake up as well as stores the last time the sensor was told to wake up
   */
  void wakeUpPms(){
    if (!wakeUpStatus){
      pms.wakeUp();
      wakeUpStatus = true;
      currentWakeSec = msToSec();
      previousWakeSec = msToSec();
    }
  }

  /*
   * This tells the PMS sensor to sleep as well as stores the time when it last slept
   */
  void goToSleepPms(){
    currentSleepSec = msToSec();
    previousSleepSec = msToSec();
    pms.sleep();
    wakeUpStatus = false;
  }


  /*
   * This requests to read from the PMS sensor, waits for data to come in, then stores the AQI in currentReading
   */
  void readFromPms(){
    pms.requestRead();
    if (pms.readUntil(data)){
      readingCount++;
      currentReading[0] = data.PM_AE_UG_2_5;
      currentReading[1] = data.PM_AE_UG_10_0;
      currentReadingStored = true;
      updateAQI = true;
    }
    else{
      updateAQI = false;
    }
  }
};

class oledDisplay{
  int testDisp = 12;
  public:
  void init(){
    
    // initialise the SSD1331
    delay(250);
    oled.begin();
  
    oled.fillScreen(OLED_Background_Color);
    oled.setFont();
    oled.setTextColor(OLED_Text_Color);
    oled.setTextSize(1);
  
    // the display is now on
    isDisplayVisible = true;
  
    changeHeader();
  
    oled.setCursor(3,65);
    oled.print("10 Min");

    oled.setCursor(13,74);
    oled.print("AQI");
  
    oled.setCursor(53,65);
    oled.print("1 Hr");

    oled.setCursor(55,74);
    oled.print("AQI");
  
    oled.setCursor(94,65);
    oled.print("24 Hr");

    oled.setCursor(99,74);
    oled.print("AQI");
  
  }

  
};

oledDisplay oledDisplay1;

void changeSplashScreen(){
  oled.fillScreen(OLED_Background_Color);
  oled.setFont(&FreeSans9pt7b);
  
  oled.setCursor(27,47);
  oled.print("Changing");

  oled.setCursor(10,67);
  oled.print("measurement");

  if (measureSize == 2.5){
    oled.setCursor(31,87);
    oled.print("to 2.5uM");
  }
  else if (measureSize == 10){
    oled.setCursor(25,87);
    oled.print("to 10.0uM");
  }
  delay(2500);
}


pmsSensor pmsSensor1;

void changeMeasurementActions(){
  is12pt = false; 
  //timer0_millis = 0;   
  changeSplashScreen();    
  
  aqiTenMinHasPrinted = false;
  aqiOneHourHasPrinted = false;
  aqiOneDayHasPrinted = false;
  currentReadingStored = false;
  isNotFilled = false;

  oledDisplay1.init();
  pmsSensor1.goToSleepPms();
  currentSleepSec  = msToSec();
  previousSleepSec = msToSec();
  currentWakeSec   = msToSec();
  previousWakeSec  = msToSec();

  if(aqiTenMinHasDisplayed){
    is12pt = true;
    if(measureSize = 2.5){
      displayRect(tenMinRectXY, aqiColorSet(aqiTenMin[0]));
      displayAQI(tenMinAQIXY, aqiTenMin[0]); 
    }
    else if(measureSize = 10){
      displayRect(tenMinRectXY, aqiColorSet(aqiTenMin[1]));
      displayAQI(tenMinAQIXY, aqiTenMin[1]); 
    }   
  }
  
  if(aqiOneHourHasDisplayed){
    is12pt = true;
    if(measureSize = 2.5){
      displayRect(oneHourRectXY, aqiColorSet(aqiOneHour[0]));      
      displayAQI(oneHourAQIXY, aqiOneHour[0]); 
    }
    else if(measureSize = 10){
      displayRect(oneHourRectXY, aqiColorSet(aqiOneHour[1]));      
      displayAQI(oneHourAQIXY, aqiOneHour[1]); 
    }    
  }
  
  if(aqiOneDayHasDisplayed){
    is12pt = true;
    if(measureSize = 2.5){
      displayRect(oneDayRectXY, aqiColorSet(aqiOneDay[0]));      
      displayAQI(oneDayAQIXY, aqiOneDay[0]); 
    }
    else if(measureSize = 10){
      displayRect(oneDayRectXY, aqiColorSet(aqiOneDay[1]));      
      displayAQI(oneDayAQIXY, aqiOneDay[1]); 
    } 
  }
}



void buttonMonitor(){
  buttonState = digitalRead(buttonPin);
  
  if (buttonState == LOW && (millis() - buttonPressedTime) >= 500 && millis() >= 500 && !buttonInterrupt){  
        buttonPressedTime = millis();
        if (measureSize == 2.5){
          measureSize = 10;
          buttonPressedTime = buttonPressedTime - millis();
        }
        else if (measureSize == 10){
          measureSize = 2.5;
        }
        buttonInterrupt = true;
  } 
  else if (buttonState == HIGH) {
       //Do nothing
  }
};




void setup() {
  Serial.begin(9600);
  pms.passiveMode();
  pmsSensor1.init(); 
  oledDisplay1.init();
  pinMode(buttonPin, INPUT_PULLUP);  
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonMonitor, CHANGE);

}

void loop() {
  if(!buttonInterrupt){
    pmsSensor1.DoReading();
    aqiOverTime();
  }
  else{
    changeMeasurementActions();
    buttonInterrupt = false;
  }
}
