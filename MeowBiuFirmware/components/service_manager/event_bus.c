// #include "event_bus.h"
// #include "esp_log.h"

// static const char *TAG = "event_bus";

// static esp_event_loop_handle_t event_loop = NULL;

// esp_err_t event_bus_init(void)
// {
//     if (event_loop != NULL) {
//         ESP_LOGW(TAG, "Event bus already initialized");
//         return ESP_OK;
//     }
    
//     esp_event_loop_args_t loop_args = {
//         .queue_size = 32,
//         .task_name = "event_bus",
//         .task_priority = 10,
//         .task_stack_size = 4096,
//         .task_core_id = tskNO_AFFINITY
//     };
    
//     esp_err_t ret = esp_event_loop_create(&loop_args, &event_loop);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to create event loop");
//         return ret;
//     }
    
//     ESP_LOGI(TAG, "Event bus initialized");
//     return ESP_OK;
// }

// esp_err_t event_bus_deinit(void)
// {
//     if (event_loop) {
//         esp_event_loop_delete(event_loop);
//         event_loop = NULL;
//     }
//     return ESP_OK;
// }

// esp_err_t event_bus_post(app_event_id_t event_id, void *event_data, size_t data_size)
// {
//     if (!event_loop) {
//         ESP_LOGE(TAG, "Event bus not initialized");
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     return esp_event_post_to(event_loop, APP_EVENTS, event_id, event_data, data_size, portMAX_DELAY);
// }

// esp_err_t event_bus_subscribe(app_event_id_t event_id, esp_event_handler_t handler, void *handler_arg)
// {
//     if (!event_loop) {
//         ESP_LOGE(TAG, "Event bus not initialized");
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     return esp_event_handler_register_with(event_loop, APP_EVENTS, event_id, handler, handler_arg);
// }

// esp_err_t event_bus_unsubscribe(app_event_id_t event_id, esp_event_handler_t handler)
// {
//     if (!event_loop) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     return esp_event_handler_unregister_with(event_loop, APP_EVENTS, event_id, handler);
// }


#include "event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "event_bus";

#define MAX_SUBSCRIBERS 10

// A subscriber
typedef struct {
    event_id_t event_id;
    event_callback_t callback;
    void *user_data;
    bool in_use;
} subscriber_t;

static struct {
    subscriber_t subscribers[MAX_SUBSCRIBERS];
    bool initialized;
} event_bus;

esp_err_t event_bus_init(void)
{
    if (event_bus.initialized) {
        ESP_LOGW(TAG, "Event bus already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing event bus");
    memset(&event_bus, 0, sizeof(event_bus));
    event_bus.initialized = true;
    
    return ESP_OK;
}

esp_err_t event_bus_subscribe(event_id_t event_id, event_callback_t callback, void *user_data)
{
    if (!event_bus.initialized) {
        ESP_LOGE(TAG, "Event bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!event_bus.subscribers[i].in_use) {
            event_bus.subscribers[i].event_id = event_id;
            event_bus.subscribers[i].callback = callback;
            event_bus.subscribers[i].user_data = user_data;
            event_bus.subscribers[i].in_use = true;
            
            ESP_LOGI(TAG, "Subscribed to event %d", event_id);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "No subscriber slots available");
    return ESP_ERR_NO_MEM;
}

esp_err_t event_bus_post(event_id_t event_id, void *event_data)
{
    if (!event_bus.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Posting event %d", event_id);
    
    // Call all subscribers listening to this event
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (event_bus.subscribers[i].in_use && 
            event_bus.subscribers[i].event_id == event_id) {
            
            ESP_LOGD(TAG, "Notifying subscriber %d", i);
            event_bus.subscribers[i].callback(event_id, event_data, 
                                              event_bus.subscribers[i].user_data);
        }
    }
    
    return ESP_OK;
}


