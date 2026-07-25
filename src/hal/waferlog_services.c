#include "waferlog_services.h"

#include <stdio.h>
#include <string.h>

#ifdef WAFERLOG_T5AI
#include "cJSON.h"
#include "http_client_interface.h"
#include "tal_api.h"
#include "tal_bluetooth.h"
#include "tal_mutex.h"
#include "tal_wifi.h"
#include "tdl_audio_manage.h"
#include "tkl_memory.h"
#endif

#define WAFERLOG_RECORDING_MAX_BYTES (512U * 1024U)

#ifndef WAFERLOG_SERVER_HOST
#define WAFERLOG_SERVER_HOST "10.68.10.82"
#endif
#ifndef WAFERLOG_SERVER_PORT
#define WAFERLOG_SERVER_PORT 3737U
#endif

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
static uint32_t device_relation_count;
static uint32_t device_task_count;

#ifdef WAFERLOG_T5AI
#define WAFERLOG_SCAN_RESULT_MAX 16U

typedef struct {
    char ssid[WIFI_SSID_LEN + 1U];
    int32_t signal;
    bool secured;
} waferlog_wifi_result_t;

typedef struct {
    char name[32];
    int32_t signal;
    uint8_t address[6];
} waferlog_ble_result_t;

static TDL_AUDIO_HANDLE_T audio_handle;
static TDL_AUDIO_INFO_T audio_info;
static MUTEX_HANDLE recording_mutex;
static MUTEX_HANDLE ble_scan_mutex;
static bool wifi_stack_ready;
static bool ble_stack_ready;
static waferlog_wifi_result_t wifi_scan_results[WAFERLOG_SCAN_RESULT_MAX];
static uint32_t wifi_scan_result_count;
static waferlog_ble_result_t ble_scan_results[WAFERLOG_SCAN_RESULT_MAX];
static uint32_t ble_scan_result_count;

static int32_t waferlog_rssi_percent(int32_t rssi)
{
    int32_t percent = (rssi + 100) * 2;
    return percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

static void waferlog_wifi_event_cb(WF_EVENT_E event, void * arg)
{
    (void)arg;
    if(event == WFE_CONNECT_FAILED || event == WFE_DISCONNECTED) {
        wifi_connected = false;
    }
    else if(event == WFE_CONNECTED) {
        wifi_connected = false;
    }
}

static void waferlog_ble_copy_name(
    const TAL_BLE_ADV_REPORT_T * report,
    char * name,
    size_t capacity
)
{
    name[0] = '\0';
    if(report == NULL || report->p_data == NULL || capacity == 0U) {
        return;
    }
    uint32_t offset = 0U;
    while(offset + 1U < report->data_len) {
        uint8_t field_len = report->p_data[offset];
        if(field_len == 0U || offset + 1U + field_len > report->data_len) {
            break;
        }
        uint8_t field_type = report->p_data[offset + 1U];
        if(field_type == 0x08U || field_type == 0x09U) {
            uint32_t name_len = field_len - 1U;
            if(name_len >= capacity) {
                name_len = (uint32_t)capacity - 1U;
            }
            memcpy(name, report->p_data + offset + 2U, name_len);
            name[name_len] = '\0';
            return;
        }
        offset += (uint32_t)field_len + 1U;
    }
    snprintf(
        name,
        capacity,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        report->peer_addr.addr[5], report->peer_addr.addr[4],
        report->peer_addr.addr[3], report->peer_addr.addr[2],
        report->peer_addr.addr[1], report->peer_addr.addr[0]
    );
}

static void waferlog_ble_event_cb(TAL_BLE_EVT_PARAMS_T * event)
{
    if(event == NULL || event->type != TAL_BLE_EVT_ADV_REPORT ||
       ble_scan_mutex == NULL) {
        return;
    }
    const TAL_BLE_ADV_REPORT_T * report = &event->ble_event.adv_report;
    if(tal_mutex_lock(ble_scan_mutex) != OPRT_OK) {
        return;
    }
    uint32_t index = 0U;
    for(; index < ble_scan_result_count; index++) {
        if(memcmp(
            ble_scan_results[index].address,
            report->peer_addr.addr,
            sizeof(ble_scan_results[index].address)
        ) == 0) {
            break;
        }
    }
    if(index < WAFERLOG_SCAN_RESULT_MAX) {
        if(index == ble_scan_result_count) {
            ble_scan_result_count++;
            memcpy(
                ble_scan_results[index].address,
                report->peer_addr.addr,
                sizeof(ble_scan_results[index].address)
            );
        }
        waferlog_ble_copy_name(report, ble_scan_results[index].name,
                               sizeof(ble_scan_results[index].name));
        ble_scan_results[index].signal = waferlog_rssi_percent(report->rssi);
        ble_scan_ready = true;
    }
    tal_mutex_unlock(ble_scan_mutex);
}
#endif

#ifndef WAFERLOG_T5AI
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
#endif

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
    device_relation_count = 0U;
    device_task_count = 0U;
#ifdef WAFERLOG_T5AI
    wifi_stack_ready = tal_wifi_init(waferlog_wifi_event_cb) == OPRT_OK;
    if(ble_scan_mutex == NULL) {
        tal_mutex_create_init(&ble_scan_mutex);
    }
    wifi_scan_result_count = 0U;
    ble_scan_result_count = 0U;
    ble_stack_ready = false;
#endif
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
#ifdef WAFERLOG_T5AI
    if(!wifi_stack_ready || ssid == NULL || password == NULL || ssid[0] == '\0') {
        return false;
    }
    if(tal_wifi_station_connect((int8_t *)ssid, (int8_t *)password) != OPRT_OK) {
        wifi_connected = false;
        return false;
    }
    wifi_connected = false;
    return true;
#else
    wifi_connected = ssid != NULL && password != NULL &&
                     strlen(ssid) > 0;
    return wifi_connected;
#endif
}

