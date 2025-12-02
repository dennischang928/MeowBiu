#include "service_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "service_manager";

#define MAX_SERVICES 5

static struct {
    service_t *services[MAX_SERVICES];
    int count;
    bool initialized;
} manager;

esp_err_t service_manager_init(void)
{
    if (manager.initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing service manager");
    memset(&manager, 0, sizeof(manager));
    manager.initialized = true;
    
    return ESP_OK;
}

esp_err_t service_manager_register(service_t *service)
{
    if (!manager.initialized) {
        ESP_LOGE(TAG, "Manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (manager.count >= MAX_SERVICES) {
        ESP_LOGE(TAG, "Too many services");
        return ESP_ERR_NO_MEM;
    }
    
    manager.services[manager.count] = service;
    manager.count++;
    
    ESP_LOGI(TAG, "Registered service: %s", service->name);
    return ESP_OK;
}



esp_err_t service_manager_start_all(void)
{
    ESP_LOGI(TAG, "Starting %d services", manager.count);

    // Initialize all services (basically running the init() function defined in each service ops(ops = operations))
    for (int i = 0; i < manager.count; i++) {
        service_t *svc = manager.services[i];
        
        if (svc->ops->init) {
            ESP_LOGI(TAG, "Initializing: %s", svc->name);
            esp_err_t ret = svc->ops->init(svc);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to init %s", svc->name);
                return ret;
            }
        }
    }
    
    // Start all services (basically running the start() function defined in each service ops(ops = operations))
    for (int i = 0; i < manager.count; i++) {
        service_t *svc = manager.services[i];
        
        if (svc->ops->start) {
            ESP_LOGI(TAG, "Starting: %s", svc->name);
            esp_err_t ret = svc->ops->start(svc);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start %s", svc->name);
                return ret;
            }
        }
    }
    
    ESP_LOGI(TAG, "All services running!");
    return ESP_OK;
}