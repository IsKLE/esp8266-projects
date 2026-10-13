#pragma once
#include <Adafruit_GFX.h>
#include "TrafficPet.h"

// Draw straight into the existing 1 KiB framebuffer. No sprite allocation,
// floating point, delay, network requests or flash writes are needed.
inline void drawTrafficCat(Adafruit_GFX& g, TrafficPet::State state, uint8_t frame) {
  constexpr uint16_t white = 1, black = 0;
  const bool asleep = state == TrafficPet::Sleep;
  const bool offline = state == TrafficPet::Offline || state == TrafficPet::Setup;
  const bool running = state == TrafficPet::Run;
  const bool moving = state == TrafficPet::Walk || running;
  const int bob = moving && (frame & 1) ? -2 : asleep && frame == 2 ? 1 : 0;
  const int y = 44 + bob;

  // Ground and tail: the tail swishes while walking, droops when disconnected.
  g.drawFastHLine(5,89,54,white);
  if (offline) {
    g.drawLine(46,73,55,78,white); g.drawLine(55,78,57,85,white);
    g.drawLine(47,75,54,80,white);
  } else {
    const int tip = asleep ? 72 : 54 + (frame & 1)*5;
    g.drawLine(45,74,55,66,white); g.drawLine(46,76,57,67,white);
    g.drawLine(55,66,55,tip,white); g.drawLine(57,67,57,tip,white);
    g.drawFastHLine(52,tip,5,white);
  }
  g.fillRoundRect(17,65+bob,29,18,5,white);
  // Four poses alternate feet; sprint frames have a wider stride.
  const int stride = running ? 5 : 2;
  const int step = moving ? ((frame & 1) ? stride : -stride) : 0;
  g.fillRoundRect(19+step,79+bob,8,7,2,white);
  g.fillRoundRect(36-step,79+bob,8,7,2,white);
  g.drawFastVLine(30,73+bob,9,black);
  g.drawFastHLine(22,69+bob,18,black);

  // Pointed ears, solid white head and dark facial details.
  g.fillTriangle(17,y+8,18,y-11,30,y,white);
  g.fillTriangle(34,y,46,y-11,47,y+8,white);
  g.fillTriangle(20,y-5,21,y-7,26,y,black);
  g.fillTriangle(38,y,43,y-7,44,y-5,black);
  g.fillRoundRect(15,y,34,27,6,white);
  const bool blink = !offline && !asleep && frame == 3;
  if (asleep || blink) {
    g.drawFastHLine(21,y+10,6,black); g.drawFastHLine(37,y+10,6,black);
    if (asleep) { g.drawPixel(21,y+9,black); g.drawPixel(42,y+9,black); }
  } else if (offline) {
    g.drawLine(21,y+7,26,y+9,black); g.drawLine(38,y+9,43,y+7,black);
    g.fillRect(23,y+11,2,3,black); g.fillRect(39,y+11,2,3,black);
  } else {
    g.fillRoundRect(22,y+7,4,running?8:6,1,black);
    g.fillRoundRect(39,y+7,4,running?8:6,1,black);
  }
  g.fillTriangle(29,y+16,35,y+16,32,y+19,black);
  if (offline) g.drawCircle(32,y+22,2,black);
  else {
    g.drawLine(32,y+19,29,y+22,black); g.drawLine(32,y+19,35,y+22,black);
  }
  g.drawFastHLine(10,y+16,11,white); g.drawFastHLine(44,y+16,11,white);
  g.drawLine(11,y+20,20,y+19,white); g.drawLine(45,y+19,54,y+20,white);

  g.setTextSize(1); g.setTextColor(white); g.setTextWrap(false);
  if (asleep) {
    g.setCursor(47,24-(frame&1)*2); g.print('z');
    g.setCursor(53,16-(frame&1)*2); g.print('Z');
  } else if (offline) {
    g.setCursor(28,19); g.print('?');
  } else if (moving) {
    const int shift = frame*3;
    g.drawFastHLine(2,72,4+(shift%7),white);
    if (running) {
      g.drawFastHLine(1,62,8,white); g.drawFastHLine(3,81,7,white);
    }
  }
}