void waferlog_wifi_disconnect(void)
{
#ifdef WAFERLOG_T5AI
    if(wifi_stack_ready) {
        tal_wifi_station_disconnect();
    }
#endif
    wifi_connected = false;
}

bool waferlog_wifi_is_connected(void)
{
#ifdef WAFERLOG_T5AI
    WF_STATION_STAT_E status;
    if(wifi_stack_ready && tal_wifi_station_get_status(&status) == OPRT_OK) {
        wifi_connected = status == WSS_GOT_IP;
    }
#endif
    return wifi_connected;
}

bool waferlog_wifi_scan(void)
{
#ifdef WAFERLOG_T5AI
    AP_IF_S * access_points = NULL;
    uint32_t count = 0U;
    if(tal_wifi_all_ap_scan(&access_points, &count) != OPRT_OK) {
        wifi_scan_result_count = 0U;
        wifi_scan_ready = false;
        return false;
    }
    wifi_scan_result_count = count > WAFERLOG_SCAN_RESULT_MAX
        ? WAFERLOG_SCAN_RESULT_MAX
        : count;
    for(uint32_t i = 0U; i < wifi_scan_result_count; i++) {
        uint32_t ssid_len = access_points[i].s_len;
        if(ssid_len > WIFI_SSID_LEN) {
            ssid_len = WIFI_SSID_LEN;
        }
        memcpy(wifi_scan_results[i].ssid, access_points[i].ssid, ssid_len);
        wifi_scan_results[i].ssid[ssid_len] = '\0';
        wifi_scan_results[i].signal = waferlog_rssi_percent(access_points[i].rssi);
        wifi_scan_results[i].secured = access_points[i].security != WAAM_OPEN;
    }
    tal_wifi_release_ap(access_points);
    wifi_scan_ready = true;
    return true;
#else
    wifi_scan_ready = true;
    return wifi_scan_ready;
#endif
}

uint32_t waferlog_wifi_scan_count(void)
{
#ifdef WAFERLOG_T5AI
    return wifi_scan_ready ? wifi_scan_result_count : 0U;
#else
    return wifi_scan_ready ? (uint32_t)(sizeof(wifi_networks) / sizeof(wifi_networks[0])) : 0U;
#endif
}

const char * waferlog_wifi_scan_ssid(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return NULL;
    }
#ifdef WAFERLOG_T5AI
    return wifi_scan_results[index].ssid;
#else
    return wifi_networks[index];
#endif
}

int32_t waferlog_wifi_scan_signal(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return 0;
    }
#ifdef WAFERLOG_T5AI
    return wifi_scan_results[index].signal;
#else
    return wifi_signals[index];
#endif
}

