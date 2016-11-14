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

// Тахометр
// Количество тиков для усреднения
#define RPMTICKS 8
volatile unsigned long rpm;                             // Обороты в минуту для отображения
volatile unsigned long rpmTicks[RPMTICKS];              // Отсчёты для усреднения
// minTick < Интервал между прерываниями < maxTick
// Меньше: тик пропускается (многоискровое зажигание)
// Больше: rpm=0;
static const unsigned long rpmMinTick=30000*1000000/60;  // Минимальный тик
static const unsigned long rpmMaxTick=100*1000000/60;    // Максимальный тик
static const unsigned int rpmDivide=1;      // Умножить для коррекции 
static const unsigned int rpmMultiply=1;    // Разделить для коррекции

//  Спидометр
volatile unsigned long velo;
volatile unsigned long veloTick;
const unsigned long veloMin= 1* 100000;
const unsigned long veloMax=80*1000000;
const unsigned float veloDiameter=21;
const unsigned float veloLength=veloDiameter*2.54/100*3.1416f;

// Дисплей
static const unsigned long dispDelay=1*1000*1000; // Microseconds
smartDelay dispUpdate(dispDelay);

// Прерывания

void intTacho() {
  
}

void intVelo() {
  unsigned long mcs=micros();
  if (mcs-veloTick>veloMax) {
    velo=0;
  } else {
    if (mcs-veloTick<veloMin) {
      // ничего
    } else {
      velo=(veloLength/1000)/((mcs-veloTick)/1000000);
      veloTick=mcs;
    }
  }
}

void setup() {
  

}

void loop() {
  
  // Показать на дисплее
  if (dispUpdate.Now()) {
    // Отрисовать дисплей
  }
}
