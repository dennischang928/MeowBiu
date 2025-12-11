#ifndef CLOCK_UI_H
#define CLOCK_UI_H

#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif



    void clock_face_set_mode(size_t new_face_index);
    void clock_ui_init(void);
    void clock_ui_show(void);
    void clock_ui_hide(void);
    void clock_ui_set_time(int hour, int minute, int second);
    void clock_ui_set_date(int year, int month, int day, int weekoftheday);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // CLOCK_UI_H
