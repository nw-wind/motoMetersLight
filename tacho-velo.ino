#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <RTCx.h>
#include "FM24I2C.h"

/* 
 * Тахометр 
 * Спидометр
 * Одометр с сохранением в ЕЕПРОМ
 * Одометр (с сохранением) со сбросом
 * Средняя скорость
 * Средняя скорость с учётом остановок
 * Напряжение
 * Время круга
 */

// Smart Delay class
class smartDelay {
  private:
    unsigned long smMicros;
    unsigned long smLast;
  public:
    smartDelay(unsigned long tick);
    ~smartDelay();
    int Now();
    unsigned long Get();
    unsigned long Set(unsigned long tick);
};
smartDelay::smartDelay(unsigned long tick) {
  smMicros=tick;
}
smartDelay::~smartDelay() {}
int smartDelay::Now() {
  unsigned long mcs=micros();
  // Важно помнить про переполнение целого!
  if (mcs-smLast >= smMicros) {
    smLast=mcs;
    return true;
  } else {
    return false;
  }
}
unsigned long smartDelay::Get() {
  return smMicros;
}
unsigned long smartDelay::Set(unsigned long tick) {
  unsigned long old=smMicros;
  smMicros=tick;
  return old;
}
// end of Smart Delay class definition

static char buf[128];
const unsigned long maxLong=4294967295;

// Тахометр
// Количество тиков для усреднения
//#define RPMTICKS 30
static unsigned long rpm;                             // Обороты в минуту для отображения
volatile unsigned long rpmDist;
//static unsigned long rpmTicks[RPMTICKS];              // Отсчёты для усреднения
volatile int rpmTick=0;
// minTick < Интервал между прерываниями < maxTick
// Меньше: тик пропускается (многоискровое зажигание)
// Больше: rpm=0;
static const unsigned long rpmMin=1000000*60UL/30000;  // Минимальный тик
static const unsigned long rpmMax=1000000*60UL/100;    // Максимальный тик
static const unsigned int rpmDivide=1;      // Умножить для коррекции 
static const unsigned int rpmMultiply=1;    // Разделить для коррекции
const unsigned long rpmDelay=1*100*1000UL; // 0.1 sec
const int rpmPin=3;
smartDelay rpmUpdate(rpmDelay);

//  Спидометр
volatile unsigned long velo;
volatile unsigned long veloTick;
const unsigned long veloMin= 1* 100000UL;
const unsigned long veloMax= 1*1000000UL;
const float veloDiameter=21;
const float veloLength=veloDiameter*25.4*3.1416f;
const unsigned long veloDelay=1*100*1000UL; // 0.1 sec
const int veloPin=2;
smartDelay veloUpdate(veloDelay);

// Дисплей
static const unsigned long dispDelay=1*1000000UL; // Microseconds
smartDelay dispUpdate(dispDelay);
static byte displayColon=0;
const int displayAddress=0x27;
LiquidCrystal_PCF8574 lcd(displayAddress);
static byte displayOK=0;

// Одометр
static unsigned long odo;

// Температура
static int temp;
static byte tempOK=0;

// Моточасы
static unsigned int motoHours;
static unsigned int motoMinutes;
static unsigned long motoLast;
static unsigned long motoMillis;
const unsigned long motoDelay=1*60*1000000L;
smartDelay motoUpdate(motoDelay);

// FRAM
const int framAddress=0x57;
FM24I2C fram(framAddress);
static byte framOK=0;

// RTC
static byte rtcOK=0;

// Прерывания

void intRpm() {
  unsigned long mcs=micros();
  //sprintf(buf,"Rpm tick: %ld ",mcs-rpmTicks[0]);
  //Serial.println(buf);
  //if ((mcs-rpmDist)>rpmMin) {
  //  rpmDist=mcs;
    //Serial.println("Good rpm tick");
  //}
  rpmTick++;
}

void intVelo() {
  unsigned long mcs=micros();
  //sprintf(buf,"Velo tick: %ld ",mcs-veloTick);
  //Serial.println(buf);
  if ((mcs-veloTick)>veloMin) {
    veloTick=mcs;
    //Serial.println("Good velo tick");
    odo+=(unsigned long)(veloLength);
  }
}

