#pragma once
#include <stdint.h>

// Byte rates include protocol traffic. Ignore small background packets so the
// cat can rest; hold short bursts long enough to be visible on the OLED.
class TrafficPet {
public:
  enum State : uint8_t { Setup, Offline, Sleep, Walk, Run };
  State state = Setup;
  void sample(uint32_t now, bool configured, bool online, uint32_t up, uint32_t down) {
    if (!configured || !online) {
      state = configured ? Offline : Setup;
      moving = sprinting = false;
      return;
    }
    const uint64_t rate = uint64_t(up) + down;
    if (rate >= 1024) { lastMove = now; moving = true; }
    if (rate >= 32768) { lastRun = now; sprinting = true; }
    if (sprinting && uint32_t(now-lastRun) >= 1500) sprinting = false;
    if (moving && uint32_t(now-lastMove) >= 2500) moving = false;
    state = sprinting ? Run : moving ? Walk : Sleep;
  }
  uint8_t frame(uint32_t now) const {
    return (now / (state == Walk || state == Run ? 250 : 500)) % 4;
  }
  const char* name() const { return nameOf(state); }
  static const char* nameOf(State s) {
    switch (s) {
      case Setup: return "SETUP";
      case Offline: return "OFFLINE";
      case Sleep: return "SLEEP";
      case Walk: return "WALK";
      default: return "RUN";
    }
  }
private:
  bool moving = false, sprinting = false;
  uint32_t lastMove = 0, lastRun = 0;
};

struct PetTestResult { unsigned passed = 0, total = 0; };
inline PetTestResult testTrafficPet() {
  TrafficPet p; PetTestResult result;
  auto check = [&](bool ok) { ++result.total; if (ok) ++result.passed; };
  p.sample(0,false,false,100000,100000); check(p.state==TrafficPet::Setup);
  p.sample(1,true,false,100000,100000); check(p.state==TrafficPet::Offline);
  p.sample(2,true,true,300,600); check(p.state==TrafficPet::Sleep);
  p.sample(1000,true,true,512,512); check(p.state==TrafficPet::Walk);
  p.sample(3499,true,true,0,0); check(p.state==TrafficPet::Walk);
  p.sample(3500,true,true,0,0); check(p.state==TrafficPet::Sleep);
  p.sample(4000,true,true,20000,20000); check(p.state==TrafficPet::Run);
  p.sample(5499,true,true,0,0); check(p.state==TrafficPet::Run);
  p.sample(5500,true,true,0,0); check(p.state==TrafficPet::Walk);
  p.sample(6500,true,true,0,0); check(p.state==TrafficPet::Sleep);
  p.sample(7000,true,true,100000,0);
  p.sample(7001,true,false,100000,0); check(p.state==TrafficPet::Offline);
  p.sample(7002,true,true,0,0); check(p.state==TrafficPet::Sleep);
  p.sample(0xffffff00,true,true,40000,0);
  p.sample(100,true,true,0,0); check(p.state==TrafficPet::Run);
  p.sample(1244,true,true,0,0); check(p.state==TrafficPet::Walk);
  p.sample(2244,true,true,0,0); check(p.state==TrafficPet::Sleep);
  p.sample(3000,true,true,0xffffffff,0xffffffff); check(p.state==TrafficPet::Run);
  check(p.frame(0)==0 && p.frame(250)==1 && p.frame(1000)==0);
  p.sample(3001,false,true,0,0); check(p.state==TrafficPet::Setup);
  return result;
}
