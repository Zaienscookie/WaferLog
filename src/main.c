#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "lvgl/lvgl.h"
#include "hal/hal.h"
#include "waferlog_ui.h"

int main(int argc, char ** argv)
{
    bool landscape = argc > 1 && strcmp(argv[1], "--landscape") == 0;
    int32_t width = landscape ? 480 : 320;
    int32_t height = landscape ? 320 : 480;

    lv_init();
    sdl_hal_init(width, height);
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
