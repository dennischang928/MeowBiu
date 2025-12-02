#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include "service_base.h"

esp_err_t service_manager_init(void);
esp_err_t service_manager_register(service_t *service);
esp_err_t service_manager_start_all(void);

#endif // SERVICE_MANAGER_H