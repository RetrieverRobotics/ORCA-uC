#include <font_Arial.h>
#include <font_ArialBold.h>
#include <ILI9341_t3.h>
#include <XPT2046_Touchscreen.h>

/*
 * Display pins:
 * T_IRQ - 2, optional, any pin
 * T_DO - 12
 * T_DIN - 11
 * T_CS - 8, or any pin
 * T_CLK - 13
 * ---------------------------------------
 * MISO (DIN) - 12
 * LED - VIN, 3.5-5.5V
 * SCK - 13
 * MOSI (DOUT) - 11
 * D/C - 9
 * RESET - +3.3V
 * CS - 10
 * GND - GND
 * VCC - VIN, 3.5-5.5V, so USB supply
 */

#include <Servo.h>

// wraps the handling of a Blue Robotics
// brushless motor esc as a Servo
// maybe also wrap a pid controller, reversal, etc
class BLThruster {
  private:
    Servo esc; // esc = electronic speed controller
    int pwm_pin;
    bool reversed;

    int power;
    bool enabled;

    const static int ESC_MIN;
    const static int ESC_MAX;
    const static int ESC_HALF;

    void update() {
      // pid if implemented, update esc pwm
      int pwr = power;
      pwr = (reversed ? -1*power : power);
      int us = map(pwr, -100, 100, BLThruster::ESC_MIN, BLThruster::ESC_MAX);
      esc.writeMicroseconds(us);
      Serial.println(us);
    }
  
  public:
    BLThruster(int _pwm_pin, boolean _reversed = false) {
      pwm_pin = _pwm_pin;
      esc.attach(pwm_pin);
      // blue robotics esc has forward reverse capability,
      // so write a value in the middle of range to turn it off
      esc.writeMicroseconds(BLThruster::ESC_HALF);

      reversed = _reversed;

      power = 0;
      enabled = false;
    }

    void setPower(int _power) {
      // not applying reversal here b/c a call to getPower()
      // might return a confusing result
      if(enabled) {
        power = _power;
      }
      update();
    }
    void changePower(int delta) {
      if(enabled) {
        power += delta;
        power = constrain(power, -100, 100);
        update();
      }
    }

    int getPower(void) {
      return power;
    }

    void setEnabled(bool en) {
      enabled = en;
      if(!en) power = 0;
      update();
    }
    bool isEnabled() {
      return enabled;
    }
};

// these are static (shared between instances),
// which makes the initialization weird
const int BLThruster::ESC_MIN = 1100;
const int BLThruster::ESC_MAX = 1900;
const int BLThruster::ESC_HALF = (BLThruster::ESC_MAX + BLThruster::ESC_MIN)/2;

#define TFT_DC (9)
#define TFT_CS (10)
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);

#define F_WID (0)
#define F_HGT (1)
#define TEXT_BASE_W (5)
#define TEXT_BASE_H (7)
int fontDim(int dim) {
  if(dim == F_WID) return TEXT_BASE_W*tft.getTextSize();
  if(dim == F_HGT) return TEXT_BASE_H*tft.getTextSize();
  return 0;
}

#define CS_PIN (8)
XPT2046_Touchscreen touch(CS_PIN);
#define TIRQ_PIN (2)
//XPT2046_Touchscreen ts(CS_PIN);  // Param 2 - NULL - No interrupts
//XPT2046_Touchscreen ts(CS_PIN, 255);  // Param 2 - 255 - No interrupts
//XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling

int touch_x_max_left = 3700;
int touch_x_min_right = 150;
int touch_y_max_top = 3750;
int touch_y_min_bottom = 320;

#include "Button.h"
Button touch_point;

bool isTouched() {
  if(touch.touched()) {
    TS_Point p = touch.getPoint();
    if(touch_x_max_left > p.x && p.x > touch_x_min_right) {
      if(touch_y_max_top > p.y && p.y > touch_y_min_bottom) {
        if(p.z < 3000) {
          return true; // touch screen data corrupted -> z returns 4095
        } else {
          Serial.println("bad touch");
        }
      }
    }
  }
  return false;
}

int getMappedX(int t_x) {
  return map(t_x, touch_x_max_left, touch_x_min_right, 0, tft.width());
}
int getMappedY(int t_y) {
  return map(t_y, touch_y_max_top, touch_y_min_bottom, 0, tft.height());
}

