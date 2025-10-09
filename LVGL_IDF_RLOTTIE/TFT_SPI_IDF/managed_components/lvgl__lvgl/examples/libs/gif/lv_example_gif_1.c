#include "../../lv_examples.h"
#if LV_USE_GIF && LV_BUILD_EXAMPLES

/**
 * Open a GIF image from a file and a variable
 */
void lv_example_gif_1(void)
{
    LV_IMAGE_DECLARE(img_bulb_gif);
    lv_obj_t * img;

    img = lv_gif_create(lv_screen_active());
    lv_gif_set_src(img, &img_bulb_gif);
    /* Scale the GIF up (512 = 2x). Use lv_image_set_scale since GIF is an image object. */
    lv_image_set_scale(img, 512);
    lv_image_set_antialias(img, true);
    lv_obj_align(img, LV_ALIGN_CENTER, 0 ,0);

    // img = lv_gif_create(lv_screen_active());
    /* Assuming a File system is attached to letter 'A'
     * E.g. set LV_USE_FS_STDIO 'A' in lv_conf.h */
    // lv_gif_set_src(img, "A:lvgl/examples/libs/gif/bulb.gif");
    /* Scale the GIF up (512 = 2x) and enable antialiasing */
    // lv_image_set_scale(img, 512);
    // lv_image_set_antialias(img, true);
    // lv_obj_align(img, LV_ALIGN_RIGHT_MID, -20, 0);
}

#endif
