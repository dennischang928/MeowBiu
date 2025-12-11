/**
 * @file network_service.c
 * @brief Minimal WiFi + HTTP service with response buffering
 */

#include "network_service.h"
#include "service_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

static const char *TAG = "network_service";
static bool wifi_connected = false;
static char http_response_buffer[2048] = {0};
static int http_response_len = 0;

/* Buffer HTTP response chunks and log when complete */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id)
  {
  case HTTP_EVENT_ON_DATA:
    int remaining = sizeof(http_response_buffer) - http_response_len - 1;
    if (remaining > 0)
    {
      int to_copy = (evt->data_len < remaining) ? evt->data_len : remaining;
      memcpy(http_response_buffer + http_response_len, evt->data, to_copy);
      http_response_len += to_copy;
      http_response_buffer[http_response_len] = '\0';
    }
    break;
  case HTTP_EVENT_ON_FINISH:
    if (http_response_len > 0)
    {
      ESP_LOGI(TAG, "Response: %s", http_response_buffer);
      http_response_len = 0;
    }
    break;
  default:
    break;
  }
  return ESP_OK;
}

/* Make HTTP GET request */
esp_err_t network_service_test_request(void)
{
  esp_http_client_config_t config = {
      .url = "http://api.open-meteo.com/v1/forecast"
             "?latitude=40.20522222944286&longitude=-76.7417407532657"
             "&current=temperature_2m,relative_humidity_2m"
             "&timezone=auto&forecast_days=3&wind_speed_unit=mph"
             "&temperature_unit=fahrenheit&precipitation_unit=inch",
      .event_handler = _http_event_handler,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
    ESP_LOGI(TAG, "HTTP Status: %d", esp_http_client_get_status_code(client));
  else
    ESP_LOGE(TAG, "HTTP failed: %s", esp_err_to_name(err));

  esp_http_client_cleanup(client);
  return err;
}

/* WiFi connection state management */
static void _wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    esp_wifi_connect();
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    wifi_connected = false;
    esp_wifi_connect();
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    wifi_connected = true;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

/* Periodic HTTP requests after WiFi connects */
static void _test_request_task(void *arg)
{
  while (!wifi_connected)
    vTaskDelay(pdMS_TO_TICKS(1000));

  while (1)
  {
    network_service_test_request();
    vTaskDelay(pdMS_TO_TICKS(1000*60)); //60 min
  }
}

/* Initialize NVS storage */
esp_err_t network_service_init(service_t *service)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  return ret;
}

/* Start WiFi and create HTTP task */
esp_err_t network_service_start(service_t *service)
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  /* Configure WiFi buffers for minimal RAM usage */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.static_rx_buf_num = 4;
  cfg.dynamic_rx_buf_num = 4;
  cfg.dynamic_tx_buf_num = 4;
  cfg.nvs_enable = 0;
  cfg.feature_caps = 0;

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_wifi_event_handler, NULL, NULL));

  /* Apply WiFi credentials and start connection */
  wifi_config_t wifi_config = {
      .sta = {
          .ssid = DEFAULT_SSID,
          .password = DEFAULT_PWD,
          .scan_method = DEFAULT_SCAN_METHOD,
          .sort_method = DEFAULT_SORT_METHOD,
          .threshold.authmode = DEFAULT_AUTHMODE,
      },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));

  xTaskCreate(_test_request_task, "network_test", 6144, NULL, 1, NULL);
  return ESP_OK;
}

/* Register service lifecycle */
static const service_ops_t NETWORK_OPS = {
    .init = network_service_init,
    .start = network_service_start,
};

DEFINE_SERVICE(network_service, &NETWORK_OPS);