// printAt()
#include <stdarg.h>
#define PRINTF_BUF 21
void printAt(int c, int r, char *s, ...) {
  char buf[PRINTF_BUF];
  va_list ap;
  va_start(ap, s);
  if (displayOK) {
    lcd.setCursor(c,r);
    vsnprintf(buf, sizeof(buf), s, ap);
    lcd.print(buf); 
  }
  va_end(ap);
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  
  pinMode(veloPin,INPUT_PULLUP);
  pinMode(rpmPin,INPUT_PULLUP);
  //digitalWrite(veloPin,HIGH); // pull up
  //digitalWrite(rpmPin,HIGH);   // pull down
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(rpmPin),intRpm,RISING);
  attachInterrupt(digitalPinToInterrupt(veloPin),intVelo,FALLING);

  Wire.beginTransmission(displayAddress);
  if (Wire.endTransmission(true) == 0) {
    displayOK=true;
  } else {
    displayOK=false;
  }

  if (displayOK) {
    lcd.begin(16, 2);
    lcd.setBacklight(200);
    lcd.noCursor();
    lcd.home(); 
    lcd.clear();
    printAt(0,0,"%16s","DISPLAY OK");
  }
  delay(2000);

  // Добавить в класс!
  Wire.beginTransmission(framAddress);
  if (Wire.endTransmission(true) == 0) {
    framOK=true;
    printAt(0,0,"%16s","FRAM OK");
  } else {
    framOK=false;
    printAt(0,0,"%16s","FRAM ERROR");
  }
 
  delay(2000);

  uint8_t addressList[] = {RTCx::MCP7941xAddress, RTCx::DS1307Address};
  if (rtc.autoprobe(addressList, sizeof(addressList))) {
    rtc.enableBatteryBackup();
    rtc.startClock();
    rtcOK=true;
    printAt(0,0,"%16s","RTC OK");
  } else {
    printAt(0,0,"%16s","RTC ERROR");
  }
  delay(2000);
  if (displayOK) lcd.clear();
  Serial.println("Ready");
}

void displayClock() {
  struct RTCx::tm tm;
  char buf[17];
  if (rtcOK) {
    // Читать часы
    rtc.readClock(tm);
    // Вывести их
    sprintf(buf,"%02d%s%02d",tm.tm_hour,(displayColon?":":" "),tm.tm_min,tm.tm_sec);
    lcd.setCursor(11, 1);
    lcd.print(buf);
    displayColon=(displayColon+1)%2;
  }
}

void displaySpeed() {
  printAt(0,0,"%3d",velo);
}

void displayRpm() {
  printAt(4,0,"%5d",rpm);
}

void displayOdo() {
  if (framOK) {
    printAt(10,0,"%6ld",odo/1000000L);
  }
}

void displayTemp() {
  int t=(temp>0?temp:0);
  if (tempOK) {
    printAt(0,1,"%3d",t);
  }
}

void displayMH() {
  printAt(4,1,"%3u:%02u",motoHours,motoMinutes);
}

void loop() {
  unsigned long mcs=micros();;
  
  // Вычислить обороты
  if (rpmUpdate.Now()) {
    // вычисляем обороты
    /*
    unsigned long rd=rpmDist;
    if ((mcs-rd)>rpmMin && (mcs-rd)<rpmMax) {
      rpmTicks[rpmTick]=1000000UL*60/(mcs-rd);
      sprintf(buf,"Good rpm tick: %ld %ld",mcs-rpmTicks[rpmTick],rpm);
      Serial.println(buf);
    } 
    if ((mcs-rd)>rpmMax) { 
      rpmTicks[rpmTick]=0;
    }
    rpm=0;
    for (int i=0; i<RPMTICKS; i++) {
      rpm+=rpmTicks[i];
    }
    rpm/=RPMTICKS;
    rpmTick=(rpmTick+1)%RPMTICKS;
    */
    /// TODO:
    // Добавить минимальное и максимальное.
    rpm=1000000L*60L/((mcs-rpmDist)/rpmTick);
    rpmTick=0;
    rpmDist=mcs;
  }
  // Вычислить скорость
  if (veloUpdate.Now()) {
    unsigned long vt=veloTick;
    if ((mcs-vt)>veloMin && (mcs-vt)<veloMax) {
      velo=(veloLength/1000.0f/1000.0f)/((mcs-vt)/1000000.0f/3600.0f);
      //sprintf(buf,"Good velo tick: %ld %ld %d",mcs-veloTick,velo,((mcs-veloTick)>veloMax));
      //Serial.println(buf);
    } 
    if ((mcs-vt)>veloMax) velo=0;
  }
  // Моточасы
  if (motoUpdate.Now()) {
    if (rpm) {
      // Часы идут
      motoMillis+=(mcs-motoLast)/1000L;
      motoHours=motoMillis/1000/60/60;
      motoMinutes=(motoMillis/1000/60)%60;
    } else {
      // Часы стоят
      
    }
    motoLast=mcs;
  }
  // Показать на дисплее
  if (dispUpdate.Now()) {
    // Отрисовать дисплей
    displayClock();
    displaySpeed();
    displayRpm();
    displayOdo();
    displayTemp();
    displayMH();
    //sprintf(buf,"v=%ld r=%ld",velo,rpm);
    //Serial.println(buf);
  }
}
