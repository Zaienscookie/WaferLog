#include "waferlog_services.h"

#include <string.h>

static bool recording_active;
static bool wifi_connected;
static bool ble_enabled;

void waferlog_services_init(void)
{
    recording_active = false;
    wifi_connected = false;
    ble_enabled = false;
}

bool waferlog_recording_start(void)
{
    recording_active = true;
    return recording_active;
}

void waferlog_recording_stop(void)
{
    recording_active = false;
}

bool waferlog_recording_is_active(void)
{
    return recording_active;
}

bool waferlog_wifi_connect(const char * ssid, const char * password)
{
    wifi_connected = ssid != NULL && password != NULL &&
                     strlen(ssid) > 0 && strlen(password) > 0;
    return wifi_connected;
}

void waferlog_wifi_disconnect(void)
{
    wifi_connected = false;
}

bool waferlog_wifi_is_connected(void)
{
    return wifi_connected;
}

bool waferlog_ble_enable(void)
{
    ble_enabled = true;
    return ble_enabled;
}

void waferlog_ble_disable(void)
{
    ble_enabled = false;
}

bool waferlog_ble_is_enabled(void)
{
    return ble_enabled;
}
