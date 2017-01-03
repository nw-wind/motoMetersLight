#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

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
    unsigned long Wait();
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
unsigned long smartDelay::Wait() {
  unsigned long old = smMicros;
  smMicros = micros();
  return old;
}
// end of Smart Delay class definition

const int displayAddress = 0x27;
LiquidCrystal_PCF8574 lcd(displayAddress);
static byte displayOK;

byte pos = 0;
int val[4] = {11, 22, 33, 44};
int col[4] = {0, 4, 8, 12};
int curPos = 0;
int button;
unsigned long buttonPressed;
unsigned long buttonReleased;
smartDelay valUp(600000);

const long Click = 25;
const long Press = 1000;
const long Push = 6000;
byte clickMark;
byte pressMark;
byte pushMark;
#define RELEASE 0
#define CLICK   1
#define PRESS   2
#define PUSH    3
byte buttonState = RELEASE;
smartDelay pressBt(500000);
byte pressBtSwitch;
byte buttonActionTime; // bool

#define CHOOSE_OFF 0
#define CHOOSE_ON  1
byte chooseMode = CHOOSE_OFF;
smartDelay chooseDelay(10000000);
byte current = 0;

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
  for (int i = 6; i <= 9; i++) {
    pinMode(i, INPUT_PULLUP);
  }
  Serial.begin(9600);
  delay(25);
  Wire.begin();
  Wire.beginTransmission(displayAddress);
  if (Wire.endTransmission(true) == 0) {
    displayOK = true;
  } else {
    displayOK = false;
    Serial.println("No display");
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
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    printAt(col[i], 0, "%3d", val[i]);
  }
}

#define MODE_CLEAR 0
#define MODE_DRAW  1
#define MODE_BLACK 3
smartDelay dispUp(1000000);

void disp0(byte mode) {
  switch (mode) {
    case MODE_CLEAR:
      printAt(col[0], 0, "   ");
      break;
    case MODE_DRAW:
      printAt(col[0], 0, "%3d", val[0]);
      break;
    case MODE_BLACK:
      printAt(col[0], 0, "\xFF\xFF\xFF");
      break;
  }
}

void disp1(byte mode) {
  switch (mode) {
    case MODE_CLEAR:
      printAt(col[1], 0, "   ");
      break;
    case MODE_DRAW:
      printAt(col[1], 0, "%3d", val[1]);
      break;
    case MODE_BLACK:
      printAt(col[1], 0, "\xFF\xFF\xFF");
      break;
  }
}

void disp2(byte mode) {
  switch (mode) {
    case MODE_CLEAR:
      printAt(col[2], 0, "   ");
      break;
    case MODE_DRAW:
      printAt(col[2], 0, "%3d", val[2]);
      break;
    case MODE_BLACK:
      printAt(col[2], 0, "\xFF\xFF\xFF");
      break;
  }
}

void disp3(byte mode) {
  switch (mode) {
    case MODE_CLEAR:
      printAt(col[3], 0, "   ");
      break;
    case MODE_DRAW:
      printAt(col[3], 0, "%3d", val[3]);
      break;
    case MODE_BLACK:
      printAt(col[3], 0, "\xFF\xFF\xFF");
      break;
  }
}


void (*disp[4])(byte mode) = {&disp0, &disp1, &disp2, &disp3};


void loop() {
  if (chooseDelay.Now()) {
    chooseMode = CHOOSE_OFF;
  }
  if (valUp.Now()) {
    for (int i = 0; i < 4; i++) {
      val[i]++;
      if (val[i] > 999) val[i] = 0;
    }

  }
  if (chooseMode == CHOOSE_ON) {
    if (pressBt.Now()) {
      if (pressBtSwitch) {
        (*disp[current])(MODE_BLACK);
      } else {
        (*disp[current])(MODE_DRAW);
      }
      for (int i = 0; i < 4; i++) if (i != current) (*disp[i])(MODE_DRAW);
      pressBtSwitch = !pressBtSwitch;
    }
  } else {
    if (dispUp.Now()) {
      for (int i = 0; i < 4; i++) (*disp[i])(MODE_DRAW);
    }

  }
  for (int i = 6; i <= 9; i++) {
    //Serial.print(" ");
    if (digitalRead(i)) {
      //Serial.print(i);
      if (button == i) {
        buttonReleased = millis();
        button = 0;
        // что делать?
        //Serial.print("Button ");
        //Serial.print(i);
        //Serial.print(" Released ");
        //Serial.print(buttonReleased - buttonPressed);
        //Serial.println();
        long period = buttonReleased - buttonPressed;
        if (period > Click) {
          if (period > Press) {
            if (period > Push) {
              // Push
              //Serial.println("Push OFF");
              printAt(col[i - 6], 1, "   ");
            } else {
              // Press
              //Serial.println("Press OFF");
              printAt(col[i - 6], 1, "   ");
            }
          } else {
            // Click
            //Serial.println("Click OFF");
            printAt(col[i - 6], 1, "   ");
            buttonActionTime = 0;
          }
          clickMark = pressMark = pushMark = 0;
        } else {
          // Nothing
        }
        buttonState = RELEASE;
      }
    } else {
      //Serial.print("*");
      chooseDelay.Wait();
      if (button == 0) {
        button = i;
        buttonPressed = millis();
        //Serial.print(i);
        //Serial.println(" Pressed");
      } else {
        // Still pressed
        long period = millis() - buttonPressed;
        //Serial.print(period);
        //Serial.print(" period ");
        if (period > Click) {
          if (period > Press) {
            if (period > Push) {
              // Push
              //Serial.println("Push ON");
              if (!pushMark) {
                printAt(col[i - 6], 1, "\xFF\xFF\xFF");
                pushMark = 1;
                pressMark = clickMark = 0;
                buttonState = PUSH;
                // Действие
                if (chooseMode==CHOOSE_OFF) {
                val[i - 6] = 0;
                (*disp[i - 6])(MODE_DRAW);
                } else {
                  val[current] = 0;
                (*disp[current])(MODE_DRAW);
                }
                chooseMode = CHOOSE_OFF;
              }
            } else {
              // Press
              //Serial.println("Press ON");
              if (!pressMark) {
                printAt(col[i - 6], 1, "***");
                pushMark = clickMark = 0;
                buttonState = PRESS;
                //
                chooseMode = CHOOSE_ON;
              }
            }
          } else {
            // Click
            //Serial.println("Click ON");
            if (!clickMark) {
              printAt(col[i - 6], 1, "^^^");
              pressMark = pushMark = 0;
              buttonState = CLICK;
              //
              if (buttonActionTime == 0) {
                if (chooseMode == CHOOSE_ON) {
                  if (button == 6) current--;
                  if (button == 7) current++;
                  Serial.print("current="); Serial.println(current);
                  if (current > 200 ) current = 3;
                  if (current >= 4) current = 0;
                  Serial.print("Result current="); Serial.println(current);
                }
                buttonActionTime = 1;
              }
            }
          }
        } else {
          // Nothing
        }
      }
    }
  }
  //Serial.println();
  //delay(500);
}
