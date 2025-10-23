#include "face.h"
#include "lvgl.h"

void face_init()
{
  /*I4 format: internally ARGB8888, only ~6KB storage!*/
  LV_DRAW_BUF_DEFINE_STATIC(draw_buf, 240, 50, LV_COLOR_FORMAT_I4);
  LV_DRAW_BUF_INIT_STATIC(draw_buf);

  lv_obj_t *canvas = lv_canvas_create(lv_screen_active());
  lv_canvas_set_draw_buf(canvas, &draw_buf);
  lv_obj_center(canvas);

  /*Set up 2-color palette for B&W - this creates the ARGB8888 colors*/
  lv_canvas_set_palette(canvas, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
  lv_canvas_set_palette(canvas, 1, lv_color_to_32(lv_color_white(), LV_OPA_COVER));

  /*Fill background with white (palette index 1)*/
  lv_canvas_fill_bg(canvas, lv_color_hex(1), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);

  lv_draw_vector_dsc_t * dsc = lv_draw_vector_dsc_create(&layer);
  lv_vector_path_t * path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);

  /*Rectangle path*/
  lv_fpoint_t pts[] = {{10, 10}, {10, 40}, {230, 40}, {230, 10}};
  lv_vector_path_move_to(path, &pts[0]);
  lv_vector_path_line_to(path, &pts[1]);
  lv_vector_path_line_to(path, &pts[2]);
  lv_vector_path_line_to(path, &pts[3]);
  lv_vector_path_close(path);

  /*Use palette index 0 (black) for vector fill*/
  lv_draw_vector_dsc_set_fill_color(dsc, lv_color_black());
  lv_draw_vector_dsc_add_path(dsc, path);

  lv_draw_vector(dsc);
  lv_vector_path_delete(path);
  lv_draw_vector_dsc_delete(dsc);

  lv_canvas_finish_layer(canvas, &layer);

  /*Scale up*/
  lv_obj_set_size(canvas, 240, 240);
  lv_obj_center(canvas);
}