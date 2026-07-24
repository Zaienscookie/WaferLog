#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdint.h>

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "lvgl/lvgl.h"
#include "hal/hal.h"
#include "waferlog_ui.h"
// 初始化
int main(void)
{
    lv_init();
    sdl_hal_init(320, 480);
    waferlog_ui_create();

    while(1) {
        uint32_t sleep_time_ms = lv_timer_handler();
        if(sleep_time_ms == LV_NO_TIMER_READY) {
            sleep_time_ms = LV_DEF_REFR_PERIOD;
        }
#ifdef _MSC_VER
        Sleep(sleep_time_ms);
#else
        usleep(sleep_time_ms * 1000U);
#endif
    }

    return 0;
}
