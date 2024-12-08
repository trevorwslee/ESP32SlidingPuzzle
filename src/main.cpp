#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #include "_secret.h"
#else
  #define BLUETOOTH "ESP32Sliding"
#endif

#include "esp32_sliding_puzzle/esp32_sliding_puzzle.ino"
