#include "face_old_fashioned.h"
#include "lvgl.h"
#include <math.h>
#include "esp_log.h"
#include "network_service.h"

static const char *TAG = "face_old_fashioned";

// LVGL objects for the clock UI
static lv_obj_t *clock_container = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *day_label = NULL;
static lv_obj_t *weather_label = NULL;

// Analog clock objects
static lv_obj_t *scale = NULL;
static lv_obj_t *hour_hand = NULL;
static lv_obj_t *minute_hand = NULL;
static lv_obj_t *second_hand = NULL;
static lv_obj_t *center_dot = NULL;

// Arrays for labels
static const char *days[] = {"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
                             "FRIDAY", "SATURDAY", "SUNDAY"};
// 0:Monday, 1:Tuesday, 2:Wednesday, 3:Thursday, 4:Friday, 5:Saturday, 6:Sunday
static const char *scale_labels[] = {"12", "1", "2", "3", "4", "5", "6",
                                     "7", "8", "9", "10", "11", NULL};

static void hide_old_fashioned(void)
{
    if (!clock_container) return;
    lv_lock();
    lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN); // hide the clock container
    lv_unlock();
}

static void init_old_fashioned(void)
{
    lv_lock();
    
    // Create main container
    clock_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(clock_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(clock_container);
    lv_obj_set_style_bg_color(clock_container, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_border_width(clock_container, 0, 0);
    lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_SCROLLABLE);
    
    int width = (LV_HOR_RES < LV_VER_RES ? LV_HOR_RES : LV_VER_RES);
    int scale_size = width - 0; // Add padding to prevent overflow

    // Create scale for ticks
    scale = lv_scale_create(clock_container);
    lv_obj_set_size(scale, scale_size, scale_size);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_post_draw(scale, true); // Draw on top of background
    lv_obj_center(scale);

    // Configure range and ticks (60 minutes)
    lv_scale_set_total_tick_count(scale, 61);
    lv_scale_set_major_tick_every(scale, 5);    // 5 minutes per major tick2
    lv_scale_set_label_show(scale, true);       // Show numbers
    lv_scale_set_text_src(scale, scale_labels); // Custom "12, 1, 2..." labels
    lv_scale_set_rotation(scale, 270);          // 0 at top
    lv_scale_set_angle_range(scale, 360);

    // Style the ticks and labels
    lv_obj_set_style_line_color(scale, lv_color_make(255, 255, 255),
                                LV_PART_ITEMS); // Minor ticks
    lv_obj_set_style_line_color(scale, lv_color_make(255, 255, 255),
                                LV_PART_INDICATOR);        // Major ticks
    lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);      // Minor tick length
    lv_obj_set_style_length(scale, 10, LV_PART_INDICATOR); // Major tick length
    lv_obj_set_style_line_width(scale, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale, 4, LV_PART_INDICATOR);

    // Style the numbering
    lv_obj_set_style_text_color(scale, lv_color_make(255, 255, 255),
                                LV_PART_INDICATOR);
    lv_obj_set_style_text_font(scale, &lv_font_montserrat_20, LV_PART_INDICATOR);

    // Create hour hand
    hour_hand = lv_line_create(clock_container);
    lv_obj_set_style_line_color(hour_hand, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_line_width(hour_hand, 6, 0);
    lv_obj_set_style_line_rounded(hour_hand, true, 0);
    lv_obj_set_size(hour_hand, scale_size, scale_size);
    lv_obj_center(hour_hand);

    // Create minute hand
    minute_hand = lv_line_create(clock_container);
    lv_obj_set_style_line_color(minute_hand, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_line_width(minute_hand, 4, 0);
    lv_obj_set_style_line_rounded(minute_hand, true, 0);
    lv_obj_set_size(minute_hand, scale_size, scale_size);
    lv_obj_center(minute_hand);

    // Create second hand
    second_hand = lv_line_create(clock_container);
    lv_obj_set_style_line_color(second_hand, lv_color_make(255, 200, 200), 0);
    lv_obj_set_style_line_width(second_hand, 2, 0);
    lv_obj_set_style_line_rounded(second_hand, true, 0);
    lv_obj_set_size(second_hand, scale_size, scale_size);
    lv_obj_center(second_hand);

    // Set padding of clock hands to 0 to fix alignment issue
    lv_obj_set_style_pad_all(hour_hand, 0, 0);
    lv_obj_set_style_pad_all(minute_hand, 0, 0);
    lv_obj_set_style_pad_all(second_hand, 0, 0);

    // Create center dot
    center_dot = lv_obj_create(clock_container);
    lv_obj_set_size(center_dot, 12, 12);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);

    // Create date label above center dot
    date_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(date_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(date_label, LV_OPA_80, 0);
    lv_label_set_text(date_label, "12/05");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, -30);

    // Create day of week label below center dot
    day_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(day_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(day_label, LV_OPA_60, 0);
    lv_label_set_text(day_label, "MONDAY");
    lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 35);

    // Weather label near top
    weather_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(weather_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_label, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_text_opa(weather_label, LV_OPA_80, 0);
    lv_label_set_text(weather_label, "--.-F  --%");
    lv_obj_align(weather_label, LV_ALIGN_CENTER, 0, -70);

    lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_unlock();
}

static void show_old_fashioned(void)
{
    if (!clock_container) return;
    lv_lock();
    lv_obj_remove_flag(clock_container, LV_OBJ_FLAG_HIDDEN); // show the clock container
    lv_unlock();
}

static void destroy_old_fashioned(void)
{
    // destroy LVGL objects
}

int last_minute_render = -1;
int last_hour_render = -1;

static void set_time_old_fashioned(int hour, int minute, int second)
{
    // Update analog clock hands
    int center_x = LV_HOR_RES / 2;
    int center_y = LV_VER_RES / 2;
    // Radius should match the scale size (scale_size / 2)
    // scale_size was width - 40, so radius is (width - 40) / 2
    int radius = ((LV_HOR_RES < LV_VER_RES ? LV_HOR_RES : LV_VER_RES) - 40) / 2;

    // Only update Hour and Minute hands if the minute has changed
    if (minute != last_minute_render || hour != last_hour_render)
    {
        // Calculate angles (0 degrees = 12 o'clock, clockwise)
        float hour_angle =
            ((hour % 12) * 30.0f + minute * 0.5f - 90.0f) * M_PI / 180.0f;
        float minute_angle = (minute * 6.0f - 90.0f) * M_PI / 180.0f;

        // Hour hand (50% of radius)
        static lv_point_precise_t hour_points[2];
        hour_points[0].x = center_x;
        hour_points[0].y = center_y;
        hour_points[1].x = center_x + (int)(cos(hour_angle) * radius * 0.5f);
        hour_points[1].y = center_y + (int)(sin(hour_angle) * radius * 0.5f);

        if (hour_hand)
        {
            lv_line_set_points(hour_hand, hour_points, 2);
            lv_obj_invalidate(hour_hand);
        }

        // Minute hand (75% of radius)
        static lv_point_precise_t minute_points[2];
        minute_points[0].x = center_x;
        minute_points[0].y = center_y;
        minute_points[1].x = center_x + (int)(cos(minute_angle) * radius * 0.75f);
        minute_points[1].y = center_y + (int)(sin(minute_angle) * radius * 0.75f);

        if (minute_hand)
        {
            lv_line_set_points(minute_hand, minute_points, 2);
            lv_obj_invalidate(minute_hand);
        }

        last_minute_render = minute;
        last_hour_render = hour;
    }

    // Second hand always updates
    float second_angle = (second * 6.0f - 90.0f) * M_PI / 180.0f;

    // Second hand (90% of radius)
    static lv_point_precise_t second_points[2];
    second_points[0].x = center_x;
    second_points[0].y = center_y;
    second_points[1].x = center_x + (int)(cos(second_angle) * radius * 0.9f);
    second_points[1].y = center_y + (int)(sin(second_angle) * radius * 0.9f);

    if (second_hand)
    {
        lv_line_set_points(second_hand, second_points, 2);
        lv_obj_invalidate(second_hand);
    }
}

int last_year_render = -1;
int last_month_render = -1;
int last_day_render = -1;
int last_weekday_render = -1;

static void set_date_old_fashioned(int year, int month, int day, int weekoftheday)
{
    if(last_year_render == year && last_month_render == month && last_day_render == day && last_weekday_render == weekoftheday)
        return;
    // Save state
    last_year_render = year;
    last_month_render = month;
    last_day_render = day;
    last_weekday_render = weekoftheday;

    // Skip if labels don't exist yet
    if (!date_label || !day_label)
        return;

    // Update date label - use different format based on mode
    char date_str[11];
    snprintf(date_str, sizeof(date_str), "%02d/%02d", month, day);
    lv_label_set_text(date_label, date_str);

    // Update day of the week label
    if (weekoftheday >= 0 && weekoftheday < 7)
    {
        ESP_LOGI(TAG, "Clock day of week changed to: %d, %s", weekoftheday,
                 days[weekoftheday]);
        lv_label_set_text(day_label, days[weekoftheday]);
    }
}

static void set_weather_old_fashioned(const weather_data_t *data)
{
    if (!data || !weather_label)
        return;

    char buf[24];
    snprintf(buf, sizeof(buf), "%.1fF %d%%", data->temperature_f,
             data->relative_humidity);
    lv_label_set_text(weather_label, buf);
}

const clock_face_t face_old_fashioned = {
    .id = "old_fashioned",
    .name = "Old Fashioned",
    .init = init_old_fashioned,
    .show = show_old_fashioned,
    .hide = hide_old_fashioned,
    .destroy = destroy_old_fashioned,
    .set_time = set_time_old_fashioned,
    .set_date = set_date_old_fashioned,
    .weather_compatible = true,
    .set_weather = set_weather_old_fashioned,
};
