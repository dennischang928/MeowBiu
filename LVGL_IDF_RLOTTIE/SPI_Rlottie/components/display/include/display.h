#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void SPI_Setup(void);               // init SPI bus
void LVGL_Setup(void);              // init LVGL

#ifdef __cplusplus
}
#endif
