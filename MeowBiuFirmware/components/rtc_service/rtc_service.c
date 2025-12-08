/**
 * @file rtc_service.c
 * @brief Real-Time Clock Service Implementation using DS3231 RTC Module.
 *
 * This service manages the DS3231 RTC via I2C. It provides accurate timekeeping
 * and periodically broadcasts time updates to the system via the event bus.
 */

#include "rtc_service.h"
#include "event_bus.h"
#include "service_manager.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include <string.h>

static const char *TAG = "rtc_service";

/* ==========================================================================
 * Configuration & Constants
 * ========================================================================== */

#define DS3231_ADDR 0x68 // DS3231 I2C address

// DS3231 Register addresses
#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_MINUTES 0x01
#define DS3231_REG_HOURS 0x02
#define DS3231_REG_DOW 0x03
#define DS3231_REG_DATE 0x04
#define DS3231_REG_MONTH 0x05
#define DS3231_REG_YEAR 0x06

// I2C configuration - adjust these pins for your board
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 6 // GPIO for SDA
#define I2C_MASTER_SCL_IO 7 // GPIO for SCL
#define I2C_MASTER_FREQ_HZ 100000

/* ==========================================================================
 * Private State
 * ========================================================================== */

typedef struct {
  esp_timer_handle_t tick_timer; ///< Timer for periodic time updates
  rtc_time_t current_time;       ///< Cached current time
  bool i2c_initialized;          ///< Flag indicating if I2C is ready
} rtc_service_state_t;

static rtc_service_state_t priv = {
    .tick_timer = NULL,
    .current_time = {0},
    .i2c_initialized = false,
};

/* ==========================================================================
 * Private Helper Functions (BCD Conversion)
 * ========================================================================== */

/**
 * @brief Convert Binary Coded Decimal (BCD) to Decimal.
 */
static uint8_t bcd_to_dec(uint8_t bcd) {
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/**
 * @brief Convert Decimal to Binary Coded Decimal (BCD).
 */
static uint8_t dec_to_bcd(uint8_t dec) {
  return ((dec / 10) << 4) | (dec % 10);
}

/* ==========================================================================
 * Private Helper Functions (I2C Communication)
 * ========================================================================== */

/**
 * @brief Initialize the I2C master interface.
 */
static esp_err_t i2c_master_init(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
  if (err != ESP_OK) {
    return err;
  }

  return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Read data from a DS3231 register.
 */
static esp_err_t ds3231_read_reg(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_ADDR, &reg, 1,
                                      data, len, pdMS_TO_TICKS(100));
}

/**
 * @brief Write data to a DS3231 register.
 */
static esp_err_t ds3231_write_reg(uint8_t reg, uint8_t data) {
  uint8_t buf[2] = {reg, data};
  return i2c_master_write_to_device(I2C_MASTER_NUM, DS3231_ADDR, buf, 2,
                                    pdMS_TO_TICKS(100));
}

/* ==========================================================================
 * DS3231 Driver Functions
 * ========================================================================== */

/**
 * @brief Read the current time from the DS3231.
 */
static esp_err_t ds3231_get_time(rtc_time_t *time) {
  uint8_t data[7];

  esp_err_t err = ds3231_read_reg(DS3231_REG_SECONDS, data, 7);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read DS3231: %s", esp_err_to_name(err));
    return err;
  }

  time->second = bcd_to_dec(data[0] & 0x7F);
  time->minute = bcd_to_dec(data[1] & 0x7F);
  time->hour = bcd_to_dec(data[2] & 0x3F); // 24-hour mode
  time->weekday = (data[3] & 0x07) - 1;    // from 1-7 to 0-6, Monday=0
  time->day = bcd_to_dec(data[4] & 0x3F);
  time->month = bcd_to_dec(data[5] & 0x1F);
  time->year = 2000 + bcd_to_dec(data[6]);

  return ESP_OK;
}

/**
 * @brief Set the time on the DS3231.
 */
