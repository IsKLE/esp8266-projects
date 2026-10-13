#pragma once
#include <stdint.h>

// One action on a debounced release. A held button does not repeat.
class FlashButton {
public:
  void begin(bool pressed, uint32_t now) {
    raw = stable = pressed; changedAt = now; armed = false;
  }
  bool sample(bool pressed, uint32_t now) {
    if (pressed != raw) { raw = pressed; changedAt = now; }
    if (raw != stable && uint32_t(now - changedAt) >= 40) {
      stable = raw;
      if (stable) armed = true;
      else if (armed) { armed = false; return true; }
    }
    return false;
  }
private:
  bool raw = false, stable = false, armed = false;
  uint32_t changedAt = 0;
};

struct ButtonTestResult { unsigned passed = 0, total = 0; };
inline ButtonTestResult testFlashButton() {
  FlashButton b; ButtonTestResult result;
  auto check = [&](bool pressed, uint32_t now, bool expected) {
    ++result.total; if (b.sample(pressed, now) == expected) ++result.passed;
  };
  b.begin(false, 0);
  check(true,10,false); check(false,20,false); check(true,30,false);
  check(true,69,false); check(true,70,false); // press is now stable
  check(false,71,false); check(true,80,false); check(false,90,false);
  check(false,129,false); check(false,130,true); check(false,200,false);
  check(true,210,false); check(true,250,false); check(true,500,false);
  check(false,510,false); check(false,550,true); // hold produces one action
  b.begin(true,0); // ignore a button already down at initialization
  check(false,10,false); check(false,50,false);
  check(true,60,false); check(true,100,false); check(false,110,false); check(false,150,true);
  b.begin(false,0xffffffd0);
  check(true,0xffffffe0,false); check(true,8,false);
  check(false,9,false); check(false,49,true); // millis() rollover
  return result;
}
