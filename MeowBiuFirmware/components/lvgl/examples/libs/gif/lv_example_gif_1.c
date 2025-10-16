#include "../../lv_examples.h"
#if LV_USE_GIF && LV_BUILD_EXAMPLES
// #include "a.c"
/**
 * Open a GIF image from a file and a variable
 */
void lv_example_gif_1(void)
{
    LV_IMAGE_DECLARE(WTF_clean);
    lv_obj_t * img;

    img = lv_gif_create(lv_screen_active());
    lv_gif_set_src(img, &WTF_clean);
    /* Scale the GIF up (512 = 2x). Use lv_image_set_scale since GIF is an image object. */
    lv_image_set_scale(img, 720);
    lv_image_set_antialias(img, true);
    lv_obj_align(img, LV_ALIGN_CENTER, 0 ,0);

}

#endif
