// 钓鱼海况屏 — fishing conditions display for M5Stack Tab5 (PRD v3).
#include <M5Unified.h>
#include "app.h"

AppState app;

SET_LOOP_TASK_STACK_SIZE(64 * 1024);   // report building + regex-based fact extraction need more than the default 8 KB

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.setRxBufferSize(1024);
  Serial.begin(115200);
  app.mu = xSemaphoreCreateMutex();
  Serial.printf("\nfish-display %s, psram %u\n", FW_VERSION, (unsigned)ESP.getPsramSize());
  storeBegin();
  clockBegin();
  { Lock l; storeLoad(app.inputs); }
  uiBegin();
  netBegin();
}

void loop() {
  M5.update();
  serialLoop();
  uiLoop();
  delay(10);
}
