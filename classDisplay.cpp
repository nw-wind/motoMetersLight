#include <BigCrystal.h>
#include <LiquidCrystal.h>

#include "SmartDelay.h"
#include "SmartButton.h"

// Set up according to your LCD pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
BigCrystal bigCrystal(&lcd);

///////

#ifndef CLASS_DISPLAY_CPP
#define CLASS_DISPLAY_CPP

#include "SmartDelay.h"
#include "SmartButton.h"

const unsigned long SHOW_DELAY = 1000000UL; // period to display
const unsigned long BLINK_SHOW_DELAY = 500000UL; // period to show while blinking
const unsigned long BLINK_BLACK_DELAY = 250000UL; // period to black while blinking

class DisplayField {
  public:
    enum ShowMode {Show = 0, Clear, BlinkB, BlinkW};
    enum BlinkState {BlinkShow = 0, BlinkBlack, BlinkWhite};
    enum DisplayMode {ModeShow = 0, ModeClear, ModeBlack}; // Команды: рисовать, стирать и чернить
    enum FieldSize {SingleLine = 0, DoubleLine};
    enum DispInput {};
  private:
    byte r;
    byte c; // Rpw & Column
    byte len; // Length
    byte size = SingleLine;
    enum ShowMode md = Show;
    enum BlinkState st = BlinkShow;
    LiquidCrystal *lc;
    BigCrystal *bc;
    SmartDelay d;
  public:
    DisplayField() {};
    ~DisplayField() {};
    DisplayField(int pr, int pc, int pl) {
      r = pr;
      c = pc;
      len = pl;
    };

    void run() {
      if (d.Now()) {
        switch (md) {
          case Show:
            Display(ModeClear);
            Display(ModeShow);
            d.Set(SHOW_DELAY);
            break;
          case Clear:
            Display(ModeClear);
            d.Set(SHOW_DELAY);
            break;
          case BlinkB:
            switch (st) {
              case BlinkShow:
                Display(ModeBlack);
                d.Set(BLINK_BLACK_DELAY);
                st = BlinkBlack;
                break;
              case BlinkBlack:
                Display(ModeShow);
                d.Set(BLINK_SHOW_DELAY);
                st = BlinkShow;
            }
            break;
          case BlinkW:
            switch (st) {
              case BlinkShow:
                Display(ModeClear);
                d.Set(BLINK_BLACK_DELAY);
                st = BlinkBlack;
                break;
              case BlinkBlack:
                Display(ModeShow);
                d.Set(BLINK_SHOW_DELAY);
                st = BlinkShow;
            }
            break;
        }
      }
    }

    void Display(enum DisplayMode mode) {
      switch (mode) {
        case ModeShow:
          switch (size) {
            case SingleLine:
              lc->setCursor(c, r);
              lc->print(GetData());
              break;
            case DoubleLine:
              bc->printBig(GetData(), c, r);
              break;
          }
          break;
        case ModeClear:
          switch (size) {
            case SingleLine:
              lc->setCursor(c, r);
              for (int i = 0; i < len; i++) lc->print(' ');
              break;
            case DoubleLine:
              for (int j = 0; j < 2; j++) {
                bc->setCursor(c, r + j);
                for (int i = 0; i < len; i++) bc->print(' ');
              }
              break;
          }
          break;
        case ModeBlack:
          switch (size) {
            case SingleLine:
              lc->setCursor(c, r);
              for (int i = 0; i < len; i++) lc->print('\xFF');
              break;
            case DoubleLine:
              for (int j = 0; j < 2; j++) {
                bc->setCursor(c, r + j);
                for (int i = 0; i < len; i++) bc->print('\xFF');
              }
              break;
          }
          break;
      }
    }

    // Общение с объектом снаружи.
  public:
    void begin(int pr, int pc, int pl) {
      r = pr;
      c = pc;
      len = pl;
    };
    void SetDriverBig(BigCrystal*ptr) {
      bc = ptr;
      size = DoubleLine;

    }
    void SetDriverLcd(LiquidCrystal*ptr) {
      lc = ptr;
      size = SingleLine;

    }
    enum ShowMode GetMode() {
      return md;
    }
    enum ShowMode SetMode(enum ShowMode m) {
      enum ShowMode s = md; // Reset в мигании не даст мигать при нажатии кнопок
      if (m != md) d.Reset();
      md = m;
      return s;
    }
    void Redraw() {
      d.Reset();
    }
    virtual char *GetData() {} // Переопределять! Функция возвращает указатель на строку (у неё там статикой?) для вывода.
};

#endif

////////

class _odo : public DisplayField {
  private:
    unsigned long o;
  public:
    virtual char *GetData() {
      static char b[7];
      snprintf(b, 6, "%5ul", o);
      return b;
    }
    void Set(unsigned long odo) {
      o = odo;
    }
};

class _time : public DisplayField {
  public:
    virtual char *GetData() {
      static char b[7];
      snprintf(b, 6, "%02d:%02d", (int)((millis() / 60000L)%60), (int)((millis() / 1000L) % 60));
      return b;
    }
};

class _big : public DisplayField {
  private:
    int c;
  public:
    void Set(int odo) {
      c = odo;
    }
    virtual char *GetData() {
      static char b[7];
      snprintf(b, 6, "%3d", c);
      return b;
    }
};

// Поля.
_odo f1;
_time f2;
_big f3;

unsigned long odo = 0;
SmartDelay a1(200000UL);

DisplayField *z[] = {&f1, &f2, &f3};

void setup() {
  f1.begin(0, 0, 5); f1.SetDriverLcd(&lcd);
  f2.begin(1, 0, 5); f2.SetDriverLcd(&lcd);
  f3.begin(0, 8, 12); f3.SetDriverBig(&bigCrystal);
  lcd.begin(20, 2);
  lcd.clear();
}

int s = 0;
SmartDelay sd(247000UL);

byte flag;

void loop() {
  // Рисуем
  for (int i = 0; i < 3; i++) z[i]->run();
  // Мигаем часами, если счётчик больше чего-то там.
  // Обновляем счётчик
  if (a1.Now()) {
    odo += random(50);
    odo %= 5000UL;
    f1.Set(odo);
    f1.Redraw(); // Перерисовываем сразу, не раз в секунду, как остальное всё.
    // Мигаем часами или не мигаем?
    if (odo > 2000 && flag) {
      flag = 0;
      f2.SetMode(DisplayField::BlinkB);
      f3.SetMode(DisplayField::BlinkW);
    } else {
      flag = 1;
      f2.SetMode(DisplayField::Show);
      f3.SetMode(DisplayField::Show);
    }
  }
  // Крутим большие цифры
  if (sd.Now()) {
    f3.Set(s);
    s += random(5);
    s %= 250;
  }
}