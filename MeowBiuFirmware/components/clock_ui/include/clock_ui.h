#ifndef CLOCK_UI_H
#define CLOCK_UI_H

#ifdef __cplusplus
extern "C" {
#endif
    void clock_ui_init(void);
    void clock_ui_layout_init(void);
    void clock_ui_show(void);
    void clock_ui_hide(void);
    void clock_ui_set_time(int year, int month, int day, int weekoftheday);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif // CLOCK_UI_H
