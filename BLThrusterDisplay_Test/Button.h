
#define UNPRESSED -1
#define TRIGGERED 0
#define DISABLED 1
#define PRESSED 2
#define HELD 3

void button_default(void) {}

class Button {
  private:
    void (*_onPress)(void) = &button_default;
    void (*_onHold)(void) = &button_default;
    
    void onPress() {
      _onPress();
      _onPress = &button_default;
    };
    void onHold() {
      _onHold();
      _onHold = &button_default;
    }

    int8_t prev_state = UNPRESSED;
    uint32_t timestamp = 0;
    uint8_t debounce_dur = 10;
    uint16_t hold_dur = 400;

  public:
    Button();
    Button(int);

    uint8_t pin;
    boolean hwstate = false; // hardware state as set by i2c, digitalread, etc
    int8_t state = UNPRESSED; // -1:none, 0:triggered, 1:pressed, 2:held, 3:disabled

    void setDebounceDur(int);
    void update();
    void handleHold(void (*__onHold)(void));
    void handlePress(void (*__onPress)(void));
    boolean justPressed();
    boolean justReleased();
};

Button::Button() {
  pin = -1;
}
Button::Button(int _pin) {
  pin = _pin;
}

void Button::setDebounceDur(int dur) {
  debounce_dur = (dur > 0 ? dur : debounce_dur);
}
void Button::update() {
  if(state < DISABLED) { // if not pressed or held
    if(!state == TRIGGERED) { // if not triggered
      if(hwstate && millis() - timestamp > (debounce_dur*5)) { // if pressed and enough delay after previous press
        timestamp = millis(); // get current timestamp
        prev_state = state;
        state = TRIGGERED; // trigger
      }
    } else { // triggered
      if(millis() - timestamp > debounce_dur) { // if debounce interval complete
        if(hwstate) { // if button is still pressed
          timestamp = millis(); // get current timestamp
          prev_state = state;
          state = PRESSED; // set state to pressed
          onPress(); // onPress callback if set (has default)
        }
      }
    }
  } else { // pressed or held
    if(!hwstate) { // if button is now not pressed
      prev_state = state;
      state = UNPRESSED; // disengage button
      timestamp = millis(); // get button released timestamp
    } else { // else button still pressed
      if(state == PRESSED && millis() - timestamp > hold_dur && state != DISABLED) { // make sure not already held and check if button has been pressed for hold duration
        prev_state = state;
        state = HELD; // set state to pressed and held
        onHold(); // onHold callback if set (has default)
      }
    }
  }
}

void Button::handleHold(void (*__onHold)(void)) {
  _onHold = __onHold;
}
void Button::handlePress(void (*__onPress)(void)) {
  _onPress = __onPress;
}

boolean Button::justReleased() {
  if(prev_state >= DISABLED && state == UNPRESSED) {
    prev_state = state;
    return true;
  } else {
    return false;
  }
}

boolean Button::justPressed() {
  if(prev_state < DISABLED && state > DISABLED) {
    prev_state = state;
    return true;
  } else {
    return false;
  }
}
