#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <RTCx.h>

LiquidCrystal_PCF8574 lcd(0x27);

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
#define RPMTICKS 8
volatile unsigned long rpm;                             // Обороты в минуту для отображения
volatile unsigned long rpmTicks[RPMTICKS];              // Отсчёты для усреднения
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

// Одометр
static unsigned long odo;

// Температура
static int temp;

// Моточасы
static unsigned int motoHours;
static unsigned int motoMinutes;
static unsigned long motoLast;
static unsigned long motoMillis;
const unsigned long motoDelay=1*60*1000000L;
smartDelay motoUpdate(motoDelay);

// Прерывания

void intRpm() {
  unsigned long mcs=micros();
  //sprintf(buf,"Rpm tick: %ld ",mcs-rpmTicks[0]);
  //Serial.println(buf);
  if ((mcs-rpmTicks[0])>rpmMin) {
    rpmTicks[0]=mcs;
    //Serial.println("Good rpm tick");
  }
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

void setup() {
  pinMode(veloPin,INPUT);
  pinMode(rpmPin,INPUT);
  digitalWrite(veloPin,HIGH); // pull up
  digitalWrite(rpmPin,HIGH);   // pull down
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(rpmPin),intRpm,RISING);
  attachInterrupt(digitalPinToInterrupt(veloPin),intVelo,FALLING);

  lcd.begin(16, 2);
  lcd.setBacklight(200);
  lcd.noCursor();
  lcd.home(); 
  lcd.clear();

  uint8_t addressList[] = {RTCx::MCP7941xAddress, RTCx::DS1307Address};
  rtc.autoprobe(addressList, sizeof(addressList));
  rtc.enableBatteryBackup();
  rtc.startClock();
  
  Serial.println("Ready");
}

// printAt()
#include <stdarg.h>
#define PRINTF_BUF 21
void printAt(int c, int r, char *s, ...) {
  char buf[PRINTF_BUF];
  va_list ap;
  va_start(ap, s);
  lcd.setCursor(c,r);
  vsnprintf(buf, sizeof(buf), s, ap);
  lcd.print(buf); 
  va_end(ap);
}

void displayClock() {
  struct RTCx::tm tm;
  char buf[17];
  // Читать часы
  rtc.readClock(tm);
  // Вывести их
  sprintf(buf,"%02d%s%02d",tm.tm_hour,(displayColon?":":" "),tm.tm_min,tm.tm_sec);
  lcd.setCursor(11, 1);
  lcd.print(buf);
  displayColon=(displayColon+1)%2;
}

void displaySpeed() {
  char buf[17];
  sprintf(buf,"%3d",velo);
  lcd.setCursor(0, 0);
  lcd.print(buf);
}

void displayRpm() {
  char buf[17];
  sprintf(buf,"%5d",rpm);
  lcd.setCursor(4, 0);
  lcd.print(buf);
}

void displayOdo() {
  char buf[17];
  sprintf(buf,"%6ld",odo/1000000L);
  lcd.setCursor(10, 0);
  lcd.print(buf);
}

void displayTemp() {
  char buf[17];
  int t=(temp>0?temp:0);
  sprintf(buf,"%3d",t);
  lcd.setCursor(0, 1);
  lcd.print(buf);
}

void displayMH() {
  char buf[17];
  sprintf(buf,"%3u:%02u",motoHours,motoMinutes);
  lcd.setCursor(4, 1);
  lcd.print(buf);
}

void loop() {
  unsigned long mcs=micros();;
  
  // Вычислить обороты
  if (rpmUpdate.Now()) {
    // вычисляем обороты
    if ((mcs-rpmTicks[0])>rpmMin && (mcs-rpmTicks[0])<rpmMax) {
      rpm=1000000UL*60/(mcs-rpmTicks[0]);
      sprintf(buf,"Good rpm tick: %ld %ld",mcs-rpmTicks[0],rpm);
      Serial.println(buf);
    } 
    if ((mcs-rpmTicks[0])>rpmMax) { 
      rpm=0;
    }
  }
  // Вычислить скорость
  if (veloUpdate.Now()) {
    if ((mcs-veloTick)>veloMin && (mcs-veloTick)<veloMax) {
      velo=(veloLength/1000.0f/1000.0f)/((mcs-veloTick)/1000000.0f/3600.0f);
      //sprintf(buf,"Good velo tick: %ld %ld %d",mcs-veloTick,velo,((mcs-veloTick)>veloMax));
      //Serial.println(buf);
    } 
    if ((mcs-veloTick)>veloMax) velo=0;
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
