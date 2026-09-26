#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
inline uint32_t millis() { return 123; }
inline void delay(uint32_t) {}
struct FakeSerial {
  void begin(int) {}
  template<class... T> void printf(const char*, T...) {}
  void println(const char*) {}
  int available() { return 0; }
  char read() { return 0; }
};
inline FakeSerial Serial;
struct Bus {
  std::vector<std::string> events;
  std::vector<std::vector<uint8_t>> writes;
  std::vector<size_t> reads;
  bool failWrite = false;
  int getSDA() { return 31; }
  int getSCL() { return 32; }
  bool start(uint8_t a, bool r, uint32_t hz) {
    if (a != 0x29 || r || hz != 400000) return false;
    events.push_back("start"); return true;
  }
  bool restart(uint8_t a, bool r, uint32_t hz) {
    events.push_back("restart"); return a == 0x29 && r && hz == 400000;
  }
  bool write(const uint8_t* p, size_t n) {
    writes.emplace_back(p,p+n); return !failWrite;
  }
  bool read(uint8_t*, size_t n, bool nack) { reads.push_back(n); return nack; }
  bool stop() { events.push_back("stop"); return true; }
  bool scanID(uint8_t, uint32_t) { return false; }
};
struct FakeM5 {
  Bus In_I2C;
  FakeSerial Display;
  int config() { return 0; }
  void begin(int) {}
  void update() {}
};
inline FakeM5 M5;
