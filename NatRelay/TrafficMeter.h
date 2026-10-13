#pragma once
#include <stdint.h>

class TrafficMeter {
public:
  uint32_t upBps = 0, downBps = 0, peakUpBps = 0, peakDownBps = 0;
  void begin(uint32_t now, uint32_t up, uint32_t down) {
    at = now; lastUp = up; lastDown = down;
    upBps = downBps = peakUpBps = peakDownBps = 0;
  }
  bool sample(uint32_t now, uint32_t up, uint32_t down) {
    const uint32_t elapsed = now - at;
    if (elapsed < 1000) return false;
    upBps = uint64_t(uint32_t(up-lastUp))*1000/elapsed;
    downBps = uint64_t(uint32_t(down-lastDown))*1000/elapsed;
    if (upBps > peakUpBps) peakUpBps = upBps;
    if (downBps > peakDownBps) peakDownBps = downBps;
    lastUp = up; lastDown = down; at = now;
    return true;
  }
private:
  uint32_t at = 0, lastUp = 0, lastDown = 0;
};

struct TrafficTestResult { unsigned passed = 0, total = 0; };
inline TrafficTestResult testTrafficMeter() {
  TrafficMeter m; TrafficTestResult result;
  auto check = [&](bool ok) { ++result.total; if(ok) ++result.passed; };
  m.begin(0,0,0);
  check(!m.sample(999,2000,4000));
  check(m.sample(1000,2000,4000)); check(m.upBps==2000 && m.downBps==4000);
  check(m.sample(3000,4000,5000)); check(m.upBps==1000 && m.downBps==500);
  check(m.peakUpBps==2000 && m.peakDownBps==4000);
  check(m.sample(4000,4000,5000)); check(m.upBps==0 && m.downBps==0);
  check(m.peakUpBps==2000 && m.peakDownBps==4000);
  m.begin(0xfffffc18,0xffffff00,0xfffffe00);
  check(m.sample(0,0x300,0x600)); check(m.upBps==1024 && m.downBps==2048);
  return result;
}