// [boolean] .touched()
// [TS_Point] .getPoint(), .x .y .z
// this controller is sometimes buggy, but probably just wire noise, anyway
// check and disregard values where .z > 3000, should be good enough

#define NUM_THRUSTERS (6)
const uint8_t esc_pins[] = { 2, 3, 4, 5, 6, 7 };
const bool esc_reversed[] = { false, false, false, false, false, false };
BLThruster* thrusters[NUM_THRUSTERS];

int margin = 5;
int reserved_left = 50;







void setup() {
  // init screen
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_WHITE);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);

  tft.setCursor(5,5);
  tft.println(F("Initializing..."));

  touch.begin();
  
  // start serial
  Serial.begin(19200);

  // init the controller objects
  for(int i = 0; i < NUM_THRUSTERS; i++) {
    thrusters[i] = new BLThruster(esc_pins[i], esc_reversed[i]);
    tft.print(" ESC ");
    tft.print(i+1);
    tft.print(" on pin ");
    tft.print(esc_pins[i]);
    tft.print(", R:");
    tft.println((esc_reversed[i] ? "true" : "false"));
  }
  delay(3000);

  // set a longer debounce duration for the touchscreen touch detect
  touch_point.setDebounceDur(30);

  // finish
  tft.fillScreen(ILI9341_WHITE);
  tft.setCursor(margin, margin);
  tft.println(F("Done."));
  delay(1000);

  // labels never get changed, so might as well only draw them once
  tft.fillScreen(ILI9341_WHITE);
  tft.setCursor(margin, margin+5);
  tft.println(" EN:");
  tft.setCursor(margin, tft.height() - margin - fontDim(F_HGT) - 5);
  tft.println("Adj:");
}







int selected_esc = 0;


uint8_t refresh_rate = 40; // frames per second
uint16_t refresh_interval = 1000 / refresh_rate;
uint32_t last_display_refresh =0;

void loop() {
  uint32_t now = millis();

  touch_point.hwstate = isTouched();
  touch_point.update();

  if(now - last_display_refresh > refresh_interval) {
    // this checks the touch_point against esc enable/select buttons
    for(int i = 0; i < NUM_THRUSTERS; i++) {
      renderESCStatus(i);
    }

    // still need inc/dec/zero buttons, render and bound checks
    renderPowerAdjust();

    last_display_refresh = now;
  }
}








int render_box_wid = 40;
int render_box_hgt = tft.height() - (4*margin);

int btn_wid = 30;
int btn_hgt = 30;
int bar_hgt = 140;

int getOffsetX(int i) {
  return (reserved_left + i*render_box_wid + i*margin);
}

bool isInside(int _x, int _y, int x, int y, int wid, int hgt) {
  if(_x > x && _x < x+wid && _y > y && _y < y+hgt) return true; else return false;
}

void renderESCStatus(int esc) {

  tft.drawFastVLine(getOffsetX(esc), margin, render_box_hgt, ILI9341_LIGHTGREY);
  tft.drawFastVLine(getOffsetX(esc) + render_box_wid, margin, render_box_hgt, ILI9341_LIGHTGREY);

  int btn_x = getOffsetX(esc)+margin;
  int en_btn_y = margin;

  int bar_x = btn_x;
  int bar_y = en_btn_y+btn_hgt+margin;

  int adj_btn_x = btn_x;
  int adj_btn_y = tft.height() - margin - btn_hgt;

  if(touch_point.state > DISABLED) {
    TS_Point p = touch.getPoint();
    int tx = getMappedX(p.x);
    int ty = getMappedY(p.y);

    if(isInside(tx, ty, btn_x, en_btn_y, btn_wid, btn_hgt)) {
      touch_point.state = DISABLED;
      thrusters[esc]->setEnabled(!thrusters[esc]->isEnabled());
      // if this esc was just enabled change this to the selected esc
      // this seemed to be an obvious behavior once I messed with it -
      // I always expected to be adjusted the esc I had just enabled
      if(thrusters[esc]->isEnabled()) {
        selected_esc = esc;
      }

    } else if(isInside(tx, ty, adj_btn_x, adj_btn_y, btn_wid, btn_hgt)) {
      touch_point.state = DISABLED;
      selected_esc = esc;
    }
  }

  // enable button
  tft.fillRect(btn_x, en_btn_y, btn_wid, btn_hgt, (thrusters[esc]->isEnabled() ? ILI9341_GREEN : ILI9341_RED));

  // draw power indicator bar, first clear screen behind

  // then draw a vertical gradient bar either above or below center
  // depending on value of motor
  int bar_ctr_y = bar_y + (bar_hgt/2);
  int bar_range = bar_hgt/2; // +- this in pixels

  int pwr = thrusters[esc]->getPower();

  int this_hgt = map(abs(pwr), 0, 100, 0, bar_range);
  


  if(pwr > 0) {
    tft.fillRect(bar_x+2, bar_y, btn_wid-4, bar_range-this_hgt, ILI9341_WHITE);
    tft.fillRect(bar_x+2, bar_ctr_y-this_hgt, btn_wid-4, this_hgt, ILI9341_CYAN);
  } else if(pwr < 0) {
    tft.fillRect(bar_x+2, bar_ctr_y, btn_wid-4, this_hgt, ILI9341_CYAN);
    tft.fillRect(bar_x+2, bar_ctr_y+this_hgt, btn_wid-4, bar_range-this_hgt, ILI9341_WHITE);
  } else {
    tft.fillRect(bar_x+2, bar_y, btn_wid-4, bar_hgt, ILI9341_WHITE);
  }

  // draw a horizontal black bar for center
  tft.fillRect(bar_x, bar_ctr_y-2, btn_wid, 4, ILI9341_BLACK);

  // select button, color dependent on enable state
  tft.fillRect(adj_btn_x, adj_btn_y, btn_wid, btn_hgt, (esc == selected_esc ? ILI9341_BLACK : ILI9341_LIGHTGREY));

} 

