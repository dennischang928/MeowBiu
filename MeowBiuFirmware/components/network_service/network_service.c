#include "network_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "service_manager.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "network_service";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
             evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    if (!esp_http_client_is_chunked_response(evt->client)) {
      // Write out data
      printf("%.*s", evt->data_len, (char *)evt->data);
    }
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    break;
  }
  return ESP_OK;
}

esp_err_t network_service_test_request(void) {
  esp_http_client_config_t config = {
      .url = "http://httpbin.org/get",
      .event_handler = _http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  } else {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
  return err;
}

esp_err_t network_service_get_mac(uint8_t *mac) {
  esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Device MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5]);
  } else {
    ESP_LOGE(TAG, "Failed to read MAC address");
  }
  return ret;
}

esp_err_t network_service_init(service_t *service) {
  ESP_LOGI(TAG, "Initializing network service...");
  // Future: Initialize WiFi or check connection status here
  ESP_LOGI(TAG, "Network service initialized.");
  return ESP_OK;
}

esp_err_t network_service_start(service_t *service) {
  ESP_LOGI(TAG, "Starting network service...");

  uint8_t mac[6];
  network_service_get_mac(mac);

  // For demonstration, let's fire a request on start (or maybe not, let's keep
  // it manual first as per plan to minimal structure)
  // network_service_test_request();
  ESP_LOGI(TAG, "Network service started.");
  return ESP_OK;
}

static const service_ops_t NETWORK_OPS = {
    .init = network_service_init,
    .start = network_service_start,
};

DEFINE_SERVICE(network_service, &NETWORK_OPS);
