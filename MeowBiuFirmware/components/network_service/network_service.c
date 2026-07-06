/**
 * @file network_service.c
 * @brief Minimal WiFi + HTTP service with response buffering
 */

#include "network_service.h"
#include "service_manager.h"
#include "event_bus.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <assert.h>
#include <string.h>

#include "./get_connected/get_connected.h"

static const char *TAG = "network_service";
static bool wifi_connected = false;

static void _test_request_task(void *arg)
{
  while (!is_connected())
    vTaskDelay(pdMS_TO_TICKS(1000));

  while (1)
  {
    test_request();
    vTaskDelay(pdMS_TO_TICKS(1000 * 10)); // 10 seconds
  }
}

void weather_task_cb(const char *response)
{
    weather_data_t data = {0};

    if (response) {
        ESP_LOGI(TAG, "Weather API Response: %s", response);
        cJSON *root = cJSON_Parse(response);
        if (root) {
            cJSON *lat = cJSON_GetObjectItemCaseSensitive(root, "latitude");
            cJSON *lon = cJSON_GetObjectItemCaseSensitive(root, "longitude");
            if (cJSON_IsNumber(lat))
                data.latitude = lat->valuedouble;
            if (cJSON_IsNumber(lon))
                data.longitude = lon->valuedouble;

            cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
            if (cJSON_IsObject(current)) {
                cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
                cJSON *rh = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
                cJSON *time = cJSON_GetObjectItemCaseSensitive(current, "time");

                if (cJSON_IsNumber(temp))
                    data.temperature_f = temp->valuedouble;
                if (cJSON_IsNumber(rh))
                    data.relative_humidity = (int)rh->valuedouble;
                if (cJSON_IsString(time) && time->valuestring)
                    strlcpy(data.time_iso, time->valuestring, sizeof(data.time_iso));
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "Failed to parse weather JSON");
        }
    } else {
        ESP_LOGW(TAG, "Weather API Response is NULL");
    }
    ESP_LOGI(TAG, "Parsed Weather Data: Temp=%.2fF, Humidity=%d%%, Lat=%.4f, Lon=%.4f, Time=%s",
             data.temperature_f, data.relative_humidity, data.latitude,
             data.longitude, data.time_iso);

    event_bus_post(APP_EVENT_WEATHER_UPDATED, &data);
}

static void weather_task(void *arg)
{
    while (!is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (1) {
        get_request(
            "http://api.open-meteo.com/v1/forecast"
            "?latitude=40.19721&longitude=-76.74883"
            "&current=temperature_2m,relative_humidity_2m"
            "&timezone=auto&forecast_days=3&wind_speed_unit=mph"
            "&temperature_unit=fahrenheit&precipitation_unit=inch",
            weather_task_cb   // type: void (*)(const char *)
        );

        vTaskDelay(pdMS_TO_TICKS(10 * 1000)); // 10 seconds
    }
}

/* Initialize NVS storage */
esp_err_t network_service_init(service_t *service)
{
  esp_err_t err = get_connected_init();
  esp_err_t start_err = get_connected_start();
  return err == ESP_OK ? start_err : err;
}



/* Start WiFi and create HTTP task */
esp_err_t network_service_start(service_t *service)
{
  // xTaskCreate(&_test_request_task, "test_request_task", 4096, NULL, 5, NULL);
  xTaskCreate(&weather_task, "weather_task", 4096, NULL, 5, NULL);
  return ESP_OK;
}

/* Register service lifecycle */
static const service_ops_t NETWORK_OPS = {
    .init = network_service_init,
    .start = network_service_start,
};

DEFINE_SERVICE(network_service, &NETWORK_OPS);