int btn_x = margin*2; // multiplier arbitrary

int inc_btn_y = 50; // value arbitrary
int zero_btn_y = inc_btn_y + btn_hgt+margin;
int dec_btn_y = zero_btn_y + btn_hgt+margin;

int delta = 20;

void renderPowerAdjust() {
  if(touch_point.state > DISABLED) {
    touch_point.state = DISABLED;
    TS_Point p = touch.getPoint();
    int tx = getMappedX(p.x);
    int ty = getMappedY(p.y);

    if(isInside(tx, ty, btn_x, inc_btn_y, btn_wid, btn_hgt)) {
      thrusters[selected_esc]->changePower(delta);
    } else if(isInside(tx, ty, btn_x, zero_btn_y, btn_wid, btn_hgt)) {
      thrusters[selected_esc]->setPower(0);
    } else if(isInside(tx, ty, btn_x, dec_btn_y, btn_wid, btn_hgt)) {
      thrusters[selected_esc]->changePower(-1*delta);
    }
  }

  // up arrow - equilateral
  // p0: x, y+h
  // p1: x+w, y+h
  // p2: x+(w/2), y
 tft.fillTriangle(btn_x, inc_btn_y+btn_hgt, btn_x+btn_wid, inc_btn_y+btn_hgt, btn_x+(btn_wid/2), inc_btn_y, ILI9341_OLIVE);

  // zero button
  tft.fillRect(btn_x, zero_btn_y, btn_wid, btn_hgt, ILI9341_DARKGREY);

  // down arrow - equilateral
  // p0: x, y
  // p1: x+(w/2), y+h
  // p2: x+w, y
  tft.fillTriangle(btn_x, dec_btn_y, btn_x+(btn_wid/2), dec_btn_y+btn_hgt, btn_x+btn_wid, dec_btn_y, ILI9341_OLIVE);
}
/*
  kill [X] [X] [X] [X] [X] [X]
        _   _   _   _   _   _ 
   /\  | | | | | | | | | | | |
       | | | | | | | | | | | |
   []  |-| |-| |-| |-| |-| |-|
       | | | | | | | | | | | |
   \/  |_| |_| |_| |_| |_| |_|

select [1] [2] [3] [4] [5] [6]
*/


/*
Motor Testing:
  procedure:
    attach known functioning esc to motor (any wire config)
    attach appropriate battery to esc
    set esc to 20% power for about 3 sec
    set esc to 0%
    unplug battery

(X) - smokes, doesn't spin, etc.
(G)ood - spins
(I)ssue - can't test

   FRONT
   
   G   I1
G         G
   G   G

I1 - bullets connectors not soldered

ESC Testing:
  procedure:
    atttach known functioning motor to esc (any wire config)
    attach esc to battery
    gently feel for heat -> get ready to unplug!

2 good
4 bad, failed tantalum capacitor - overvolt?

*/