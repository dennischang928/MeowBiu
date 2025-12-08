#ifndef CLOCK_UI_H
#define CLOCK_UI_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        CLOCK_FACE_TEXT,
        CLOCK_FACE_OLD_FASHION,
    } clock_face_t;

    void clock_face_set_mode(clock_face_t mode);
    void clock_face_toggle_mode(void);
    void clock_ui_init(void);
    void clock_ui_layout_init(void);
    void clock_ui_show(void);
    void clock_ui_hide(void);
    void clock_ui_set_time(int hour, int minute, int second);
    void clock_ui_set_date(int year, int month, int day, int weekoftheday);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // CLOCK_UI_H
