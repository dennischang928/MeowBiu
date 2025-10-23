#ifndef EXCITED_START_ANIM_H
#define EXCITED_START_ANIM_H

// Auto-included frame definitions for 'excited_start'
#include "excited_start_frame_001.c"
#include "excited_start_frame_002.c"
#include "excited_start_frame_003.c"
#include "excited_start_frame_004.c"
#include "excited_start_frame_005.c"
#include "excited_start_frame_006.c"
#include "excited_start_frame_007.c"
#include "excited_start_frame_008.c"

// Array of frame pointers (NULL-terminated)
static const lv_image_dsc_t * excited_start_anim_frames[] = {
    &excited_start_frame_001,
    &excited_start_frame_002,
    &excited_start_frame_003,
    &excited_start_frame_004,
    &excited_start_frame_005,
    &excited_start_frame_006,
    &excited_start_frame_007,
    &excited_start_frame_008,
    NULL
};

#define EXCITED_START_ANIM_FRAME_COUNT 8

#endif // EXCITED_START_ANIM_H
