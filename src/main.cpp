#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <double_reset.h>
#include <wps_config.h>
#include <wifi_reconnect.h>
#include "sdkconfig.h"
#include "version.h"

// Configuration
static const char TAG[] = "main";

const uint32_t MAIN_LOOP_INTERVAL = 1000;

const gpio_num_t STATUS_LED_GPIO = (gpio_num_t)CONFIG_STATUS_LED_GPIO;
const uint8_t STATUS_LED_ON = CONFIG_STATUS_LED_ON;
const uint8_t STATUS_LED_OFF = (~CONFIG_STATUS_LED_ON & 1);

void setup()
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Check double reset
  // NOTE this should be called as soon as possible, ideally right after nvs init
  bool reconfigure = false;
  ESP_ERROR_CHECK(double_reset_start(&reconfigure, 5000));

  // Status LED
  ESP_ERROR_CHECK(gpio_reset_pin(STATUS_LED_GPIO));
  ESP_ERROR_CHECK(gpio_set_direction(STATUS_LED_GPIO, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, STATUS_LED_ON));

  // Initalize WiFi
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Reconnection watch
  ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

  // Start WPS if WiFi is not configured, or reconfiguration was requested
  if (!wifi_reconnect_is_ssid_stored() || reconfigure)
  {
    ESP_LOGI(TAG, "reconfigure request detected, starting WPS");
    ESP_ERROR_CHECK(wps_config_start());
  }
  else
  {
    // Connect now
    ESP_ERROR_CHECK(esp_wifi_connect());
  }

  // Wait for WiFi
  ESP_LOGI(TAG, "waiting for wifi");
  if (!wifi_reconnect_wait_for_connection(AUTO_WPS_TIMEOUT_MS + WIFI_RECONNECT_CONNECT_TIMEOUT_MS))
  {
    ESP_LOGE(TAG, "failed to connect to wifi!");
    // NOTE either fallback into emergency operation mode, do nothing, restart..
  }

  // Setup complete
  ESP_LOGI(TAG, "started %s", VERSION);
}

void loop()
{
  // Toggle Status LED
  static auto status = false;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(STATUS_LED_GPIO, (status = !status) ? STATUS_LED_ON : STATUS_LED_OFF));

  // Wait
  static auto previousWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&previousWakeTime, MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
}

extern "C" _Noreturn void app_main()
{
  setup();
  for (;;)
  {
    loop();
  }
}
