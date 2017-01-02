#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <RTCx.h>
#include "FM24I2C.h"

/*
   Тахометр
   Спидометр
   Одометр с сохранением в FRAM
   Одометр (с сохранением) со сбросом
   Средняя скорость
   Средняя скорость с учётом остановок
   Напряжение
   Время круга
*/

// Saved data
typedef struct FramSaved {
  unsigned long   odoM;   // В метрах
  unsigned long   motoS;  // Секунды от первого старта
  unsigned long   odoM1;  // Одометр 1
  unsigned long   odoM2;  // Одометр 2
} FramSaved;

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
  smMicros = tick;
}
smartDelay::~smartDelay() {}
int smartDelay::Now() {
  unsigned long mcs = micros();
  // Важно помнить про переполнение целого!
  if (mcs - smLast >= smMicros) {
    smLast = mcs;
    return true;
  } else {
    return false;
  }
}
unsigned long smartDelay::Get() {
  return smMicros;
}
unsigned long smartDelay::Set(unsigned long tick) {
  unsigned long old = smMicros;
  smMicros = tick;
  return old;
}
// end of Smart Delay class definition

static char buf[128];
const unsigned long maxLong = 4294967295;

// Тахометр
// Количество тиков для усреднения
//#define RPMTICKS 30
static unsigned long rpm;                             // Обороты в минуту для отображения
static unsigned long rpmDist;
//static unsigned long rpmTicks[RPMTICKS];              // Отсчёты для усреднения
volatile int rpmTick = 0;
volatile unsigned long rpmMicros = 0;
// minTick < Интервал между прерываниями < maxTick
// Меньше: тик пропускается (многоискровое зажигание)
// Больше: rpm=0;
static const unsigned long rpmMin = 1000000 * 60UL / 30000; // Минимальный тик
static const unsigned long rpmMax = 1000000 * 60UL / 100; // Максимальный тик
static const unsigned int rpmDivide = 1;    // Умножить для коррекции
static const unsigned int rpmMultiply = 1;  // Разделить для коррекции
const unsigned long rpmDelay = 2 * 100 * 1000UL; // 0.2 sec
const int rpmPin = 3; // pin 3
smartDelay rpmUpdate(rpmDelay);

//  Спидометр
static unsigned long velo;
volatile unsigned long veloTick;
volatile unsigned long veloMicros;
static unsigned long veloDist;
const unsigned long veloMin = 1 * 100000UL;
const unsigned long veloMax = 1 * 1000000UL;
const float veloDiameter = 21;
const float veloLength = veloDiameter * 25.4 * 3.1416f;
const unsigned long veloDelay = 2 * 100 * 1000UL; // 0.1 sec
const int veloPin = 2; // pin 2
smartDelay veloUpdate(veloDelay);

// Дисплей
static const unsigned long dispDelay = 1 * 1000000UL; // Microseconds
smartDelay dispUpdate(dispDelay);
static byte displayColon = 0;
const int displayAddress = 0x27;
LiquidCrystal_PCF8574 lcd(displayAddress);
static byte displayOK = 0;

// Одометр
static unsigned long odoM; // м
static unsigned long odoCm; // см
const unsigned long odoDelay = 1 * 1000UL * 1000UL;
smartDelay odoUpdate(odoDelay);
const unsigned long odoDelayKm = 10000UL; // 100m by cm

// Температура
static int temp;
static byte tempOK = 0;

// Моточасы
static unsigned int motoHours;
static unsigned int motoMinutes;
static unsigned long motoLast;
static unsigned long motoSec;
static unsigned long motoMillis;
const unsigned long motoDelay = 1 * 60 * 1000000L;
const unsigned long motoDelaySec = 1 * 1 * 1000000L;
smartDelay motoUpdate(motoDelay);
smartDelay motoUpdateSec(motoDelaySec);

// FRAM
const int framAddress = 0x57;
FM24I2C fram(framAddress);
static byte framOK = 0;
FramSaved saved;
const unsigned long framDelay = 10 * 1000L * 1000L; // частота записи в fram
const int framOffset = 0x00;
smartDelay framUpdate(framDelay);

// RTC
static byte rtcOK = 0;

// keep alive led
smartDelay ledUpdate(1000000UL);
static byte ledState = HIGH;

// Прерывания

void intRpm() {
  /// TODO если не слишком быстро...
  rpmTick++;
  rpmMicros = micros();
}

void intVelo() {
  veloTick++;
  veloMicros = micros();
  odoCm += (unsigned long)(veloLength / 10.0f ); // /100
}

// printAt()
#include <stdarg.h>
#define PRINTF_BUF 21
void printAt(int c, int r, const char *s, ...) {
  char buf[PRINTF_BUF];
  va_list ap;
  va_start(ap, s);
  if (displayOK) {
    lcd.setCursor(c, r);
    vsnprintf(buf, sizeof(buf), s, ap);
    lcd.print(buf);
  }
  va_end(ap);
}

