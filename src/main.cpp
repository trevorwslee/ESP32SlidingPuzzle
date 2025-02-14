#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // the following included _secret.h file should contain the following lines:
  // #define WIFI_SSID           "wifi ssid"
  // #define WIFI_PASSWORD       "wifi password"
  #include "_secret.h"
#else
  #define BLUETOOTH "ESP32Sliding"
#endif

#include "esp32_sliding_puzzle/esp32_sliding_puzzle.ino"
