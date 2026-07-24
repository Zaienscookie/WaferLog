#include "waferlog_services.h"

#include <string.h>

static bool recording_active;
static bool wifi_connected;
static bool ble_enabled;
static bool wifi_scan_ready;
static bool ble_scan_ready;

static const char * wifi_networks[] = {
    "WaferLog Lab",
    "Tuya T5AI",
    "Home Office",
    "Demo Guest"
};

static const int32_t wifi_signals[] = {92, 78, 64, 47};
static const bool wifi_secured[] = {true, true, true, false};

static const char * ble_devices[] = {
    "T5AI Board",
    "WaferLog Pen",
    "Desk Controller"
};

static const int32_t ble_signals[] = {88, 72, 55};

void waferlog_services_init(void)
{
    recording_active = false;
    wifi_connected = false;
    ble_enabled = false;
    wifi_scan_ready = false;
    ble_scan_ready = false;
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
                     strlen(ssid) > 0;
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

bool waferlog_wifi_scan(void)
{
    wifi_scan_ready = true;
    return wifi_scan_ready;
}

uint32_t waferlog_wifi_scan_count(void)
{
    return wifi_scan_ready ? (uint32_t)(sizeof(wifi_networks) / sizeof(wifi_networks[0])) : 0U;
}

const char * waferlog_wifi_scan_ssid(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return NULL;
    }
    return wifi_networks[index];
}

int32_t waferlog_wifi_scan_signal(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return 0;
    }
    return wifi_signals[index];
}

bool waferlog_wifi_scan_secured(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return false;
    }
    return wifi_secured[index];
}

bool waferlog_note_upload(void)
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

bool waferlog_ble_scan(void)
{
    ble_scan_ready = true;
    return ble_scan_ready;
}

uint32_t waferlog_ble_scan_count(void)
{
    return ble_scan_ready ? (uint32_t)(sizeof(ble_devices) / sizeof(ble_devices[0])) : 0U;
}

const char * waferlog_ble_scan_name(uint32_t index)
{
    if(!ble_scan_ready || index >= waferlog_ble_scan_count()) {
        return NULL;
    }
    return ble_devices[index];
}

int32_t waferlog_ble_scan_signal(uint32_t index)
{
    if(!ble_scan_ready || index >= waferlog_ble_scan_count()) {
        return 0;
    }
    return ble_signals[index];
}
