#ifndef INPUT_SERVICE_H
#define INPUT_SERVICE_H
#include "esp_err.h"

typedef struct service_t service_t;

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t input_service_init(service_t *service);
    esp_err_t input_service_deinit(service_t *service);

#ifdef __cplusplus
}
#endif
#endif