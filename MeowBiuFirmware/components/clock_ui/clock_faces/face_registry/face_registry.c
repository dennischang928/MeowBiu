#include "face_registry.h"

extern const clock_face_t face_old_fashioned;
extern const clock_face_t face_digital;
extern const clock_face_t face_simple_weather;

static const clock_face_t *faces[] = {
    &face_old_fashioned,
    &face_digital,
    &face_simple_weather,
};

size_t clock_face_count(void) { return sizeof(faces) / sizeof(faces[0]); }
const clock_face_t *clock_face_at(size_t i) { return faces[i]; } //syntax anlaysis: return type is const clock_face_t * (pointer to const clock_face_t)
// and the function name is clock_face_at
// and the parameter is size_t i
// and the parameter is the index of the face
// usage: const clock_face_t *face = clock_face_at(i);
static const clock_face_t *current;

void switch_to(size_t new_face_index)
{
    if (current && current->hide)
        current->hide();
    current = clock_face_at(new_face_index);
    if (current && current->show)
        current->show();
}
