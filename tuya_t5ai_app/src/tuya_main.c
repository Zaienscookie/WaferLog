#include "tuya_cloud_types.h"

#include <stdbool.h>

#include "board_com_api.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "waferlog_ui.h"
#include "waferlog_services.h"

void waferlog_tuya_lvgl_init(void);
void waferlog_tuya_lvgl_lock(void);
void waferlog_tuya_lvgl_unlock(void);

static void waferlog_start(void * arg)
{
    (void)arg;

    tal_log_init(TAL_LOG_LEVEL_INFO, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    if(board_register_hardware() != OPRT_OK) {
        PR_ERR("WaferLog board hardware init failed");
    }

    waferlog_tuya_lvgl_init();

    waferlog_tuya_lvgl_lock();
    waferlog_services_init();
    waferlog_ui_create();
    waferlog_tuya_lvgl_unlock();

    while(true) {
        tal_system_sleep(1000);
    }
}

void tuya_app_main(void)
{
    static THREAD_HANDLE thread;
    THREAD_CFG_T cfg = {
        .stackDepth = 12288,
        .priority = THREAD_PRIO_2,
        .thrdname = "waferlog_main",
        .psram_mode = 1,
    };
    tal_thread_create_and_start(&thread, NULL, NULL, waferlog_start, NULL, &cfg);
}
