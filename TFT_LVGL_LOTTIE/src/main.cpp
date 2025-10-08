#include <lvgl.h>
#include <TFT_eSPI.h>

#define TFT_HOR_RES 240
#define TFT_VER_RES 240
#define TFT_ROTATION LV_DISPLAY_ROTATION_0

#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

static uint32_t my_tick(void) { return millis(); }

// Eye components
lv_obj_t *left_eye_white;
lv_obj_t *left_eye_pupil;
lv_obj_t *right_eye_white;
lv_obj_t *right_eye_pupil;

void create_eyes(lv_obj_t *parent) {
    // Left eye (white part)
    left_eye_white = lv_obj_create(parent);
    lv_obj_set_size(left_eye_white, 60, 80);
    lv_obj_set_pos(left_eye_white, 60, 80);
    lv_obj_set_style_bg_color(left_eye_white, lv_color_white(), 0);
    lv_obj_set_style_radius(left_eye_white, 30, 0);
    lv_obj_set_style_border_width(left_eye_white, 2, 0);
    
    // Left pupil
    left_eye_pupil = lv_obj_create(left_eye_white);
    lv_obj_set_size(left_eye_pupil, 20, 20);
    lv_obj_center(left_eye_pupil);
    lv_obj_set_style_bg_color(left_eye_pupil, lv_color_black(), 0);
    lv_obj_set_style_radius(left_eye_pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(left_eye_pupil, LV_OBJ_FLAG_SCROLLABLE);
    
    // Right eye (white part)
    right_eye_white = lv_obj_create(parent);
    lv_obj_set_size(right_eye_white, 60, 80);
    lv_obj_set_pos(right_eye_white, 120, 80);
    lv_obj_set_style_bg_color(right_eye_white, lv_color_white(), 0);
    lv_obj_set_style_radius(right_eye_white, 30, 0);
    lv_obj_set_style_border_width(right_eye_white, 2, 0);
    
    // Right pupil
    right_eye_pupil = lv_obj_create(right_eye_white);
    lv_obj_set_size(right_eye_pupil, 20, 20);
    lv_obj_center(right_eye_pupil);
    lv_obj_set_style_bg_color(right_eye_pupil, lv_color_black(), 0);
    lv_obj_set_style_radius(right_eye_pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(right_eye_pupil, LV_OBJ_FLAG_SCROLLABLE);
}

void move_eyes_to(int16_t x, int16_t y) {
    // Move both pupils to look at a point
    lv_obj_set_pos(left_eye_pupil, x, y);
    lv_obj_set_pos(right_eye_pupil, x, y);
}

void animate_idle_look() {
    static int16_t look_x = 10;
    static int16_t look_y = 10;
    static int direction = 0;
    
    // Random look direction every few seconds
    if (random(20) < 5) {  // 5% chance to change direction
        direction = random(8);
    }
    
    // Smooth movement patterns
    switch(direction) {
        case 0: look_x = 10; look_y = 10; break;   // Center-right
        case 1: look_x = -10; look_y = 10; break;  // Center-left
        case 2: look_x = 10; look_y = -5; break;   // Up-right
        case 3: look_x = -10; look_y = -5; break;  // Up-left
        case 4: look_x = 10; look_y = 20; break;   // Down-right
        case 5: look_x = -10; look_y = 20; break;  // Down-left
        case 6: look_x = 0; look_y = 0; break;     // Center
        case 7: look_x = 0; look_y = 15; break;    // Down
    }
    
    // Smooth animation
    lv_anim_t a_left, a_right;
    
    lv_anim_init(&a_left);
    lv_anim_set_var(&a_left, left_eye_pupil);
    lv_anim_set_exec_cb(&a_left, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a_left, lv_obj_get_x(left_eye_pupil), look_x);
    lv_anim_set_time(&a_left, 500);
    lv_anim_set_path_cb(&a_left, lv_anim_path_ease_in_out);
    lv_anim_start(&a_left);
    
    lv_anim_init(&a_right);
    lv_anim_set_var(&a_right, right_eye_pupil);
    lv_anim_set_exec_cb(&a_right, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a_right, lv_obj_get_x(right_eye_pupil), look_x);
    lv_anim_set_time(&a_right, 500);
    lv_anim_set_path_cb(&a_right, lv_anim_path_ease_in_out);
    lv_anim_start(&a_right);
}

void setup() {
    Serial.begin(115200);
    lv_init();
    lv_tick_set_cb(my_tick);
    
    lv_display_t *disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, TFT_ROTATION);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    
    create_eyes(lv_screen_active());
}

void loop() {
    static unsigned long last_eye_move = 0;
    
    // Move eyes every 2-4 seconds
    if (millis() - last_eye_move > random(2000, 4000)) {
        animate_idle_look();
        last_eye_move = millis();
    }
    
    lv_timer_handler();
    delay(5);
}