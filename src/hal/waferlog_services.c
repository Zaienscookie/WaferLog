#include "waferlog_services.h"

#include <string.h>

#ifdef WAFERLOG_T5AI
#include "tal_api.h"
#include "tal_mutex.h"
#include "tdl_audio_manage.h"
#include "tkl_memory.h"
#endif

#define WAFERLOG_RECORDING_MAX_BYTES (512U * 1024U)

static bool recording_active;
static bool recording_ready;
static bool wifi_connected;
static bool ble_enabled;
static bool wifi_scan_ready;
static bool ble_scan_ready;
static uint8_t * recording_buffer;
static uint32_t recording_buffer_size;
static uint32_t recording_data_size;
static uint32_t recording_sample_rate = 16000U;
static uint16_t recording_channels = 1U;
static uint16_t recording_bits = 16U;
static bool note_upload_ready;
static bool recording_upload_ready;

#ifdef WAFERLOG_T5AI
static TDL_AUDIO_HANDLE_T audio_handle;
static TDL_AUDIO_INFO_T audio_info;
static MUTEX_HANDLE recording_mutex;
#endif

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

#ifdef WAFERLOG_T5AI
static void waferlog_audio_frame_cb(
    TDL_AUDIO_FRAME_FORMAT_E type,
    TDL_AUDIO_STATUS_E status,
    uint8_t * data,
    uint32_t len
)
{
    (void)status;
    if(type != TDL_AUDIO_FRAME_FORMAT_PCM ||
       data == NULL ||
       len == 0U ||
       !recording_active ||
       recording_mutex == NULL) {
        return;
    }

    if(tal_mutex_lock(recording_mutex) != OPRT_OK) {
        return;
    }
    uint32_t remaining = recording_buffer_size - recording_data_size;
    uint32_t copy_size = len < remaining ? len : remaining;
    if(copy_size > 0U && recording_buffer != NULL) {
        memcpy(recording_buffer + recording_data_size, data, copy_size);
        recording_data_size += copy_size;
    }
    tal_mutex_unlock(recording_mutex);
}

static bool waferlog_audio_prepare(void)
{
    if(recording_mutex == NULL &&
       tal_mutex_create_init(&recording_mutex) != OPRT_OK) {
        return false;
    }
    if(audio_handle == NULL) {
        if(tdl_audio_find(AUDIO_CODEC_NAME, &audio_handle) != OPRT_OK) {
            return false;
        }
        if(tdl_audio_get_info(audio_handle, &audio_info) == OPRT_OK) {
            if(audio_info.sample_rate > 0U) {
                recording_sample_rate = audio_info.sample_rate;
            }
            if(audio_info.sample_ch_num > 0U) {
                recording_channels = audio_info.sample_ch_num;
            }
            if(audio_info.sample_bits > 0U) {
                recording_bits = audio_info.sample_bits;
            }
        }
    }
    if(recording_buffer == NULL) {
        recording_buffer = tkl_system_psram_malloc(WAFERLOG_RECORDING_MAX_BYTES);
        if(recording_buffer == NULL) {
            return false;
        }
        recording_buffer_size = WAFERLOG_RECORDING_MAX_BYTES;
    }
    return true;
}
#endif

void waferlog_services_init(void)
{
    recording_active = false;
    recording_ready = false;
    wifi_connected = false;
    ble_enabled = false;
    wifi_scan_ready = false;
    ble_scan_ready = false;
    recording_data_size = 0U;
    note_upload_ready = false;
    recording_upload_ready = false;
}

bool waferlog_recording_start(void)
{
#ifdef WAFERLOG_T5AI
    if(!waferlog_audio_prepare()) {
        return false;
    }
    if(tdl_audio_open(audio_handle, waferlog_audio_frame_cb) != OPRT_OK) {
        return false;
    }
    if(tal_mutex_lock(recording_mutex) == OPRT_OK) {
        recording_data_size = 0U;
        recording_ready = false;
        tal_mutex_unlock(recording_mutex);
    }
#endif
    recording_active = true;
    return recording_active;
}

void waferlog_recording_stop(void)
{
    recording_active = false;
#ifdef WAFERLOG_T5AI
    if(recording_mutex != NULL && tal_mutex_lock(recording_mutex) == OPRT_OK) {
        recording_ready = recording_data_size > 0U;
        tal_mutex_unlock(recording_mutex);
    }
#else
    recording_ready = true;
#endif
}

bool waferlog_recording_is_active(void)
{
    return recording_active;
}

bool waferlog_recording_is_ready(void)
{
    return recording_ready;
}

uint32_t waferlog_recording_size(void)
{
#ifdef WAFERLOG_T5AI
    uint32_t size = 0U;
    if(recording_mutex != NULL && tal_mutex_lock(recording_mutex) == OPRT_OK) {
        size = recording_data_size;
        tal_mutex_unlock(recording_mutex);
    }
    return size;
#else
    return recording_ready ? recording_data_size : 0U;
#endif
}

bool waferlog_recording_get_info(
    uint32_t * sample_rate,
    uint16_t * channels,
    uint16_t * bits
)
{
    if(sample_rate == NULL || channels == NULL || bits == NULL) {
        return false;
    }
    *sample_rate = recording_sample_rate;
    *channels = recording_channels;
    *bits = recording_bits;
    return true;
}

bool waferlog_recording_read(
    uint32_t offset,
    uint8_t * buffer,
    uint32_t capacity,
    uint32_t * read_size
)
{
    if(buffer == NULL || read_size == NULL || offset >= recording_data_size) {
        return false;
    }
    if(recording_buffer == NULL) {
        return false;
    }
    uint32_t available = recording_data_size - offset;
    uint32_t copy_size = capacity < available ? capacity : available;
#ifdef WAFERLOG_T5AI
    if(recording_mutex == NULL ||
       tal_mutex_lock(recording_mutex) != OPRT_OK) {
        return false;
    }
#endif
    memcpy(buffer, recording_buffer + offset, copy_size);
#ifdef WAFERLOG_T5AI
    tal_mutex_unlock(recording_mutex);
#endif
    *read_size = copy_size;
    return true;
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
    return waferlog_content_upload(true, false);
}

bool waferlog_recording_upload(void)
{
    return waferlog_content_upload(false, true);
}

bool waferlog_content_upload(bool include_note, bool include_recording)
{
    if(!wifi_connected || (!include_note && !include_recording)) {
        return false;
    }
    if(include_recording && !recording_ready) {
        return false;
    }
    note_upload_ready = include_note;
    recording_upload_ready = include_recording;
    return true;
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
