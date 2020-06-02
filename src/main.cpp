#include <Arduino.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_bt.h>
#include <EspWifiSetup.h>
#include "version.h"

// Configuration
const auto MAIN_LOOP_INTERVAL = 1000;

void setup()
{
  // Disable BT
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_bt_mem_release(ESP_BT_MODE_BTDM));

  // WiFi
  WiFiSetup(WIFI_SETUP_WPS);

  // Done
  pinMode(0, OUTPUT);
  log_i("started %s", VERSION);
}

void loop()
{
  // Toggle Status LED
  static auto status = false;
  digitalWrite(0, (status = !status) ? HIGH : LOW);

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL);
}