bool waferlog_wifi_scan_secured(uint32_t index)
{
    if(!wifi_scan_ready || index >= waferlog_wifi_scan_count()) {
        return false;
    }
#ifdef WAFERLOG_T5AI
    return wifi_scan_results[index].secured;
#else
    return wifi_secured[index];
#endif
}

 #ifdef WAFERLOG_T5AI
static bool waferlog_http_request(
    const char * method,
    const char * path,
    const char * content_type,
    const uint8_t * body,
    size_t body_length,
    const char * extra_key,
    const char * extra_value,
    http_client_response_t * response
)
{
    http_client_header_t headers[3];
    uint8_t header_count = 0U;
    headers[header_count++] = (http_client_header_t){"Content-Type", content_type};
    if(extra_key != NULL && extra_value != NULL) {
        headers[header_count++] = (http_client_header_t){extra_key, extra_value};
    }
    headers[header_count++] = (http_client_header_t){"X-WaferLog-Device", "waferlog-t5ai"};
    http_client_status_t status = http_client_request(
        &(const http_client_request_t){
            .host = WAFERLOG_SERVER_HOST,
            .port = WAFERLOG_SERVER_PORT,
            .path = path,
            .method = method,
            .headers = headers,
            .headers_count = header_count,
            .body = body,
            .body_length = body_length,
            .timeout_ms = 10000U,
        },
        response
    );
    bool ok = status == HTTP_CLIENT_SUCCESS &&
              response->status_code >= 200U && response->status_code < 300U;
    PR_INFO("WaferLog HTTP %s %s status=%u result=%s", method, path,
            response != NULL ? (unsigned)response->status_code : 0U,
            ok ? "ok" : "failed");
    return ok;
}

static bool waferlog_upload_audio(char * audio_id, size_t audio_id_capacity)
{
    if(!recording_ready || recording_buffer == NULL || recording_data_size == 0U) {
        return false;
    }
    char sample_rate[16];
    char channels[8];
    char bits[8];
    snprintf(sample_rate, sizeof(sample_rate), "%u", (unsigned)recording_sample_rate);
    snprintf(channels, sizeof(channels), "%u", (unsigned)recording_channels);
    snprintf(bits, sizeof(bits), "%u", (unsigned)recording_bits);
    http_client_header_t headers[] = {
        {"Content-Type", "audio/pcm"},
        {"X-WaferLog-Device", "waferlog-t5ai"},
        {"X-WaferLog-Sample-Rate", sample_rate},
        {"X-WaferLog-Channels", channels},
        {"X-WaferLog-Bits", bits},
    };
    http_client_response_t response = {0};
    http_client_status_t status = http_client_request(
        &(const http_client_request_t){
            .host = WAFERLOG_SERVER_HOST,
            .port = WAFERLOG_SERVER_PORT,
            .path = "/api/device/audio",
            .method = "POST",
            .headers = headers,
            .headers_count = sizeof(headers) / sizeof(headers[0]),
            .body = recording_buffer,
            .body_length = recording_data_size,
            .timeout_ms = 15000U,
        },
        &response
    );
    bool ok = false;
    if(status == HTTP_CLIENT_SUCCESS && response.status_code >= 200U &&
       response.status_code < 300U && response.body != NULL) {
        cJSON *root = cJSON_ParseWithLength((const char *)response.body, response.body_length);
        cJSON *id = root != NULL ? cJSON_GetObjectItem(root, "id") : NULL;
        if(cJSON_IsString(id) && id->valuestring != NULL && audio_id_capacity > 0U) {
            snprintf(audio_id, audio_id_capacity, "%s", id->valuestring);
            ok = true;
        }
        cJSON_Delete(root);
    }
    http_client_free(&response);
    return ok;
}