static esp_err_t ds3231_set_time(const rtc_time_t *time) {
  esp_err_t err;

  // Calculate day of week using Zeller's congruence
  int y = time->year, m = time->month, d = time->day;
  if (m < 3) {
    m += 12;
    y--;
  }
  int dow =
      ((d + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7) +
      1; // 1-7, 1 = Monday

  ESP_LOGI(TAG, "Setting time to %04d-%02d-%02d %02d:%02d:%02d (DOW=%d)",
           time->year, time->month, time->day, time->hour, time->minute,
           time->second, dow);

  err = ds3231_write_reg(DS3231_REG_SECONDS, dec_to_bcd(time->second));
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_MINUTES, dec_to_bcd(time->minute));
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_HOURS, dec_to_bcd(time->hour));
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_DOW, dow);
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_DATE, dec_to_bcd(time->day));
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_MONTH, dec_to_bcd(time->month));
  if (err != ESP_OK)
    return err;

  err = ds3231_write_reg(DS3231_REG_YEAR, dec_to_bcd(time->year - 2000));
  if (err != ESP_OK)
    return err;

  return ESP_OK;
}

/* ==========================================================================
 * Service Task (Timer Callback)
 * ========================================================================== */

/**
 * @brief Periodic timer callback to read time and post updates.
 */
static void tick_timer_callback(void *arg) {
  // Read time from DS3231
  if (ds3231_get_time(&priv.current_time) != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read time from DS3231");
    return;
  }
  ESP_LOGI(TAG, "Time read from DS3231: %04d-%02d-%02d %02d:%02d:%02d (DOW=%d)",
           priv.current_time.year, priv.current_time.month,
           priv.current_time.day, priv.current_time.hour,
           priv.current_time.minute, priv.current_time.second,
           priv.current_time.weekday);

  // Post time update event to event bus
  event_bus_post(APP_EVENT_TIME_UPDATE, &priv.current_time);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

esp_err_t rtc_get_time(rtc_time_t *time) {
  if (!time) {
    return ESP_ERR_INVALID_ARG;
  }

  // Read fresh time from DS3231
  esp_err_t err = ds3231_get_time(time);
  if (err == ESP_OK) {
    memcpy(&priv.current_time, time, sizeof(rtc_time_t));
  }
  return err;
}

esp_err_t rtc_set_time(const rtc_time_t *time) {
  if (!time) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ds3231_set_time(time);
  if (err == ESP_OK) {
    memcpy(&priv.current_time, time, sizeof(rtc_time_t));
  }
  return err;
}

/* ==========================================================================
 * Service Lifecycle
 * ========================================================================== */

esp_err_t ds3231_rtc_init(service_t *svc) {
  ESP_LOGI(TAG, "Initializing RTC service (DS3231)");

  // Initialize I2C
  esp_err_t err = i2c_master_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(err));
    return err;
  }
  priv.i2c_initialized = true;

  // Read initial time from DS3231
  err = ds3231_get_time(&priv.current_time);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read initial time from DS3231");
    // Continue anyway - time may be set later
  } else {
    ESP_LOGI(TAG, "DS3231 time: %04d-%02d-%02d %02d:%02d:%02d",
             priv.current_time.year, priv.current_time.month,
             priv.current_time.day, priv.current_time.hour,
             priv.current_time.minute, priv.current_time.second);
  }

  // Create 1-second tick timer
  const esp_timer_create_args_t timer_args = {
      .callback = tick_timer_callback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "rtc_tick",
  };

  err = esp_timer_create(&timer_args, &priv.tick_timer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create tick timer: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "RTC service initialized");
  return ESP_OK;
}

esp_err_t ds3231_rtc_start(service_t *svc) {
  ESP_LOGI(TAG, "Starting RTC service");

  // Start 1-second timer
  esp_err_t err =
      esp_timer_start_periodic(priv.tick_timer, 1000000); // 1 second
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start tick timer: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "RTC service started");
  return ESP_OK;
}

/* ==========================================================================
 * Service Registration
 * ========================================================================== */

static const service_ops_t RTC_OPS = {
    .init = ds3231_rtc_init,
    .start = ds3231_rtc_start,
};

DEFINE_SERVICE(rtc_service, &RTC_OPS);
