#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <double_reset.h>
#include <wps_config.h>
#include <wifi_reconnect.h>
#include <status_led.h>
#include "sdkconfig.h"
#include "version.h"

// Configuration
static const char TAG[] = "main";
static const char HOSTNAME[] = "esp-app-template"; // You want unique name per device, probably dynamic in configuration

const uint32_t MAIN_LOOP_INTERVAL = 1000;

const gpio_num_t STATUS_LED_GPIO = (gpio_num_t)CONFIG_STATUS_LED_GPIO;
const uint32_t STATUS_LED_ON = CONFIG_STATUS_LED_ON;

static status_led_handle_t status_led = NULL;

void setup()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Check double reset
  // NOTE this should be called as soon as possible, ideally right after nvs init
  bool reconfigure = false;
  ESP_ERROR_CHECK(double_reset_start(&reconfigure, DOUBLE_RESET_DEFAULT_TIMEOUT));

  // Status LED
  ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_create(STATUS_LED_GPIO, STATUS_LED_ON, &status_led));
  ESP_ERROR_CHECK_WITHOUT_ABORT(status_led_set_interval(status_led, 500, true));

  // Events
  esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval(status_led, 500, true); }, NULL);
  esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_WPS_ER_SUCCESS, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval(status_led, 500, true); }, NULL);
  esp_event_handler_register(
      WPS_CONFIG, WPS_CONFIG_EVENT_START, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval(status_led, 100, true); }, NULL);
  esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, [](void *, esp_event_base_t, int32_t, void *) { status_led_set_interval_for(status_led, 200, false, 700, true); }, NULL);

  // Initalize WiFi
  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, HOSTNAME));

  // Reconnection watch
  ESP_ERROR_CHECK(wifi_reconnect_start()); // NOTE this must be called before connect, otherwise it might miss connected event

  // Start WPS if WiFi is not configured, or reconfiguration was requested
  if (!wifi_reconnect_is_ssid_stored() || reconfigure)
  {
    ESP_LOGI(TAG, "reconfigure request detected, starting WPS");
    ESP_ERROR_CHECK(wps_config_start());

    // Wait for WPS to finish
    wifi_reconnect_wait_for_connection(WPS_CONFIG_TIMEOUT_MS);
  }
  else
  {
    // Connect now
    wifi_reconnect_resume();
  }

  // Wait for WiFi
  ESP_LOGI(TAG, "waiting for wifi");
  if (!wifi_reconnect_wait_for_connection(WIFI_RECONNECT_CONNECT_TIMEOUT_MS))
  {
    ESP_LOGE(TAG, "failed to connect to wifi!");
    // NOTE either fallback into emergency operation mode, do nothing, restart..
  }

  // Setup complete
  ESP_LOGI(TAG, "started %s", VERSION);
}

void loop()
{
  // Wait
  static auto previous_wake_time = xTaskGetTickCount();
  vTaskDelayUntil(&previous_wake_time, MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
}

extern "C" _Noreturn void app_main()
{
  setup();
  for (;;)
  {
    loop();
  }
}
