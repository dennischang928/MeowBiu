#ifndef FACE_H
#define FACE_H

#include <stdint.h>

typedef enum {
    EMOTION_IDLE,
    EMOTION_EXCITED,
    EMOTION_ANGRY,
    EMOTION_BLINK,
    EMOTION_IDLE_LOOK_RIGHT,
    EMOTION_IDLE_LOOK_LEFT,
    EMOTION_SAD,
    EMOTION_SCARED,
    EMOTION_DGAF,
    // Add more emotions
} emotion_t;

void face_init(void);

void face_show(void);
void face_hide(void);


void face_set_emotion(emotion_t emotion);
void face_get_loop_count(emotion_t emtion);
emotion_t face_get_emotion(void);
typedef void (*face_animation_finished_cb_t)(emotion_t emotion, void *user_data);
void face_set_animation_finished_callback(face_animation_finished_cb_t callback, void *user_data);
void face_set_loop_count(emotion_t emotion, uint8_t count);


#endif // FACE_H