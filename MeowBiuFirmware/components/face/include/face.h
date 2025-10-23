#ifndef FACE_H
#define FACE_H

typedef enum {
    EMOTION_IDLE,
    EMOTION_EXCITED,
    EMOTION_ANGRY,
    EMOTION_BLINK,
    // Add more emotions
} emotion_t;

void face_init(void);
void face_set_emotion(emotion_t emotion);
emotion_t face_get_emotion(void);
typedef void (*face_animation_finished_cb_t)(emotion_t emotion, void *user_data);
void face_set_animation_finished_callback(face_animation_finished_cb_t callback, void *user_data);


#endif // FACE_H