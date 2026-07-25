#include "tuya_cloud_types.h"

#include <stdbool.h>

#include "board_com_api.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "waferlog_ui.h"
#include "waferlog_services.h"

#ifdef WAFERLOG_HAS_LOCAL_CONFIG
#include "waferlog_local_config.h"
#endif

void waferlog_tuya_lvgl_init(void);
void waferlog_tuya_lvgl_lock(void);
void waferlog_tuya_lvgl_unlock(void);

#ifdef WAFERLOG_HAS_LOCAL_CONFIG
static void waferlog_cloud_probe(void * arg)
{
    (void)arg;
    for(uint32_t attempt = 0U; attempt < 3U; attempt++) {
        if(waferlog_wifi_connect(WAFERLOG_WIFI_SSID, WAFERLOG_WIFI_PASSWORD)) {
            PR_INFO("WaferLog Wi-Fi connect requested");
            for(uint32_t wait = 0U; wait < 30U; wait++) {
                if(waferlog_wifi_is_connected()) {
                    PR_INFO("WaferLog Wi-Fi got IP; probing cloud server");
                    PR_INFO("WaferLog cloud sync result=%s",
                            waferlog_device_sync() ? "ok" : "failed");
                    return;
                }
                tal_system_sleep(1000U);
            }
        }
        PR_WARN("WaferLog Wi-Fi connection attempt %u failed",
                (unsigned)(attempt + 1U));
        tal_system_sleep(2000U);
    }
    PR_ERR("WaferLog Wi-Fi cloud probe failed");
}
#endif

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

#ifdef WAFERLOG_HAS_LOCAL_CONFIG
    static THREAD_HANDLE cloud_thread;
    THREAD_CFG_T cloud_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_3,
        .thrdname = "waferlog_cloud",
        .psram_mode = 1,
    };
    tal_thread_create_and_start(
        &cloud_thread, NULL, NULL, waferlog_cloud_probe, NULL, &cloud_cfg
    );
#endif

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
