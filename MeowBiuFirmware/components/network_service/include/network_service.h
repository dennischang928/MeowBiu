#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H
#include "esp_err.h"

typedef struct service_t service_t;

#define DEFAULT_SSID "Chang"
#define DEFAULT_PWD "12345678"
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
// #define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#define DEFAULT_RSSI_5G_ADJUSTMENT 0

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t network_service_init(service_t *service);
    esp_err_t network_service_start(service_t *service);
    // Test function to trigger a web request manually
    esp_err_t network_service_test_request(void);
    
    // Get and log the device MAC address
    esp_err_t network_service_get_mac(uint8_t *mac);


#ifdef __cplusplus
}
#endif
#endif
