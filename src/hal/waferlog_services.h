#ifndef WAFERLOG_SERVICES_H
#define WAFERLOG_SERVICES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void waferlog_services_init(void);

bool waferlog_recording_start(void);
void waferlog_recording_stop(void);
bool waferlog_recording_is_active(void);
bool waferlog_recording_is_ready(void);
uint32_t waferlog_recording_size(void);
bool waferlog_recording_get_info(
    uint32_t * sample_rate,
    uint16_t * channels,
    uint16_t * bits
);
bool waferlog_recording_read(
    uint32_t offset,
    uint8_t * buffer,
    uint32_t capacity,
    uint32_t * read_size
);

bool waferlog_wifi_connect(const char * ssid, const char * password);
void waferlog_wifi_disconnect(void);
bool waferlog_wifi_is_connected(void);
bool waferlog_wifi_is_connecting(void);
bool waferlog_wifi_connection_failed(void);
bool waferlog_wifi_scan(void);
bool waferlog_wifi_scan_in_progress(void);
uint32_t waferlog_wifi_scan_count(void);
const char * waferlog_wifi_scan_ssid(uint32_t index);
int32_t waferlog_wifi_scan_signal(uint32_t index);
bool waferlog_wifi_scan_secured(uint32_t index);
bool waferlog_note_upload(void);
bool waferlog_note_upload_payload(
    const char * title,
    const char * raw_content,
    const char * raw_text
);
bool waferlog_device_sync(void);
uint32_t waferlog_device_relation_count(void);
uint32_t waferlog_device_task_count(void);
bool waferlog_recording_upload(void);
bool waferlog_content_upload(bool include_note, bool include_recording);

bool waferlog_ble_enable(void);
void waferlog_ble_disable(void);
bool waferlog_ble_is_enabled(void);
bool waferlog_ble_scan(void);
uint32_t waferlog_ble_scan_count(void);
const char * waferlog_ble_scan_name(uint32_t index);
int32_t waferlog_ble_scan_signal(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
