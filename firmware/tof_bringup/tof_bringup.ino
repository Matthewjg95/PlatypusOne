// Standalone bench sketch. See docs/hardware/TOF_BRINGUP.md before powering.
#include <M5Unified.h>
#include "src/uld/vl53l8cx_api.h"

static VL53L8CX_Configuration sensor;
static VL53L8CX_ResultsData results;
static constexpr uint8_t address = 0x29;
static constexpr uint32_t busHz = 400000;
static uint32_t ioErrors = 0, frames = 0, lastFrame = 0;
static bool running = false;

// ULD uses 16-bit big-endian register addresses, unlike M5's 8-bit helpers.
// All transactions use the already-initialized internal bus, on the loop task.
static uint8_t transfer(uint16_t reg, uint8_t* data, uint32_t size, bool read) {
  while (size) {
    const uint32_t n = size > 128 ? 128 : size;
    uint8_t header[] = {uint8_t(reg >> 8), uint8_t(reg)};
    bool ok = M5.In_I2C.start(address, false, busHz);
    if (ok) ok = M5.In_I2C.write(header, 2);
    if (ok && read) ok = M5.In_I2C.restart(address, true, busHz);
    if (ok) ok = read ? M5.In_I2C.read(data, n, true) : M5.In_I2C.write(data, n);
    const bool stopped = M5.In_I2C.stop(); // release even on NACK
    if (!ok || !stopped) { ++ioErrors; return 1; }
    reg += n; data += n; size -= n;
  }
  return 0;
}
static uint8_t writeSensor(void*, uint16_t r, uint8_t* d, uint32_t n) { return transfer(r,d,n,false); }
static uint8_t readSensor(void*, uint16_t r, uint8_t* d, uint32_t n) { return transfer(r,d,n,true); }
static uint8_t waitSensor(void*, uint32_t ms) { delay(ms); return 0; }
static bool checked(const char* stage, uint8_t status) {
  Serial.printf("event,%lu,%s,%u,io_errors,%lu\n", (unsigned long)millis(), stage, status, (unsigned long)ioErrors);
  if (status) { running = false; Serial.println("HALT,power_down_and_investigate"); }
  return status == 0;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  delay(2000);
  Serial.println("tof_bringup_v1,4x4,10Hz,400000Hz,strict_status_5");
  if (M5.In_I2C.getSDA() != 31 || M5.In_I2C.getSCL() != 32) {
    Serial.println("HALT,unexpected_internal_bus"); return;
  }
  M5.Display.println("ToF bench: send b for baseline; r after rail checks");
  Serial.println("READY,b=baseline_scan,r=range_after_human_rail_checks");
}

void loop() {
  M5.update();
  if (!running && Serial.available()) {
    const char command = Serial.read();
    if (command == 'b' || command == 'r') {
      for (uint8_t a = 8; a < 120; ++a)
        if (M5.In_I2C.scanID(a, 100000)) Serial.printf("ack,0x%02x\n", a);
    }
    if (command == 'r') {
      if (!M5.In_I2C.scanID(address, 100000)) { Serial.println("HALT,no_0x29"); return; }
      sensor.platform.address = 0x52; // ST convention; callbacks use 7-bit 0x29
      sensor.platform.Write = writeSensor;
      sensor.platform.Read = readSensor;
      sensor.platform.Wait = waitSensor;
      Serial.println("INIT_BEGIN,firmware_upload_may_take_seconds");
      if (!checked("init", vl53l8cx_init(&sensor))) return;
      if (!checked("resolution", vl53l8cx_set_resolution(&sensor, VL53L8CX_RESOLUTION_4X4))) return;
      if (!checked("frequency", vl53l8cx_set_ranging_frequency_hz(&sensor, 10))) return;
      if (!checked("start", vl53l8cx_start_ranging(&sensor))) return;
      Serial.println("zone,frame,ms,read_ms,index,targets,status,mm,sigma_mm,signal_kcps_spad,ambient_kcps_spad,valid,io_errors");
      running = true; lastFrame = millis();
    }
  }
  if (!running) { delay(5); return; }
  uint8_t ready = 0;
  const uint8_t rc = vl53l8cx_check_data_ready(&sensor, &ready);
  if (rc) { checked("ready", rc); return; }
  if (ready) {
    const uint32_t start = millis();
    if (!checked("read", vl53l8cx_get_ranging_data(&sensor, &results))) return;
    const uint32_t now = millis();
    ++frames; lastFrame = now;
    for (unsigned z = 0; z < 16; ++z) {
      const unsigned i = z * VL53L8CX_NB_TARGET_PER_ZONE;
      const bool valid = results.nb_target_detected[z] > 0 && results.target_status[i] == 5;
      Serial.printf("zone,%lu,%lu,%lu,%u,%u,%u,%d,%u,%lu,%lu,%u,%lu\n",
        (unsigned long)frames, (unsigned long)now, (unsigned long)(now-start), z, results.nb_target_detected[z], results.target_status[i],
        results.distance_mm[i], results.range_sigma_mm[i],
        (unsigned long)results.signal_per_spad[i], (unsigned long)results.ambient_per_spad[z], valid, (unsigned long)ioErrors);
    }
  }
  if (millis() - lastFrame > 2000) {
    Serial.printf("TIMEOUT,%lu,frames,%lu,io_errors,%lu\n", (unsigned long)millis(), (unsigned long)frames, (unsigned long)ioErrors);
    running = false; // preserve failure; do not silently reset and hide it
  }
  delay(5);
}
