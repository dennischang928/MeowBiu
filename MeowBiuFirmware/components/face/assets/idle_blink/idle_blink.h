#ifndef IDLE_BLINK_ANIM_H
#define IDLE_BLINK_ANIM_H

// Auto-included frame definitions for 'idle_blink'
#include "idle_blink_frame_001.c"
#include "idle_blink_frame_002.c"
#include "idle_blink_frame_003.c"
#include "idle_blink_frame_004.c"
#include "idle_blink_frame_005.c"
#include "idle_blink_frame_006.c"
#include "idle_blink_frame_007.c"
#include "idle_blink_frame_008.c"
#include "idle_blink_frame_009.c"
#include "idle_blink_frame_010.c"

// Array of frame pointers (NULL-terminated)
static const lv_image_dsc_t * idle_blink_anim_frames[] = {
    &idle_blink_frame_001,
    &idle_blink_frame_002,
    &idle_blink_frame_003,
    &idle_blink_frame_004,
    &idle_blink_frame_005,
    &idle_blink_frame_006,
    &idle_blink_frame_007,
    &idle_blink_frame_008,
    &idle_blink_frame_009,
    &idle_blink_frame_010,
    NULL
};

#define IDLE_BLINK_ANIM_FRAME_COUNT 10

#endif // IDLE_BLINK_ANIM_H
