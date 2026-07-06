/**
 * @file network_service.c
 * @brief Minimal WiFi + HTTP service with response buffering
 */

#include "get_connected.h"
#include "event_bus.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <lwip/inet.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

static const char *TAG = "get_connected";
static bool wifi_connected = false;
static char http_response_buffer[2048] = {0};
static int http_response_len = 0;
char ssid[WIFI_SSID_MAX_LEN + 1] = DEFAULT_SSID;
char pwd[WIFI_PASSWD_MAX_LEN + 1] = DEFAULT_PWD;

static void post_wifi_status(wifi_connection_state_t state, const char *ip, int reason)
{
  wifi_status_event_t status = {
      .state = state,
      .reason = reason,
  };

  if (ip)
    strlcpy(status.ip, ip, sizeof(status.ip));

  event_bus_post(APP_EVENT_WIFI_STATUS, &status);
}

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

esp_err_t get_request(const char *url, void (*response_callback)(const char *response))
{
  if (!wifi_connected)
  {
    ESP_LOGW(TAG, "WiFi not connected, cannot make HTTP request");
    response_callback(NULL);
    return ESP_ERR_INVALID_STATE;
  }
  esp_http_client_config_t config = {
      .url = url,
      .event_handler = _http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK)
    response_callback(http_response_buffer);
  else
    response_callback(NULL);
  esp_http_client_cleanup(client);
  return err;
}

void cb(const char *response)
{
  ESP_LOGI(TAG, "%s", response ? response : "(null)");
}

esp_err_t test_request(void)
{
  // return get_request("http://api.open-meteo.com/v1/forecast"
  //             "?latitude=40.20522222944286&longitude=-76.7417407532657"
  //             "&current=temperature_2m,relative_humidity_2m"
  //             "&timezone=auto&forecast_days=3&wind_speed_unit=mph"
  //             "&temperature_unit=fahrenheit&precipitation_unit=inch",
  // &cb);
  return get_request("http://worldtimeapi.org/api/ip", &cb);
}

/* WiFi connection state management */
static void _wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    post_wifi_status(WIFI_STATE_CONNECTING, NULL, 0);
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    wifi_connected = false;
    wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
    int reason = disc ? disc->reason : 0;
    post_wifi_status(WIFI_STATE_DISCONNECTED, NULL, reason);
    esp_wifi_connect();
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    wifi_connected = true;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    char ip_str[40] = {0};
    esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
    post_wifi_status(WIFI_STATE_CONNECTED, ip_str, 0);
    ESP_LOGI(TAG, "Connected! IP: %s", ip_str);
  }
}

/* Periodic HTTP requests after WiFi connects */

/* Initialize NVS storage */
esp_err_t get_connected_init()
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
esp_err_t get_connected_start()
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
          .scan_method = DEFAULT_SCAN_METHOD,
          .sort_method = DEFAULT_SORT_METHOD,
          .threshold.authmode = DEFAULT_AUTHMODE,
      },
  };

  /* Copy credentials into config buffers */
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, pwd, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start()); // Start WiFi
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));

  return ESP_OK;
}

void set_ssid(const char *new_ssid)
{
  if (new_ssid)
  {
    strlcpy(ssid, new_ssid, sizeof(ssid));
  }
}

void set_pwd(const char *new_pwd)
{
  if (new_pwd)
  {
    strlcpy(pwd, new_pwd, sizeof(pwd));
  }
}

bool is_connected(void)
{
  return wifi_connected;
}

char *get_mac(void)
{
  static char mac_str[18] = {0};
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return mac_str;
}