static bool waferlog_upload_note(
    const char * title,
    const char * raw_content,
    const char * raw_text,
    const char * audio_id
)
{
    cJSON *root = cJSON_CreateObject();
    if(root == NULL) {
        return false;
    }
    cJSON_AddStringToObject(root, "deviceId", "waferlog-t5ai");
    cJSON_AddStringToObject(root, "title", title != NULL ? title : "WaferLog note");
    cJSON_AddStringToObject(root, "inputType", "handwriting");
    cJSON_AddStringToObject(root, "rawContent", raw_content != NULL ? raw_content : "");
    cJSON_AddStringToObject(root, "rawText", raw_text != NULL ? raw_text : "");
    if(audio_id != NULL && audio_id[0] != '\0') {
        cJSON_AddStringToObject(root, "audioId", audio_id);
        cJSON_AddStringToObject(root, "audioMime", "audio/pcm");
        cJSON_AddNumberToObject(root, "audioSize", recording_data_size);
        cJSON_AddNumberToObject(root, "audioSampleRate", recording_sample_rate);
        cJSON_AddNumberToObject(root, "audioChannels", recording_channels);
        cJSON_AddNumberToObject(root, "audioBits", recording_bits);
    }
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if(body == NULL) {
        return false;
    }
    http_client_response_t response = {0};
    bool ok = waferlog_http_request(
        "POST", "/api/device/notes", "application/json", (const uint8_t *)body,
        strlen(body), NULL, NULL, &response
    );
    char note_id[64] = {0};
    if(ok && response.body != NULL) {
        cJSON *response_root = cJSON_ParseWithLength(
            (const char *)response.body, response.body_length
        );
        cJSON *note = response_root != NULL
            ? cJSON_GetObjectItem(response_root, "note") : NULL;
        cJSON *id = note != NULL ? cJSON_GetObjectItem(note, "id") : NULL;
        if(cJSON_IsString(id) && id->valuestring != NULL) {
            snprintf(note_id, sizeof(note_id), "%s", id->valuestring);
        }
        cJSON_Delete(response_root);
    }
    http_client_free(&response);
    cJSON_free(body);
    if(!ok || note_id[0] == '\0') {
        return false;
    }

    char path[128];
    snprintf(path, sizeof(path), "/api/device/notes/%s?deviceId=waferlog-t5ai", note_id);
    http_client_response_t sync_response = {0};
    ok = waferlog_http_request(
        "GET", path, "application/json", NULL, 0U, NULL, NULL, &sync_response
    );
    if(ok && sync_response.body != NULL) {
        cJSON *root = cJSON_ParseWithLength(
            (const char *)sync_response.body, sync_response.body_length
        );
        cJSON *relations = root != NULL ? cJSON_GetObjectItem(root, "relations") : NULL;
        cJSON *tasks = root != NULL ? cJSON_GetObjectItem(root, "tasks") : NULL;
        device_relation_count = cJSON_IsArray(relations)
            ? (uint32_t)cJSON_GetArraySize(relations) : 0U;
        device_task_count = cJSON_IsArray(tasks)
            ? (uint32_t)cJSON_GetArraySize(tasks) : 0U;
        cJSON_Delete(root);
    }
    http_client_free(&sync_response);
    return ok;
}
#endif

bool waferlog_device_sync(void)
{
#ifdef WAFERLOG_T5AI
    if(!waferlog_wifi_is_connected()) {
        PR_INFO("WaferLog sync skipped: Wi-Fi has no IP");
        return false;
    }
    http_client_response_t response = {0};
    bool ok = waferlog_http_request(
        "GET",
        "/api/device/sync?deviceId=waferlog-t5ai&limit=8",
        "application/json",
        NULL,
        0U,
        NULL,
        NULL,
        &response
    );
    if(ok && response.body != NULL) {
        cJSON *root = cJSON_ParseWithLength(
            (const char *)response.body, response.body_length
        );
        cJSON *items = root != NULL ? cJSON_GetObjectItem(root, "items") : NULL;
        if(cJSON_IsArray(items) && cJSON_GetArraySize(items) > 0) {
            cJSON *item = cJSON_GetArrayItem(items, cJSON_GetArraySize(items) - 1);
            cJSON *relations = item != NULL ? cJSON_GetObjectItem(item, "relations") : NULL;
            cJSON *tasks = item != NULL ? cJSON_GetObjectItem(item, "tasks") : NULL;
            device_relation_count = cJSON_IsArray(relations)
                ? (uint32_t)cJSON_GetArraySize(relations) : 0U;
            device_task_count = cJSON_IsArray(tasks)
                ? (uint32_t)cJSON_GetArraySize(tasks) : 0U;
        }
        cJSON_Delete(root);
    }
    http_client_free(&response);
    return ok;
#else
    return wifi_connected;
#endif
}

uint32_t waferlog_device_relation_count(void)
{
    return device_relation_count;
}

uint32_t waferlog_device_task_count(void)
{
    return device_task_count;
}

bool waferlog_note_upload(void)
{
    return waferlog_content_upload(true, false);
}

