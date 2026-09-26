// Host transport test, not an emulation of hardware or the M5 library.
#include <cassert>
#include "../../firmware/tof_bringup/tof_bringup.ino"
int main() {
  uint8_t data[300] = {};
  assert(transfer(0x1234, data, 300, true) == 0);
  assert((M5.In_I2C.reads == std::vector<size_t>{128,128,44}));
  assert((M5.In_I2C.writes == std::vector<std::vector<uint8_t>>{
    {0x12,0x34},{0x12,0xb4},{0x13,0x34}}));
  assert(M5.In_I2C.events.back() == "stop");
  M5.In_I2C = Bus{};
  assert(transfer(0x0000, data, 300, false) == 0);
  assert(M5.In_I2C.writes.size() == 6);
  assert(M5.In_I2C.writes[1].size() == 128);
  assert(M5.In_I2C.writes[5].size() == 44);
  M5.In_I2C = Bus{};
  M5.In_I2C.failWrite = true;
  assert(transfer(0, data, 300, false) == 1);
  assert(ioErrors == 1);
  assert(M5.In_I2C.events.back() == "stop");
  assert(M5.In_I2C.writes.size() == 1); // no later chunks after failure
}