void setup() {
  Wire.begin();
  Serial.begin(9600);

  pinMode(veloPin, INPUT_PULLUP);
  pinMode(rpmPin, INPUT_PULLUP);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  //digitalWrite(veloPin,HIGH); // pull up
  //digitalWrite(rpmPin,HIGH);   // pull down
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(rpmPin), intRpm, RISING);
  attachInterrupt(digitalPinToInterrupt(veloPin), intVelo, RISING);

  Wire.beginTransmission(displayAddress);
  if (Wire.endTransmission(true) == 0) {
    displayOK = true;
  } else {
    displayOK = false;
  }

  if (displayOK) {
    lcd.begin(16, 2);
    lcd.setBacklight(200);
    lcd.noCursor();
    lcd.home();
    lcd.clear();
    printAt(0, 0, "%16s", "DISPLAY OK");
  }
  delay(2000);

  // Добавить в класс!
  Wire.beginTransmission(framAddress);
  if (Wire.endTransmission(true) == 0) {
    framOK = true;
    printAt(0, 0, "%16s", "FRAM OK");
    fram.unpack(framOffset, &saved, sizeof(saved));
    odoM = saved.odoM;
    motoSec = saved.motoS;
    motoHours = motoSec / 60 / 60;
    motoMinutes = (motoSec / 60) % 60;
    //printAt(0, 1, "%ld %ld", odoM, motoSec);
  } else {
    framOK = false;
    printAt(0, 0, "%16s", "FRAM ERROR");
  }

  delay(2000);

  uint8_t addressList[] = {RTCx::MCP7941xAddress, RTCx::DS1307Address};
  if (rtc.autoprobe(addressList, sizeof(addressList))) {
    rtc.enableBatteryBackup();
    rtc.startClock();
    rtcOK = true;
    printAt(0, 0, "%16s", "RTC OK");
  } else {
    printAt(0, 0, "%16s", "RTC ERROR");
  }

  //if (!rtc.setSQW(RTCx::freq4096Hz)) Serial.println("Fail to set SW");

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
    sprintf(buf, "%02d%s%02d", tm.tm_hour, (displayColon ? ":" : " "), tm.tm_min, tm.tm_sec);
    lcd.setCursor(11, 1);
    lcd.print(buf);
    displayColon = (displayColon + 1) % 2;
  }
}

void displaySpeed() {
  printAt(0, 0, "%3d", velo);
}

void displayRpm() {
  printAt(4, 0, "%5ld", rpm);
}

void displayOdo() {
  if (framOK) {
    printAt(10, 0, "%6ld", odoM / 1000UL);
  }
}

void displayTemp() {
  int t = (temp > 0 ? temp : 0);
  if (tempOK) {
    printAt(0, 1, "%3d", t);
  }
}

void displayMH() {
  printAt(4, 1, "%3u:%02u", motoHours, motoMinutes);
}

unsigned long CalcRPM() {
  /// TODO:
  // Добавить минимальное и максимальное.
  if (rpmTick > 0) {
    rpm = 1000000L * 60L / ((rpmMicros - rpmDist) / rpmTick);
    //rpm /= 100; /////
    rpmTick = 0;
    rpmDist = rpmMicros;
  } else {
    rpm = 0;
  }
  if (rpm > 30000) rpm = 0;
  //Serial.println(rpm);
  return rpm;
}

unsigned long  CalcSpeed() {
  if (veloTick > 0) {
    velo = veloLength / ((float)(veloMicros - veloDist) / veloTick / 3600.f);
    //velo /= 100;
    //sprintf(buf, "Good velo tick: %ld %ld %ld", velo, veloMicros - veloDist, veloTick);
    //Serial.println(buf);
    veloTick = 0;
    veloDist = veloMicros;
  } else {
    velo = 0;
  }
  if (velo > 400) velo = 0;
  return velo;
}

void loop() {
  unsigned long mcs = micros();
  unsigned long mls = millis();

  if (ledUpdate.Now()) {
    ledState = (ledState == HIGH ? LOW : HIGH);
    digitalWrite(13, ledState);
  }
  // Вычислить обороты
  if (rpmUpdate.Now()) rpm = CalcRPM();
  // Вычислить скорость
  if (veloUpdate.Now()) velo = CalcSpeed();
  // Моточасы
  if (motoUpdateSec.Now()) {
    if (rpm) {
      // Часы идут
      //Serial.println("MH ticks");
      motoMillis += mls - motoLast;
    } else {
      // Часы стоят
      //Serial.println("MH sleeps");
    }
    motoLast = mls;
  }
  if (motoUpdate.Now()) {
    Serial.println("MH updates");
    motoSec += motoMillis / 1000UL;
    motoHours = motoSec / 60 / 60;
    motoMinutes = (motoSec / 60) % 60;
    motoMillis = 0;
  }
  if (odoUpdate.Now()) {
    if (odoCm > odoDelayKm) {
      // пора обновить одометр в метрах
      odoM += odoCm / 100;
      odoCm = 0;
    }
  }
  if (framUpdate.Now() && framOK) {
    saved.odoM = odoM;
    saved.motoS = motoSec;
    fram.pack(framOffset, &saved, sizeof(saved));
    //Serial.println("Fram saved");
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
