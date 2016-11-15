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
const float veloLength=veloDiameter*2.54/100*3.1416f;
const unsigned long veloDelay=1*100*1000UL; // 0.1 sec
const int veloPin=2;
smartDelay veloUpdate(veloDelay);

// Дисплей
static const unsigned long dispDelay=1*1000000UL; // Microseconds
smartDelay dispUpdate(dispDelay);

// Тахометр
#include <NeoPixelBus.h>
const uint16_t PixelCount = 16;  // Количество диодов в ленте
const uint16_t PixelPin = 6;      // Нога для диодов
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

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
  }
}

void setup() {
  pinMode(veloPin,INPUT);
  pinMode(rpmPin,INPUT);
  digitalWrite(veloPin,HIGH); // pull up
  digitalWrite(rpmPin,HIGH);   // pull down
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(rpmPin),intRpm,FALLING);
  attachInterrupt(digitalPinToInterrupt(veloPin),intVelo,FALLING);
  Serial.println("Ready");
}

void loop() {
  unsigned long mcs;
  
  // Вычислить обороты
  if (rpmUpdate.Now()) {
    mcs=micros();
    // вычисляем обороты
    if ((mcs-rpmTicks[0])>rpmMin && (mcs-rpmTicks[0])<rpmMax) {
      rpm=1000000UL*60/(mcs-rpmTicks[0]);
      sprintf(buf,"Good rpm tick: %ld %ld %d",mcs-rpmTicks[0],rpm,((mcs-rpmTicks[0])>rpmMax));
      Serial.println(buf);
    } 
    if ((mcs-rpmTicks[0])>rpmMax) rpm=0;
  }
  // Вычислить скорость
  if (veloUpdate.Now()) {
    mcs=micros();
    if ((mcs-veloTick)>veloMin && (mcs-veloTick)<veloMax) {
      velo=(veloLength/1000.0f)/((mcs-veloTick)/1000000.0f/3600.0f);
      //sprintf(buf,"Good velo tick: %ld %ld %d",mcs-veloTick,velo,((mcs-veloTick)>veloMax));
      //Serial.println(buf);
    } 
    if ((mcs-veloTick)>veloMax) velo=0;
  }
  // Показать на дисплее
  if (dispUpdate.Now()) {
    // Отрисовать дисплей
    sprintf(buf,"v=%ld r=%ld",velo,rpm);
    Serial.println(buf);
  }
}
