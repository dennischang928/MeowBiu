#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "esp_err.h"
#include "esp_event.h"

// System-wide events
ESP_EVENT_DECLARE_BASE(APP_EVENTS);

typedef enum {
  APP_EVENT_UI_IDLE,
  APP_EVENT_SINGLE_TAP_DETECTED,
  APP_EVENT_DOUBLE_TAP_DETECTED,
  APP_EVENT_TRIPLE_TAP_DETECTED,
  APP_EVENT_SPAM_TAP_DETECTED, // Posted when user taps too rapidly
  APP_EVENT_TIME_UPDATE, // Posted by RTC service every second with rtc_time_t*
                         // data
} event_id_t;

typedef void (*event_callback_t)(event_id_t event_id, void *event_data,
                                 void *user_data);

esp_err_t event_bus_init(void);
esp_err_t event_bus_post(event_id_t event_id, void *event_data);
esp_err_t event_bus_subscribe(event_id_t event_id, event_callback_t callback,
                              void *user_data);

#endif // EVENT_BUS_H