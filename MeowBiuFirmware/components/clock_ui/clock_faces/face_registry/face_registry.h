#ifndef FACE_REGISTRY_H
#define FACE_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>
#include "network_service.h"

#ifdef __cplusplus
extern "C"{
#endif


    typedef struct {
        const char *id;          // "old_fashioned", "digital", ...
        const char *name;        // shown in menu
        void (*init)(void);      // build LVGL objects (hidden)
        void (*show)(void);      // make visible, start timers
        void (*hide)(void);      // hide, stop timers
        void (*destroy)(void);   // free if needed
        
        void (*set_time)(int hour, int minute, int second);  // Called when time changes //"*" in c means optional
        void (*set_date)(int year, int month, int day, int weekoftheday);      // Called when date changes //"*" in c means optional
        bool weather_compatible; // face can display weather
        void (*set_weather)(const weather_data_t *data); // optional weather update
    } clock_face_t;
    
    size_t clock_face_count(void);
    const clock_face_t *clock_face_at(size_t i);
    void switch_to(size_t new_face_index);


#ifdef __cplusplus
}
#endif
#endif // FACE_REGISTRY_H
