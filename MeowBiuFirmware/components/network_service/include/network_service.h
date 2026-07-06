#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H
#include "esp_err.h"
#include <stdint.h>

typedef struct service_t service_t;

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float latitude;
        float longitude;
        float temperature_f;
        int relative_humidity;
        char time_iso[32];
    } weather_data_t;

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
