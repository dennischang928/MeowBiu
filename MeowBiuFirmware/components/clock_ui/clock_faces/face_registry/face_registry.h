#ifndef FACE_REGISTRY_H
#define FACE_REGISTRY_H

#include <stddef.h>

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
        // why is it in c means optional?
        // because in c, we can't use pointers to functions
        // so we use a pointer to a function
        // and we can set it to NULL if we don't want to use it
        // and we can check if it's NULL before calling it
    } clock_face_t;
    
    size_t clock_face_count(void);
    const clock_face_t *clock_face_at(size_t i);
    void switch_to(size_t new_face_index);


#ifdef __cplusplus
}
#endif
#endif // FACE_REGISTRY_H