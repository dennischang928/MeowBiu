#include <lvgl.h>
#include <TFT_eSPI.h>

#define TFT_HOR_RES 240
#define TFT_VER_RES 240
#define TFT_ROTATION LV_DISPLAY_ROTATION_0

#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  lv_display_flush_ready(disp);
}

static void my_log_cb(signed char, const char *buf)
{
  Serial.println(buf); // Print logs to Serial Monitor
}
/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
  return millis();
}

void setup()
{
  // Don't TOUCH THIS SECTION
  Serial.begin(115200);
  lv_init();
  lv_tick_set_cb(my_tick);
  lv_log_register_print_cb(my_log_cb);
  lv_display_t *disp;
  disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, TFT_ROTATION);
  // Don't TOUCH THIS SECTION


  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);           /* let this time pass */
}