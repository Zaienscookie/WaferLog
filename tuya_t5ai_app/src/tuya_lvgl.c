#include "tuya_kconfig.h"
#include "lv_vendor.h"

void waferlog_tuya_lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);
}

void waferlog_tuya_lvgl_lock(void)
{
    lv_vendor_disp_lock();
}

void waferlog_tuya_lvgl_unlock(void)
{
    lv_vendor_disp_unlock();
}
