#include "face.h"
#include "esp_log.h"
#include "lvgl.h"

#include "./assets/output/excited_start.c"
// #include "./assets/output/excited_start_trans.c"

#include "./assets/output/excited_loop.c"
#include "./assets/output/excited_end.c"

static lv_obj_t *img;

// LV_IMG_DECLARE(excited_start_trans);
LV_IMG_DECLARE(excited_start);
LV_IMG_DECLARE(excited_loop);
LV_IMG_DECLARE(excited_end);

static void gif_finished_cb_3(lv_event_t *e);

void gif_finished_cb_2(lv_event_t *e)
{
    ESP_LOGI("face", "GIF finished, switching to end GIF");
    
    img = lv_gif_create(lv_screen_active());

    lv_gif_set_src(img, &excited_end);
    ESP_LOGI("face", "Set source to loop GIF");
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_scale(img, 700);
    lv_image_set_antialias(img, true);

    // CRITICAL: Start the GIF!
    lv_gif_restart(img); // Make sure this is here
    ESP_LOGI("face", "Restarted");
    lv_gif_set_loop_count(img, 1);
    lv_obj_add_event_cb(img, gif_finished_cb_3, LV_EVENT_READY, NULL);
}

static void gif_finished_cb(lv_event_t *e)
{
    ESP_LOGI("face", "GIF finished, switching to loop GIF");

    ESP_LOGI("face", "Deleted previous img object");

    img = lv_gif_create(lv_screen_active());

    lv_gif_set_src(img, &excited_loop);
    ESP_LOGI("face", "Set source to loop GIF");
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_scale(img, 700);
    lv_image_set_antialias(img, true);
    // CRITICAL: Start the GIF!
    lv_gif_restart(img); // Make sure this is here
    ESP_LOGI("face", "Restarted");
    lv_gif_set_loop_count(img, 2);
    lv_obj_add_event_cb(img, gif_finished_cb_2, LV_EVENT_READY, NULL);
}


void draw_gif()
{
    img = lv_gif_create(lv_screen_active());

    // lv_gif_set_src(img, &excited_start_trans); // Set source FIRST
    lv_gif_set_src(img, &excited_start); // Set source FIRST
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_scale(img, 700);
    lv_gif_set_loop_count(img, 1); // Set loop count AFTER source

    // Add event callback for when GIF finishes
    lv_obj_add_event_cb(img, gif_finished_cb, LV_EVENT_READY, NULL);
}

void gif_finished_cb_3(lv_event_t *e){
    draw_gif();
}

void face_init()
{
    draw_gif();
}