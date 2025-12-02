#ifndef SERVICE_BASE_H
#define SERVICE_BASE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct service_t service_t;

typedef struct {
    esp_err_t (*init)(service_t *service);
    esp_err_t (*start)(service_t *service);
} service_ops_t;

struct service_t {
    const char *name;
    const service_ops_t *ops;
};

#define DEFINE_SERVICE(service_name, ops_ptr) \
    service_t service_name = { \
        .name = #service_name, \
        .ops = ops_ptr, \
    }

#define DEFINE_SERVICE_STATIC(service_name, ops_ptr) \
    static service_t service_name = { \
        .name = #service_name, \
        .ops = ops_ptr, \
    }


#endif // SERVICE_BASE_H