bool waferlog_note_upload_payload(
    const char * title,
    const char * raw_content,
    const char * raw_text
)
{
#ifdef WAFERLOG_T5AI
    if(!wifi_connected || raw_content == NULL || raw_content[0] == '\0') {
        return false;
    }
    char audio_id[64] = {0};
    if(recording_ready && !waferlog_upload_audio(audio_id, sizeof(audio_id))) {
        return false;
    }
    return waferlog_upload_note(title, raw_content, raw_text, audio_id);
#else
    (void)title;
    (void)raw_content;
    (void)raw_text;
    return true;
#endif
}

bool waferlog_recording_upload(void)
{
#ifdef WAFERLOG_T5AI
    if(!wifi_connected) {
        return false;
    }
    char audio_id[64] = {0};
    return waferlog_upload_audio(audio_id, sizeof(audio_id));
#else
    return waferlog_content_upload(false, true);
#endif
}

bool waferlog_content_upload(bool include_note, bool include_recording)
{
#ifdef WAFERLOG_T5AI
    if(include_note) {
        return false;
    }
    if(!wifi_connected || !include_recording) {
        return false;
    }
    return waferlog_recording_upload();
#else
    if(!wifi_connected || (!include_note && !include_recording)) {
        return false;
    }
    if(include_recording && !recording_ready) {
        return false;
    }
    note_upload_ready = include_note;
    recording_upload_ready = include_recording;
    return true;
#endif
}

bool waferlog_ble_enable(void)
{
#ifdef WAFERLOG_T5AI
    if(!ble_stack_ready) {
        if(tal_ble_bt_init(TAL_BLE_ROLE_CENTRAL, waferlog_ble_event_cb) != OPRT_OK) {
            return false;
        }
        ble_stack_ready = true;
    }
#endif
    ble_enabled = true;
    return ble_enabled;
}

void waferlog_ble_disable(void)
{
#ifdef WAFERLOG_T5AI
    tal_ble_scan_stop();
    if(ble_stack_ready) {
        tal_ble_bt_deinit(TAL_BLE_ROLE_CENTRAL);
        ble_stack_ready = false;
    }
    if(ble_scan_mutex != NULL && tal_mutex_lock(ble_scan_mutex) == OPRT_OK) {
        ble_scan_result_count = 0U;
        tal_mutex_unlock(ble_scan_mutex);
    }
#endif
    ble_enabled = false;
    ble_scan_ready = false;
}

bool waferlog_ble_is_enabled(void)
{
    return ble_enabled;
}

bool waferlog_ble_scan(void)
{
#ifdef WAFERLOG_T5AI
    if(!ble_enabled && !waferlog_ble_enable()) {
        return false;
    }
    if(ble_scan_mutex != NULL && tal_mutex_lock(ble_scan_mutex) == OPRT_OK) {
        ble_scan_result_count = 0U;
        tal_mutex_unlock(ble_scan_mutex);
    }
    TAL_BLE_SCAN_PARAMS_T scan_config = {
        .type = TAL_BLE_SCAN_TYPE_ACTIVE,
        .scan_interval = 0x400,
        .scan_window = 0x400,
        .timeout = 8,
        .filter_dup = 1,
    };
    ble_scan_ready = tal_ble_scan_start(&scan_config) == OPRT_OK;
    return ble_scan_ready;
#else
    ble_scan_ready = true;
    return ble_scan_ready;
#endif
}

uint32_t waferlog_ble_scan_count(void)
{
#ifdef WAFERLOG_T5AI
    return ble_scan_ready ? ble_scan_result_count : 0U;
#else
    return ble_scan_ready ? (uint32_t)(sizeof(ble_devices) / sizeof(ble_devices[0])) : 0U;
#endif
}

const char * waferlog_ble_scan_name(uint32_t index)
{
    if(!ble_scan_ready || index >= waferlog_ble_scan_count()) {
        return NULL;
    }
#ifdef WAFERLOG_T5AI
    return ble_scan_results[index].name;
#else
    return ble_devices[index];
#endif
}

int32_t waferlog_ble_scan_signal(uint32_t index)
{
    if(!ble_scan_ready || index >= waferlog_ble_scan_count()) {
        return 0;
    }
#ifdef WAFERLOG_T5AI
    return ble_scan_results[index].signal;
#else
    return ble_signals[index];
#endif
}
