#ifndef UI_SERVICE_H
#define UI_SERVICE_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// #include "service_base.h"

#include "face.h"
typedef struct service_t service_t;

#ifdef __cplusplus
extern "C"
{
#endif
    static const emotion_t MOOD_HAPPY_EMOTIONS[] = {
        EMOTION_EXCITED,
    };

    static const emotion_t MOOD_FURIOUS_EMOTIONS[] = {
        EMOTION_ANGRY,
        EMOTION_DGAF,
    };

    static const emotion_t MOOD_DEPRESSED_EMOTIONS[] = {
        EMOTION_SAD,
        EMOTION_SCARED,
    };

    static const emotion_t MOOD_NORMAL_EMOTIONS[] = {
        EMOTION_IDLE,
        EMOTION_IDLE_LOOK_RIGHT,
        EMOTION_IDLE_LOOK_LEFT,
    };

    

    typedef enum
    {
        UI_MODE_FACE,
        UI_MODE_CLOCK_UI,
    } ui_mode_t;

    typedef struct
    {
        uint8_t hour;    // 0-23
        uint8_t minute;  // 0-59
        uint8_t second;  // 0-59
        uint8_t day;     // 1-31
        uint8_t month;   // 1-12
        uint16_t year;   // 2024, 2025, etc.
        uint8_t weekday; // 0=Sunday, 6=Saturday
    } ui_time_t;

    esp_err_t ui_init(service_t *svc);
    esp_err_t ui_start(service_t *svc);
    void ui_set_mode(ui_mode_t mode);
    ui_mode_t ui_get_mode();
    void ui_update_time(const ui_time_t *time);
    /**
     * @brief Trigger emotion on face
     *
     * @param emotion Emotion ID from face.h (EMOTION_EXCITED, EMOTION_ANGRY, etc.)
     *
     * Example:
     *   #include "face.h"
     *   ui_service_trigger_emotion(EMOTION_EXCITED);
     */
    // void ui_service_trigger_emotion(uint32_t emotion);

#ifdef __cplusplus
}
#endif
#endif
