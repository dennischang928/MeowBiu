/**
 * @file rtc_service.h
 * @brief Real-Time Clock Service Interface (DS3231)
 *
 * Posts APP_EVENT_TIME_UPDATE to event bus every second.
 * Subscribers receive rtc_time_t* as event_data.
 */

#ifndef RTC_SERVICE_H
#define RTC_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct service_t service_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Time Data Structure
 * ========================================================================== */

/**
 * @brief Time structure for RTC service
 */
typedef struct {
  uint16_t year;   // Full year (e.g., 2024)
  uint8_t month;   // 1-12
  uint8_t day;     // 1-31
  uint8_t weekday; // 0=Sunday, 6=Saturday
  uint8_t hour;    // 0-23
  uint8_t minute;  // 0-59
  uint8_t second;  // 0-59
} rtc_time_t;

/* ==========================================================================
 * Service Lifecycle
 * ========================================================================== */

/**
 * @brief Initialize the RTC service
 */
esp_err_t ds3231_rtc_init(service_t *svc);

/**
 * @brief Start the RTC service
 */
esp_err_t ds3231_rtc_start(service_t *svc);

/* ==========================================================================
 * Time Management API
 * ========================================================================== */

/**
 * @brief Get the current time from DS3231
 */
esp_err_t rtc_get_time(rtc_time_t *time);

/**
 * @brief Set the time on DS3231
 */
esp_err_t rtc_set_time(const rtc_time_t *time);

#ifdef __cplusplus
}
#endif

#endif // RTC_SERVICE_H
