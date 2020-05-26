#include <Arduino.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <EspWifiSetup.h>
#include "version.h"

// Configuration
const auto MAIN_LOOP_INTERVAL = 1000;

void setup()
{
  esp_err_t err;

  // Initialize NVS
  err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

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
