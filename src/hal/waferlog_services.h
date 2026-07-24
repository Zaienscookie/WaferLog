#ifndef WAFERLOG_SERVICES_H
#define WAFERLOG_SERVICES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void waferlog_services_init(void);

bool waferlog_recording_start(void);
void waferlog_recording_stop(void);
bool waferlog_recording_is_active(void);

bool waferlog_wifi_connect(const char * ssid, const char * password);
void waferlog_wifi_disconnect(void);
bool waferlog_wifi_is_connected(void);
bool waferlog_note_upload(void);

bool waferlog_ble_enable(void);
void waferlog_ble_disable(void);
bool waferlog_ble_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif
