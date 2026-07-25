#include "waferlog_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef WAFERLOG_T5AI
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "hal/waferlog_services.h"

LV_FONT_DECLARE(waferlog_font_14);
LV_FONT_DECLARE(waferlog_font_16);

#define HEADER_HEIGHT 58
#define FOOTER_HEIGHT 64
#define LANDSCAPE_FOOTER_HEIGHT 52
#define NOTE_PAGE_COUNT 12
#ifndef NOTE_STROKE_LIMIT
#define NOTE_STROKE_LIMIT 4096
#endif
#define NOTE_MIN_SAMPLE_DISTANCE 3
#define NOTE_CANVAS_MAX_WIDTH 640
#define NOTE_CANVAS_MAX_HEIGHT 464

typedef enum {
    PAGE_NOTES,
    PAGE_CALENDAR,
    PAGE_HOME
} app_page_t;

typedef enum {
    PAPER_BLANK,
    PAPER_RULED,
    PAPER_GRID,
    PAPER_DOTS
} paper_style_t;

typedef struct {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    uint8_t width;
    uint8_t color_index;
} note_stroke_t;

typedef struct {
    note_stroke_t strokes[NOTE_STROKE_LIMIT];
    uint16_t stroke_count;
    uint8_t paper;
} note_page_t;

typedef struct {
    uint32_t accent;
    uint32_t accent_soft;
    uint32_t quick;
    uint32_t media;
} accent_theme_t;

typedef enum {
    TEXT_NOTES,
    TEXT_BLANK,
    TEXT_RULED,
    TEXT_GRID,
    TEXT_DOTS,
    TEXT_SAVED_LOCALLY,
    TEXT_UPLOAD_COMPLETE,
    TEXT_CONNECT_WIFI_FIRST,
    TEXT_PAGE_CLEARED,
    TEXT_NOTHING_TO_SAVE,
    TEXT_NOTHING_TO_UPLOAD,
    TEXT_PAGE_FULL,
    TEXT_WIFI,
    TEXT_BLUETOOTH,
    TEXT_TAP_TO_CONNECT,
    TEXT_TAP_TO_ENABLE,
    TEXT_LATEST_NOTE,
    TEXT_NO_SAVED_PAGE,
    TEXT_SAVED_STATE,
    TEXT_UPLOADED_STATE,
    TEXT_UNSAVED_STATE,
    TEXT_QUICK_CAPTURE,
    TEXT_CAPTURE_TODAY,
    TEXT_KEEP_IDEA,
    TEXT_START_WRITING,
    TEXT_DESK_CALENDAR,
    TEXT_TODAY_AGENDA,
    TEXT_LANGUAGE,
    TEXT_APPEARANCE,
    TEXT_DISPLAY_MODE,
    TEXT_COLOR_THEME,
    TEXT_LIGHT,
    TEXT_DARK,
    TEXT_NETWORK,
    TEXT_SCAN,
    TEXT_SCANNING,
    TEXT_ON,
    TEXT_OFF,
    TEXT_CONNECT,
    TEXT_DISCONNECT,
    TEXT_PASSWORD,
    TEXT_NETWORK_NAME,
    TEXT_JOIN,
    TEXT_CANCEL,
    TEXT_ENTER_NETWORK_NAME,
    TEXT_ENTER_PASSWORD,
    TEXT_WIFI_CONNECTED,
    TEXT_WIFI_DISCONNECTED,
    TEXT_BLUETOOTH_ENABLED,
    TEXT_BLUETOOTH_DISABLED,
    TEXT_DEVICE_CONNECTED,
    TEXT_DEVICE_DISCONNECTED,
    TEXT_NO_RESULTS,
    TEXT_SIGNAL,
    TEXT_SECURED,
    TEXT_OPEN
} text_id_t;

static const accent_theme_t accent_themes[] = {
    {0x00B58F, 0xDDF8F1, 0xAEE3EA, 0xF03D9E},
    {0x6757E8, 0xE9E5FF, 0xC8D6FF, 0xF03D9E},
    {0xF5A616, 0xFFF0CE, 0xAEE3EA, 0xEC4C8B},
    {0xE7438B, 0xFCE3EF, 0xBCE8EB, 0xFF7A32}
};

static const uint32_t ink_colors[] = {
    0x18202A,
    0x1E73B7,
    0xD7433C,
    0x1C9B68
};

static lv_obj_t * app_root;
static lv_obj_t * content_view;
static lv_obj_t * header_clock_label;
static lv_obj_t * language_label;
static lv_obj_t * nav_buttons[3];
static lv_obj_t * note_canvas;
static lv_obj_t * toast_label;
static lv_timer_t * clock_timer;
static lv_timer_t * toast_timer;
static app_page_t current_page = PAGE_HOME;
static note_page_t note_pages[NOTE_PAGE_COUNT];
static uint8_t note_page_index;
static uint8_t note_color_index;
static int32_t brush_size = 3;
static bool eraser_enabled;
static bool note_stroke_active;
static bool note_tools_open = true;
static bool note_focus_mode;
static bool note_saved;
static bool note_uploaded;
static bool note_data_initialized;
static bool dark_mode;
static uint8_t theme_index = 1;
static uint8_t language_index;
static bool rebuild_pending;
static bool is_landscape;
static int32_t display_width;
static int32_t display_height;
static int32_t footer_height;
static int32_t note_canvas_width;
static int32_t note_canvas_height;
static uint8_t * note_canvas_buffer;
static size_t note_canvas_buffer_size;
static lv_point_t previous_note_point;
static uint32_t previous_note_tick;
static bool wifi_radio_enabled;
static bool network_bluetooth_tab;
static lv_obj_t * network_overlay;
static lv_obj_t * network_sheet;
static lv_obj_t * network_list;
static lv_obj_t * network_status_label;
static lv_obj_t * network_scan_button;
static lv_obj_t * network_scan_label;
static lv_obj_t * wifi_credentials_overlay;
static lv_obj_t * wifi_ssid_input;
static lv_obj_t * wifi_password_input;
static lv_obj_t * wifi_password_toggle;
static lv_obj_t * wifi_keyboard;
static bool wifi_password_required;
static char wifi_pending_ssid[64];
static void render_current_page(void);
static void render_note_page(void);
static void render_calendar_page(void);
static void render_home_page(void);
static void show_language_overlay(void);
static void show_appearance_overlay(void);
static void show_network_overlay(bool bluetooth_tab);
static void show_wifi_credentials(const char * ssid, bool password_required);
static void refresh_network_overlay(void);
static const char * tr(text_id_t id);
static void network_close_cb(lv_event_t * event);
static void network_tab_cb(lv_event_t * event);
static void network_toggle_cb(lv_event_t * event);
static void network_scan_cb(lv_event_t * event);
static void network_add_cb(lv_event_t * event);
static void network_row_cb(lv_event_t * event);
static void wifi_credentials_close_cb(lv_event_t * event);
static void wifi_credentials_input_cb(lv_event_t * event);
static void wifi_password_toggle_cb(lv_event_t * event);
static void wifi_credentials_join_cb(lv_event_t * event);
static void rebuild_ui_timer_cb(lv_timer_t * timer);

static lv_color_t color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static uint32_t theme_bg(void)
{
    return dark_mode ? 0x111318 : 0xF7F7FB;
}

static uint32_t theme_surface(void)
{
    return dark_mode ? 0x1D2128 : 0xFFFFFF;
}

static uint32_t theme_text(void)
{
    return dark_mode ? 0xF6F7F8 : 0x121318;
}

static uint32_t theme_muted(void)
{
    return dark_mode ? 0xA8AFBA : 0x777984;
}

static uint32_t theme_border(void)
{
    return dark_mode ? 0x363B44 : 0xE5E4EA;
}

static uint32_t theme_accent(void)
{
    return accent_themes[theme_index].accent;
}

static uint32_t theme_accent_soft(void)
{
    return dark_mode ? 0x343044 : accent_themes[theme_index].accent_soft;
}

static const char * tr(text_id_t id)
{
    static const char * const translations[5][TEXT_OPEN + 1] = {
        {
            "Notes", "Blank", "Ruled", "Grid", "Dots",
            "Saved locally", "Upload complete", "Connect Wi-Fi first",
            "Page cleared", "Nothing to save", "Nothing to upload", "This page is full",
            "Wi-Fi", "Bluetooth", "Tap to connect", "Tap to enable",
            "Latest note", "No saved page yet", "Saved locally", "Uploaded to server",
            "Unsaved changes", "Quick capture", "CAPTURE TODAY", "Keep the idea\nbefore it fades.",
            "Start writing", "Desk calendar", "Today's agenda", "Language", "Appearance",
            "Display mode", "Color theme", "Light", "Dark", "Network", "Scan", "Scanning",
            "On", "Off", "Connect", "Disconnect", "Password", "Network name", "Join", "Cancel",
            "Enter network name", "Enter password", "Wi-Fi connected", "Wi-Fi disconnected",
            "Bluetooth enabled", "Bluetooth disabled", "Device connected", "Device disconnected",
            "No results", "Signal", "Secured", "Open"
        },
        {
            "笔记", "空白", "横线", "方格", "点阵",
            "已保存到本地", "上传完成", "请先连接 Wi-Fi",
            "页面已清空", "没有可保存的内容", "没有可上传的内容", "本页已写满",
            "Wi-Fi", "蓝牙", "点击连接", "点击启用",
            "最近笔记", "还没有保存的页面", "已保存到本地", "已上传到服务器",
            "有未保存的修改", "快速记录", "记录今天", "把想法\n留在此刻",
            "开始书写", "桌面日历", "今日安排", "语言", "外观",
            "显示模式", "主题颜色", "亮色", "暗色", "网络", "搜索", "搜索中",
            "已开启", "已关闭", "连接", "断开", "密码", "网络名称", "加入", "取消",
            "输入网络名称", "输入密码", "Wi-Fi 已连接", "Wi-Fi 已断开",
            "蓝牙已开启", "蓝牙已关闭", "设备已连接", "设备已断开",
            "没有搜索结果", "信号", "已加密", "开放网络"
        },
        {
            "ノート", "白紙", "横線", "方眼", "ドット",
            "ローカル保存済み", "アップロード完了", "先に Wi-Fi に接続してください",
            "ページを消去しました", "保存する内容がありません", "アップロードする内容がありません", "このページは満杯です",
            "Wi-Fi", "Bluetooth", "タップして接続", "タップして有効化",
            "最近のノート", "保存されたページはありません", "ローカル保存済み", "サーバーにアップロード済み",
            "未保存の変更", "クイック記録", "今日を記録", "アイデアを\n今のうちに残す",
            "書き始める", "デスクカレンダー", "今日の予定", "言語", "外観",
            "表示モード", "カラーテーマ", "ライト", "ダーク", "ネットワーク", "検索", "検索中",
            "オン", "オフ", "接続", "切断", "パスワード", "ネットワーク名", "参加", "キャンセル",
            "ネットワーク名を入力", "パスワードを入力", "Wi-Fi に接続しました", "Wi-Fi を切断しました",
            "Bluetooth を有効化しました", "Bluetooth を無効化しました", "デバイスに接続しました", "デバイスを切断しました",
            "結果がありません", "信号", "保護あり", "オープン"
        },
        {
            "Notes", "Vide", "Lignes", "Grille", "Points",
            "Enregistre localement", "Téléversement terminé", "Connectez d'abord le Wi-Fi",
            "Page effacée", "Rien à enregistrer", "Rien à téléverser", "Page pleine",
            "Wi-Fi", "Bluetooth", "Appuyer pour connecter", "Appuyer pour activer",
            "Dernière note", "Aucune page enregistrée", "Enregistré localement", "Téléversé sur le serveur",
            "Modifications non enregistrées", "Capture rapide", "CAPTURER AUJOURD'HUI", "Gardez l'idée\navant qu'elle ne disparaisse",
            "Commencer à écrire", "Calendrier de bureau", "Agenda du jour", "Langue", "Apparence",
            "Mode d'affichage", "Thème de couleur", "Clair", "Sombre", "Réseau", "Rechercher", "Recherche",
            "Activé", "Désactivé", "Connecter", "Déconnecter", "Mot de passe", "Nom du réseau", "Rejoindre", "Annuler",
            "Saisir le nom du réseau", "Saisir le mot de passe", "Wi-Fi connecté", "Wi-Fi déconnecté",
            "Bluetooth activé", "Bluetooth désactivé", "Appareil connecté", "Appareil déconnecté",
            "Aucun résultat", "Signal", "Sécurisé", "Ouvert"
        },
        {
            "Заметки", "Пусто", "Линии", "Сетка", "Точки",
            "Сохранено локально", "Загрузка завершена", "Сначала подключите Wi-Fi",
            "Страница очищена", "Нечего сохранять", "Нечего загружать", "Страница заполнена",
            "Wi-Fi", "Bluetooth", "Нажмите для подключения", "Нажмите для включения",
            "Последняя заметка", "Сохранённых страниц нет", "Сохранено локально", "Загружено на сервер",
            "Есть несохранённые изменения", "Быстрая запись", "ЗАПИШИТЕ СЕГОДНЯ", "Сохраните идею\nпока она не исчезла",
            "Начать запись", "Настольный календарь", "План на сегодня", "Язык", "Внешний вид",
            "Режим отображения", "Цветовая тема", "Светлая", "Тёмная", "Сеть", "Поиск", "Поиск",
            "Включено", "Выключено", "Подключить", "Отключить", "Пароль", "Имя сети", "Войти", "Отмена",
            "Введите имя сети", "Введите пароль", "Wi-Fi подключён", "Wi-Fi отключён",
            "Bluetooth включён", "Bluetooth выключен", "Устройство подключено", "Устройство отключено",
            "Нет результатов", "Сигнал", "Защищено", "Открытая сеть"
        }
    };
    if((uint32_t)language_index >= 5U || (uint32_t)id > (uint32_t)TEXT_OPEN) {
        return translations[0][id];
    }
    return translations[language_index][id];
}

static void text_style(lv_obj_t * object, const lv_font_t * font, uint32_t rgb)
{
    if(language_index != 0U) {
        font = &waferlog_font_16;
    }
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(object, color(rgb), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(object, 0, LV_PART_MAIN);
}

static void flat_object(lv_obj_t * object)
{
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
}

static lv_obj_t * make_panel(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t background,
    int32_t radius
)
{
    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    return panel;
}

static lv_obj_t * make_button(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    const char * label,
    uint32_t background,
    uint32_t foreground,
    int32_t radius,
    const lv_font_t * font,
    lv_event_cb_t callback,
    uintptr_t user_data
)
{
    lv_obj_t * button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, radius, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    if(callback != NULL) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, (void *)user_data);
    }

    lv_obj_t * text = lv_label_create(button);
    lv_label_set_text(text, label);
    lv_obj_center(text);
    text_style(text, font, foreground);
    return button;
}

static void toast_hide_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(toast_label != NULL) {
        lv_obj_add_flag(toast_label, LV_OBJ_FLAG_HIDDEN);
    }
    toast_timer = NULL;
}

static void waferlog_move_foreground(lv_obj_t * object)
{
    if(object == NULL) {
        return;
    }
#ifdef WAFERLOG_T5AI
    lv_obj_t * parent = lv_obj_get_parent(object);
    if(parent != NULL) {
        lv_obj_move_to_index(object, lv_obj_get_child_count(parent) - 1);
    }
#else
    lv_obj_move_foreground(object);
#endif
}

static void show_toast(const char * message)
{
    if(toast_label == NULL) {
        return;
    }
    lv_label_set_text(toast_label, message);
    lv_obj_remove_flag(toast_label, LV_OBJ_FLAG_HIDDEN);
    waferlog_move_foreground(toast_label);
    if(toast_timer != NULL) {
        lv_timer_delete(toast_timer);
    }
    toast_timer = lv_timer_create(toast_hide_cb, 1800, NULL);
    lv_timer_set_repeat_count(toast_timer, 1);
}

static void update_clock(void)
{
    if(header_clock_label == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm local_time;
#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    char text[32];
    lv_snprintf(
        text,
        sizeof(text),
        "%02d:%02d | %04d-%02d-%02d",
        local_time.tm_hour,
        local_time.tm_min,
        local_time.tm_year + 1900,
        local_time.tm_mon + 1,
        local_time.tm_mday
    );
    lv_label_set_text(header_clock_label, text);
}

static void clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    update_clock();
    if(current_page == PAGE_CALENDAR) {
        render_calendar_page();
    }
}

static bool any_handwriting(void)
{
    for(uint32_t i = 0; i < NOTE_PAGE_COUNT; i++) {
        if(note_pages[i].stroke_count > 0) {
            return true;
        }
    }
    return false;
}

static void navigation_update(void)
{
    for(uint32_t i = 0; i < 3; i++) {
        if(nav_buttons[i] == NULL) {
            continue;
        }
        bool active = current_page == (app_page_t)i;
        lv_obj_set_style_bg_opa(
            nav_buttons[i],
            active ? LV_OPA_COVER : LV_OPA_TRANSP,
            LV_PART_MAIN
        );
        lv_obj_set_style_bg_color(
            nav_buttons[i],
            color(0xFFFFFF),
            LV_PART_MAIN
        );
        lv_obj_set_style_text_color(
            nav_buttons[i],
            color(active ? 0x111318 : 0x8E949D),
            LV_PART_MAIN
        );
        lv_obj_t * label = lv_obj_get_child(nav_buttons[i], 0);
        if(label != NULL) {
            lv_obj_set_style_text_color(
                label,
                color(active ? 0x111318 : 0x8E949D),
                LV_PART_MAIN
            );
        }
    }
}

static void switch_page(app_page_t page)
{
    current_page = page;
    note_focus_mode = false;
    render_current_page();
}

static void nav_clicked_cb(lv_event_t * event)
{
    switch_page((app_page_t)(uintptr_t)lv_event_get_user_data(event));
}

static void home_start_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    switch_page(PAGE_NOTES);
}

static void wifi_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    show_network_overlay(false);
}

static void bluetooth_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    show_network_overlay(true);
}

static void recording_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(waferlog_recording_is_active()) {
        waferlog_recording_stop();
        show_toast("Recording stopped");
    }
    else if(waferlog_recording_start()) {
        show_toast("Recording started");
    }
    render_home_page();
}

static void recent_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    switch_page(PAGE_NOTES);
}

static void network_close_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(network_overlay != NULL) {
        lv_obj_delete(network_overlay);
        network_overlay = NULL;
        network_sheet = NULL;
        network_list = NULL;
        network_status_label = NULL;
        network_scan_button = NULL;
        network_scan_label = NULL;
    }
}

static void network_tab_cb(lv_event_t * event)
{
    network_bluetooth_tab =
        (bool)(uintptr_t)lv_event_get_user_data(event);
    refresh_network_overlay();
}

static void network_toggle_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(network_bluetooth_tab) {
        if(waferlog_ble_is_enabled()) {
            waferlog_ble_disable();
            show_toast(tr(TEXT_BLUETOOTH_DISABLED));
        }
        else {
            waferlog_ble_enable();
            waferlog_ble_scan();
            show_toast(tr(TEXT_BLUETOOTH_ENABLED));
        }
    }
    else {
        wifi_radio_enabled = !wifi_radio_enabled;
        if(wifi_radio_enabled) {
            waferlog_wifi_scan();
        }
        else {
            waferlog_wifi_disconnect();
            show_toast(tr(TEXT_WIFI_DISCONNECTED));
        }
    }
    refresh_network_overlay();
    render_home_page();
}

static void network_scan_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(network_bluetooth_tab) {
        if(!waferlog_ble_is_enabled()) {
            waferlog_ble_enable();
        }
        waferlog_ble_scan();
    }
    else {
        if(wifi_radio_enabled) {
            waferlog_wifi_scan();
        }
    }
    refresh_network_overlay();
}

static void network_add_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    show_wifi_credentials("", false);
}

static void network_row_cb(lv_event_t * event)
{
    uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    if(network_bluetooth_tab) {
        if(!waferlog_ble_is_enabled()) {
            waferlog_ble_enable();
        }
        show_toast(tr(TEXT_DEVICE_CONNECTED));
        refresh_network_overlay();
        render_home_page();
        return;
    }

    const char * ssid = waferlog_wifi_scan_ssid(index);
    if(ssid == NULL) {
        return;
    }
    if(waferlog_wifi_scan_secured(index)) {
        show_wifi_credentials(ssid, true);
        return;
    }
    if(waferlog_wifi_connect(ssid, "")) {
        show_toast(tr(TEXT_WIFI_CONNECTED));
        network_close_cb(NULL);
        render_home_page();
    }
}

static void wifi_credentials_close_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(wifi_credentials_overlay != NULL) {
        lv_obj_delete(wifi_credentials_overlay);
        wifi_credentials_overlay = NULL;
    }
    wifi_ssid_input = NULL;
    wifi_password_input = NULL;
    wifi_password_toggle = NULL;
    wifi_keyboard = NULL;
}

static void wifi_credentials_input_cb(lv_event_t * event)
{
    lv_obj_t * target = lv_event_get_target_obj(event);
    if(wifi_keyboard != NULL) {
        lv_keyboard_set_textarea(wifi_keyboard, target);
        lv_obj_remove_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
        waferlog_move_foreground(wifi_keyboard);
    }
}

static void wifi_password_toggle_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(wifi_password_input == NULL) {
        return;
    }
    bool visible = lv_textarea_get_password_mode(wifi_password_input);
    lv_textarea_set_password_mode(wifi_password_input, !visible);
    if(wifi_password_toggle != NULL) {
        lv_label_set_text(
            lv_obj_get_child(wifi_password_toggle, 0),
            visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN
        );
    }
}

static void wifi_credentials_join_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(wifi_ssid_input == NULL || wifi_password_input == NULL) {
        return;
    }
    const char * ssid = lv_textarea_get_text(wifi_ssid_input);
    const char * password = lv_textarea_get_text(wifi_password_input);
    if(ssid == NULL || ssid[0] == '\0') {
        show_toast(tr(TEXT_ENTER_NETWORK_NAME));
        return;
    }
    if(wifi_password_required &&
       (password == NULL || password[0] == '\0')) {
        show_toast(tr(TEXT_ENTER_PASSWORD));
        return;
    }
    if(!waferlog_wifi_connect(ssid, password != NULL ? password : "")) {
        show_toast(tr(TEXT_CONNECT_WIFI_FIRST));
        return;
    }
    show_toast(tr(TEXT_WIFI_CONNECTED));
    wifi_credentials_close_cb(NULL);
    network_close_cb(NULL);
    render_home_page();
}

static void refresh_network_overlay(void)
{
    if(network_overlay == NULL || network_list == NULL) {
        return;
    }
    lv_obj_clean(network_list);
    if(network_status_label != NULL) {
        lv_label_set_text(
            network_status_label,
            network_bluetooth_tab
                ? (waferlog_ble_is_enabled() ? tr(TEXT_ON) : tr(TEXT_OFF))
                : (wifi_radio_enabled ? tr(TEXT_ON) : tr(TEXT_OFF))
        );
    }
    if(network_scan_label != NULL) {
        lv_label_set_text(network_scan_label, tr(TEXT_SCAN));
    }

    uint32_t count = network_bluetooth_tab
        ? waferlog_ble_scan_count()
        : waferlog_wifi_scan_count();
    if(count == 0U) {
        lv_obj_t * empty = lv_label_create(network_list);
        lv_label_set_text(empty, tr(TEXT_NO_RESULTS));
        lv_obj_set_pos(empty, 8, 8);
        text_style(empty, &lv_font_montserrat_14, theme_muted());
        return;
    }

    for(uint32_t i = 0; i < count; i++) {
        const char * name = network_bluetooth_tab
            ? waferlog_ble_scan_name(i)
            : waferlog_wifi_scan_ssid(i);
        int32_t signal = network_bluetooth_tab
            ? waferlog_ble_scan_signal(i)
            : waferlog_wifi_scan_signal(i);
        lv_obj_t * row = make_panel(
            network_list,
            0,
            (int32_t)i * 48,
            lv_obj_get_width(network_list),
            42,
            theme_surface(),
            8
        );
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, network_row_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        lv_obj_t * name_label = lv_label_create(row);
        lv_label_set_text(name_label, name != NULL ? name : "--");
        lv_obj_set_pos(name_label, 10, 5);
        text_style(name_label, &lv_font_montserrat_14, theme_text());

        char detail[64];
        lv_snprintf(
            detail,
            sizeof(detail),
            "%s %d%%  %s",
            tr(TEXT_SIGNAL),
            (int)signal,
            network_bluetooth_tab
                ? tr(TEXT_CONNECT)
                : (waferlog_wifi_scan_secured(i) ? tr(TEXT_SECURED) : tr(TEXT_OPEN))
        );
        lv_obj_t * detail_label = lv_label_create(row);
        lv_label_set_text(detail_label, detail);
        lv_obj_set_pos(detail_label, 10, 23);
        text_style(detail_label, &lv_font_montserrat_12, theme_muted());
    }
}

static void show_network_overlay(bool bluetooth_tab)
{
    if(network_overlay != NULL) {
        return;
    }
    network_bluetooth_tab = bluetooth_tab;
    if(network_bluetooth_tab) {
        if(!waferlog_ble_is_enabled()) {
            waferlog_ble_enable();
        }
        waferlog_ble_scan();
    }
    else {
        if(wifi_radio_enabled) {
            waferlog_wifi_scan();
        }
    }

    int32_t sheet_height = is_landscape ? 286 : 430;
    network_overlay = make_panel(
        app_root, 0, 0, display_width, display_height, 0x000000, 0
    );
    lv_obj_set_style_bg_opa(network_overlay, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(network_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_overlay, network_close_cb, LV_EVENT_CLICKED, NULL);
    waferlog_move_foreground(network_overlay);
    network_sheet = make_panel(
        network_overlay,
        is_landscape ? (display_width - 460) / 2 : 0,
        display_height - sheet_height,
        is_landscape ? 460 : display_width,
        sheet_height,
        theme_surface(),
        18
    );
    int32_t sheet_width = lv_obj_get_width(network_sheet);
    lv_obj_t * title = lv_label_create(network_sheet);
    lv_label_set_text(title, tr(TEXT_NETWORK));
    lv_obj_set_pos(title, 16, 14);
    text_style(title, &lv_font_montserrat_20, theme_text());
    make_button(
        network_sheet, sheet_width - 60, 6, 52, 44, "X",
        theme_surface(), theme_text(), 10, &lv_font_montserrat_20,
        network_close_cb, 0
    );
    make_button(
        network_sheet, 16, 54, (sheet_width - 42) / 2, 36, tr(TEXT_WIFI),
        !network_bluetooth_tab ? theme_accent_soft() : theme_bg(),
        !network_bluetooth_tab ? theme_accent() : theme_muted(),
        9, &lv_font_montserrat_14, network_tab_cb, 0
    );
    make_button(
        network_sheet, 26 + (sheet_width - 42) / 2, 54,
        (sheet_width - 42) / 2, 36, tr(TEXT_BLUETOOTH),
        network_bluetooth_tab ? theme_accent_soft() : theme_bg(),
        network_bluetooth_tab ? theme_accent() : theme_muted(),
        9, &lv_font_montserrat_14, network_tab_cb, 1
    );
    lv_obj_t * toggle = make_button(
        network_sheet, 16, 94, 92, 32,
        network_bluetooth_tab
            ? (waferlog_ble_is_enabled() ? tr(TEXT_ON) : tr(TEXT_OFF))
            : (wifi_radio_enabled ? tr(TEXT_ON) : tr(TEXT_OFF)),
        theme_accent_soft(), theme_accent(), 9, &lv_font_montserrat_12,
        network_toggle_cb, 0
    );
    network_status_label = lv_obj_get_child(toggle, 0);
    lv_obj_set_style_text_align(network_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    make_button(
        network_sheet, sheet_width - 112, 94, 96, 32, tr(TEXT_SCAN),
        theme_accent_soft(), theme_accent(), 9, &lv_font_montserrat_12,
        network_scan_cb, 0
    );
    network_scan_button = lv_obj_get_child(network_sheet, lv_obj_get_child_count(network_sheet) - 1);
    network_scan_label = lv_obj_get_child(network_scan_button, 0);
    if(!network_bluetooth_tab) {
        make_button(
            network_sheet, 16, 132, 120, 30, tr(TEXT_NETWORK_NAME),
            theme_bg(), theme_text(), 8, &lv_font_montserrat_12,
            network_add_cb, 0
        );
    }
    network_list = make_panel(
        network_sheet, 12, network_bluetooth_tab ? 132 : 168,
        sheet_width - 24, sheet_height - (network_bluetooth_tab ? 144 : 180),
        theme_bg(), 10
    );
    refresh_network_overlay();
}

static void show_wifi_credentials(const char * ssid, bool password_required)
{
    if(wifi_credentials_overlay != NULL) {
        return;
    }
    wifi_password_required = password_required;
    snprintf(
        wifi_pending_ssid,
        sizeof(wifi_pending_ssid),
        "%s",
        ssid != NULL ? ssid : ""
    );
    int32_t sheet_height = is_landscape ? 250 : 330;
    wifi_credentials_overlay = make_panel(
        app_root, 0, 0, display_width, display_height, 0x000000, 0
    );
    lv_obj_set_style_bg_opa(wifi_credentials_overlay, LV_OPA_40, LV_PART_MAIN);
    lv_obj_add_flag(wifi_credentials_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        wifi_credentials_overlay, wifi_credentials_close_cb, LV_EVENT_CLICKED, NULL
    );
    waferlog_move_foreground(wifi_credentials_overlay);
    lv_obj_t * sheet = make_panel(
        wifi_credentials_overlay,
        is_landscape ? (display_width - 460) / 2 : 0,
        display_height - sheet_height,
        is_landscape ? 460 : display_width,
        sheet_height,
        theme_surface(),
        18
    );
    int32_t sheet_width = lv_obj_get_width(sheet);
    lv_obj_t * title = lv_label_create(sheet);
    lv_label_set_text(title, tr(TEXT_WIFI));
    lv_obj_set_pos(title, 16, 14);
    text_style(title, &lv_font_montserrat_20, theme_text());
    make_button(
        sheet, sheet_width - 60, 6, 52, 44, "X",
        theme_surface(), theme_text(), 10, &lv_font_montserrat_20,
        wifi_credentials_close_cb, 0
    );
    wifi_ssid_input = lv_textarea_create(sheet);
    lv_obj_set_pos(wifi_ssid_input, 16, 58);
    lv_obj_set_size(wifi_ssid_input, sheet_width - 32, 42);
    lv_textarea_set_one_line(wifi_ssid_input, true);
    lv_textarea_set_placeholder_text(wifi_ssid_input, tr(TEXT_NETWORK_NAME));
    lv_textarea_set_text(wifi_ssid_input, wifi_pending_ssid);
    lv_obj_set_style_bg_color(wifi_ssid_input, color(theme_bg()), LV_PART_MAIN);
    lv_obj_set_style_text_color(wifi_ssid_input, color(theme_text()), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_ssid_input, &waferlog_font_16, LV_PART_MAIN);
    lv_obj_add_event_cb(wifi_ssid_input, wifi_credentials_input_cb,
                        LV_EVENT_FOCUSED, NULL);

    wifi_password_input = lv_textarea_create(sheet);
    lv_obj_set_pos(wifi_password_input, 16, 108);
    lv_obj_set_size(wifi_password_input, sheet_width - 80, 42);
    lv_textarea_set_one_line(wifi_password_input, true);
    lv_textarea_set_password_mode(wifi_password_input, true);
    lv_textarea_set_placeholder_text(wifi_password_input, tr(TEXT_PASSWORD));
    lv_obj_set_style_bg_color(wifi_password_input, color(theme_bg()), LV_PART_MAIN);
    lv_obj_set_style_text_color(wifi_password_input, color(theme_text()), LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_password_input, &waferlog_font_16, LV_PART_MAIN);
    lv_obj_add_event_cb(wifi_password_input, wifi_credentials_input_cb,
                        LV_EVENT_FOCUSED, NULL);
    wifi_password_toggle = make_button(
        sheet, sheet_width - 56, 108, 40, 42, LV_SYMBOL_EYE_OPEN,
        theme_bg(), theme_text(), 8, &lv_font_montserrat_16,
        wifi_password_toggle_cb, 0
    );
    make_button(
        sheet, 16, sheet_height - 58, (sheet_width - 42) / 2, 40,
        tr(TEXT_CANCEL), theme_bg(), theme_text(), 9,
        &lv_font_montserrat_14, wifi_credentials_close_cb, 0
    );
    make_button(
        sheet, 26 + (sheet_width - 42) / 2, sheet_height - 58,
        (sheet_width - 42) / 2, 40, tr(TEXT_JOIN),
        theme_accent(), 0xFFFFFF, 9, &lv_font_montserrat_14,
        wifi_credentials_join_cb, 0
    );
    wifi_keyboard = lv_keyboard_create(wifi_credentials_overlay);
    lv_obj_set_size(wifi_keyboard, display_width, 180);
    lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void note_set_pixel(int32_t x, int32_t y, lv_color_t pixel)
{
    if(note_canvas == NULL ||
       x < 0 || y < 0 ||
       x >= note_canvas_width || y >= note_canvas_height) {
        return;
    }
    lv_draw_buf_t * draw_buf = lv_canvas_get_draw_buf(note_canvas);
    uint8_t * data = draw_buf == NULL
        ? NULL
        : lv_draw_buf_goto_xy(draw_buf, (uint32_t)x, (uint32_t)y);
    if(data != NULL) {
        lv_color16_t * pixel16 = (lv_color16_t *)data;
        pixel16->red = pixel.red >> 3;
        pixel16->green = pixel.green >> 2;
        pixel16->blue = pixel.blue >> 3;
    }
}

static void note_draw_paper(void)
{
    if(note_canvas == NULL) {
        return;
    }

    lv_canvas_fill_bg(
        note_canvas,
        color(dark_mode ? 0x171B21 : 0xFFFFFF),
        LV_OPA_COVER
    );
    uint8_t paper = note_pages[note_page_index].paper;
    if(paper == PAPER_BLANK) {
        return;
    }

    lv_color_t paper_line = color(dark_mode ? 0x36404A : 0xDCE7ED);
    if(paper == PAPER_RULED || paper == PAPER_GRID) {
        for(int32_t y = 36; y < note_canvas_height; y += 36) {
            for(int32_t x = 0; x < note_canvas_width; x++) {
                note_set_pixel(x, y, paper_line);
            }
        }
    }
    if(paper == PAPER_GRID) {
        for(int32_t x = 36; x < note_canvas_width; x += 36) {
            for(int32_t y = 0; y < note_canvas_height; y++) {
                note_set_pixel(x, y, paper_line);
            }
        }
    }
    if(paper == PAPER_DOTS) {
        for(int32_t y = 24; y < note_canvas_height; y += 24) {
            for(int32_t x = 24; x < note_canvas_width; x += 24) {
                note_set_pixel(x, y, paper_line);
            }
        }
    }
}

static lv_color_t note_paper_pixel(int32_t x, int32_t y)
{
    uint8_t paper = note_pages[note_page_index].paper;
    bool line = false;
    if((paper == PAPER_RULED || paper == PAPER_GRID) && y >= 36 && y % 36 == 0) {
        line = true;
    }
    if(paper == PAPER_GRID && x >= 36 && x % 36 == 0) {
        line = true;
    }
    if(paper == PAPER_DOTS && x >= 24 && x % 24 == 0 &&
       y >= 24 && y % 24 == 0) {
        line = true;
    }
    return color(line ? (dark_mode ? 0x36404A : 0xDCE7ED)
                      : (dark_mode ? 0x171B21 : 0xFFFFFF));
}

static uint32_t note_stroke_color(const note_stroke_t * stroke)
{
    if(stroke->color_index == UINT8_MAX) {
        return dark_mode ? 0x171B21 : 0xFFFFFF;
    }
    if(dark_mode && stroke->color_index == 0) {
        return 0xE8EEF4;
    }
    return ink_colors[stroke->color_index % 4];
}

static bool note_build_upload_content(char * buffer, size_t capacity)
{
    if(buffer == NULL || capacity < 32U) {
        return false;
    }
    size_t used = 0U;
    int written = lv_snprintf(
        buffer + used,
        capacity - used,
        "{\"viewBox\":[%ld,%ld],\"paper\":%u,\"strokes\":[",
        (long)note_canvas_width,
        (long)note_canvas_height,
        (unsigned)note_pages[note_page_index].paper
    );
    if(written < 0 || (size_t)written >= capacity - used) {
        return false;
    }
    used += (size_t)written;
    const note_page_t * page = &note_pages[note_page_index];
    for(uint32_t i = 0U; i < page->stroke_count; i++) {
        const note_stroke_t * stroke = &page->strokes[i];
        uint32_t rgb = note_stroke_color(stroke);
        written = lv_snprintf(
            buffer + used,
            capacity - used,
            "%s{\"points\":[[%d,%d,0.5],[%d,%d,0.5]],\"color\":\"#%06lX\",\"size\":%u}",
            i == 0U ? "" : ",",
            (int)stroke->x1,
            (int)stroke->y1,
            (int)stroke->x2,
            (int)stroke->y2,
            (unsigned long)rgb,
            (unsigned)stroke->width
        );
        if(written < 0 || (size_t)written >= capacity - used) {
            return false;
        }
        used += (size_t)written;
    }
    written = lv_snprintf(buffer + used, capacity - used, "]}");
    return written >= 0 && (size_t)written < capacity - used;
}

static void note_draw_stroke_pixels(const note_stroke_t * stroke)
{
    if(note_canvas == NULL || stroke == NULL) {
        return;
    }

    const lv_color_t ink_pixel = color(note_stroke_color(stroke));
    int32_t x = stroke->x1;
    int32_t y = stroke->y1;
    int32_t dx = LV_ABS(stroke->x2 - stroke->x1);
    int32_t sx = stroke->x1 < stroke->x2 ? 1 : -1;
    int32_t dy = -LV_ABS(stroke->y2 - stroke->y1);
    int32_t sy = stroke->y1 < stroke->y2 ? 1 : -1;
    int32_t error = dx + dy;
    int32_t radius = LV_MAX(0, (int32_t)stroke->width / 2);

    while(true) {
        for(int32_t oy = -radius; oy <= radius; oy++) {
            for(int32_t ox = -radius; ox <= radius; ox++) {
                if(ox * ox + oy * oy <= radius * radius) {
                    note_set_pixel(
                        x + ox,
                        y + oy,
                        stroke->color_index == UINT8_MAX
                            ? note_paper_pixel(x + ox, y + oy)
                            : ink_pixel
                    );
                }
            }
        }
        if(x == stroke->x2 && y == stroke->y2) {
            break;
        }
        int32_t twice_error = 2 * error;
        if(twice_error >= dy) {
            error += dy;
            x += sx;
        }
        if(twice_error <= dx) {
            error += dx;
            y += sy;
        }
    }
}

static void note_flush_canvas(void)
{
#ifndef WAFERLOG_T5AI
    if(note_canvas != NULL) {
        lv_draw_buf_flush_cache(lv_canvas_get_draw_buf(note_canvas), NULL);
    }
#endif
    if(note_canvas != NULL) {
        lv_obj_invalidate(note_canvas);
    }
}

static void note_redraw_canvas(void)
{
    note_draw_paper();
    note_page_t * page = &note_pages[note_page_index];
    if(note_canvas == NULL) {
        return;
    }
    if(page->stroke_count > 0) {
        for(uint32_t i = 0; i < page->stroke_count; i++) {
            note_draw_stroke_pixels(&page->strokes[i]);
        }
    }
    note_flush_canvas();
    lv_obj_invalidate(note_canvas);
}

static void note_canvas_event_cb(lv_event_t * event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if(code != LV_EVENT_PRESSED &&
       code != LV_EVENT_PRESSING &&
       code != LV_EVENT_RELEASED &&
       code != LV_EVENT_PRESS_LOST) {
        return;
    }
    lv_event_stop_bubbling(event);

    if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        note_stroke_active = false;
        return;
    }

    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL || note_canvas == NULL) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t area;
    lv_obj_get_coords(note_canvas, &area);
    if(point.x < area.x1 || point.x > area.x2 ||
       point.y < area.y1 || point.y > area.y2) {
        note_stroke_active = false;
        return;
    }
    point.x -= area.x1;
    point.y -= area.y1;
    point.x = LV_CLAMP(0, point.x, note_canvas_width - 1);
    point.y = LV_CLAMP(0, point.y, note_canvas_height - 1);

    if(code == LV_EVENT_PRESSED) {
        previous_note_point = point;
        previous_note_tick = lv_tick_get();
        note_stroke_active = true;
        return;
    }
    if(!note_stroke_active) {
        return;
    }

    uint32_t elapsed = lv_tick_elaps(previous_note_tick);
    int32_t distance =
        LV_ABS(point.x - previous_note_point.x) +
        LV_ABS(point.y - previous_note_point.y);
    if(distance < NOTE_MIN_SAMPLE_DISTANCE) {
        return;
    }
    int32_t speed = elapsed > 0 ? distance * 10 / (int32_t)elapsed : distance * 10;
    int32_t stroke_width = brush_size;
    if(!eraser_enabled) {
        if(speed <= 2) stroke_width += 1;
        else if(speed >= 12) stroke_width -= 1;
    }
    stroke_width = LV_CLAMP(1, stroke_width, eraser_enabled ? 16 : 12);

    note_page_t * page = &note_pages[note_page_index];
    if(page->stroke_count >= NOTE_STROKE_LIMIT) {
        show_toast(tr(TEXT_PAGE_FULL));
        return;
    }

    note_stroke_t * stroke = &page->strokes[page->stroke_count++];
    stroke->x1 = (int16_t)previous_note_point.x;
    stroke->y1 = (int16_t)previous_note_point.y;
    stroke->x2 = (int16_t)point.x;
    stroke->y2 = (int16_t)point.y;
    stroke->width = (uint8_t)stroke_width;
    stroke->color_index = eraser_enabled ? UINT8_MAX : note_color_index;
    note_draw_stroke_pixels(stroke);
    note_flush_canvas();

    previous_note_point = point;
    previous_note_tick = lv_tick_get();
    note_saved = false;
    note_uploaded = false;
}

static void note_page_clicked_cb(lv_event_t * event)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    int next = (int)note_page_index + delta;
    if(next >= 0 && next < NOTE_PAGE_COUNT) {
        note_page_index = (uint8_t)next;
        render_note_page();
    }
}

static void note_tools_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    note_tools_open = !note_tools_open;
    render_note_page();
}

static void note_focus_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    note_focus_mode = !note_focus_mode;
    render_note_page();
}

static void note_save_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(!any_handwriting()) {
        show_toast(tr(TEXT_NOTHING_TO_SAVE));
        return;
    }
    note_saved = true;
    note_uploaded = false;
    show_toast(tr(TEXT_SAVED_LOCALLY));
    render_note_page();
}

static void note_upload_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(!any_handwriting()) {
        show_toast(tr(TEXT_NOTHING_TO_UPLOAD));
        return;
    }
    if(!waferlog_wifi_is_connected()) {
        show_toast(tr(TEXT_CONNECT_WIFI_FIRST));
        return;
    }
    char * upload_content = lv_malloc(128U * 1024U);
    if(upload_content == NULL || !note_build_upload_content(upload_content, 128U * 1024U)) {
        if(upload_content != NULL) {
            lv_free(upload_content);
        }
        show_toast(tr(TEXT_NOTHING_TO_UPLOAD));
        return;
    }
    char title[32];
    lv_snprintf(title, sizeof(title), "WaferLog Page %02u", (unsigned)note_page_index + 1U);
    bool uploaded = waferlog_note_upload_payload(title, upload_content, "");
    lv_free(upload_content);
    if(uploaded) {
        note_saved = true;
        note_uploaded = true;
        show_toast(tr(TEXT_UPLOAD_COMPLETE));
        render_note_page();
    }
}

static void note_paper_changed_cb(lv_event_t * event)
{
    note_pages[note_page_index].paper =
        (uint8_t)lv_dropdown_get_selected(lv_event_get_target_obj(event));
    note_saved = false;
    note_uploaded = false;
    note_redraw_canvas();
}

static void note_color_clicked_cb(lv_event_t * event)
{
    note_color_index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    eraser_enabled = false;
    render_note_page();
}

static void note_width_clicked_cb(lv_event_t * event)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    brush_size = LV_CLAMP(1, brush_size + delta, 12);
    render_note_page();
}

static void note_eraser_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    eraser_enabled = !eraser_enabled;
    render_note_page();
}

static void note_clear_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    note_pages[note_page_index].stroke_count = 0;
    note_saved = false;
    note_uploaded = false;
    note_redraw_canvas();
    show_toast(tr(TEXT_PAGE_CLEARED));
}

static void render_note_page(void)
{
    lv_obj_clean(content_view);
    note_canvas = NULL;
    note_stroke_active = false;
    navigation_update();

    int32_t content_height = lv_obj_get_height(content_view);
    int32_t primary_height = note_focus_mode ? 0 : 54;
    int32_t tools_height = note_focus_mode || !note_tools_open ? 0 : 44;

    if(!note_focus_mode) {
        lv_obj_t * primary = make_panel(
            content_view, 0, 0, display_width, primary_height, theme_bg(), 0
        );
        lv_obj_t * title = lv_label_create(primary);
        lv_label_set_text(title, tr(TEXT_NOTES));
        lv_obj_set_pos(title, 10, 17);
        text_style(title, &lv_font_montserrat_16, theme_text());

        int32_t left_x = display_width - 248;
        make_button(
            primary, left_x, 8, 34, 38, LV_SYMBOL_LEFT,
            theme_surface(), theme_text(), 10, &lv_font_montserrat_16,
            note_page_clicked_cb, (uintptr_t)-1
        );

        char page_text[16];
        lv_snprintf(
            page_text,
            sizeof(page_text),
            "%u / %u",
            note_page_index + 1U,
            NOTE_PAGE_COUNT
        );
        lv_obj_t * page_label = lv_label_create(primary);
        lv_label_set_text(page_label, page_text);
        lv_obj_set_pos(page_label, left_x + 36, 17);
        lv_obj_set_width(page_label, 42);
        lv_obj_set_style_text_align(page_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        text_style(page_label, &lv_font_montserrat_16, theme_text());

        make_button(
            primary, left_x + 78, 8, 34, 38, LV_SYMBOL_RIGHT,
            theme_surface(), theme_text(), 10, &lv_font_montserrat_16,
            note_page_clicked_cb, 1U
        );
        make_button(
            primary, display_width - 130, 8, 36, 38,
            note_tools_open ? LV_SYMBOL_CLOSE : LV_SYMBOL_LIST,
            theme_accent_soft(), theme_accent(), 10, &lv_font_montserrat_16,
            note_tools_clicked_cb, 0U
        );
        make_button(
            primary, display_width - 90, 8, 36, 38, LV_SYMBOL_SAVE,
            note_saved ? theme_accent_soft() : theme_surface(),
            note_saved ? theme_accent() : theme_text(),
            10, &lv_font_montserrat_16, note_save_clicked_cb, 0U
        );
        make_button(
            primary, display_width - 50, 8, 40, 38, LV_SYMBOL_UPLOAD,
            note_uploaded ? theme_accent_soft() : theme_surface(),
            note_uploaded ? theme_accent() : theme_text(),
            10, &lv_font_montserrat_16, note_upload_clicked_cb, 0U
        );
    }

    if(tools_height > 0) {
        lv_obj_t * tools = make_panel(
            content_view,
            0,
            primary_height,
            display_width,
            tools_height,
            theme_bg(),
            0
        );

        lv_obj_t * dropdown = lv_dropdown_create(tools);
        lv_obj_set_pos(dropdown, 4, 5);
        lv_obj_set_size(dropdown, 62, 34);
        char paper_options[96];
        lv_snprintf(
            paper_options,
            sizeof(paper_options),
            "%s\n%s\n%s\n%s",
            tr(TEXT_BLANK),
            tr(TEXT_RULED),
            tr(TEXT_GRID),
            tr(TEXT_DOTS)
        );
        lv_dropdown_set_options(dropdown, paper_options);
        lv_dropdown_set_selected(dropdown, note_pages[note_page_index].paper);
        lv_obj_set_style_bg_color(dropdown, color(theme_surface()), LV_PART_MAIN);
        lv_obj_set_style_text_color(dropdown, color(theme_text()), LV_PART_MAIN);
        lv_obj_set_style_text_font(dropdown, &waferlog_font_14, LV_PART_MAIN);
        lv_obj_set_style_text_font(dropdown, &waferlog_font_14, LV_PART_ITEMS);
        lv_obj_set_style_border_color(dropdown, color(theme_border()), LV_PART_MAIN);
        lv_obj_set_style_border_width(dropdown, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(dropdown, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(dropdown, 7, LV_PART_MAIN);
        lv_obj_add_event_cb(
            dropdown,
            note_paper_changed_cb,
            LV_EVENT_VALUE_CHANGED,
            NULL
        );

        for(uint32_t i = 0; i < 4; i++) {
            lv_obj_t * swatch = make_button(
                tools,
                70 + (int32_t)i * 23,
                9,
                20,
                24,
                "",
                dark_mode && i == 0 ? 0xE8EEF4 : ink_colors[i],
                0xFFFFFF,
                12,
                &lv_font_montserrat_12,
                note_color_clicked_cb,
                i
            );
            lv_obj_set_style_border_width(
                swatch,
                !eraser_enabled && note_color_index == i ? 3 : 1,
                LV_PART_MAIN
            );
            lv_obj_set_style_border_color(
                swatch,
                color(!eraser_enabled && note_color_index == i
                    ? theme_accent()
                    : theme_border()),
                LV_PART_MAIN
            );
        }

        make_button(
            tools, 166, 7, 28, 30, LV_SYMBOL_MINUS,
            theme_surface(), theme_text(), 9, &lv_font_montserrat_14,
            note_width_clicked_cb, (uintptr_t)-1
        );
        char size_text[8];
        lv_snprintf(size_text, sizeof(size_text), "%d", brush_size);
        lv_obj_t * size_label = lv_label_create(tools);
        lv_label_set_text(size_label, size_text);
        lv_obj_set_pos(size_label, 196, 14);
        lv_obj_set_width(size_label, 18);
        lv_obj_set_style_text_align(size_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        text_style(size_label, &lv_font_montserrat_14, theme_text());
        make_button(
            tools, 216, 7, 28, 30, LV_SYMBOL_PLUS,
            theme_surface(), theme_text(), 9, &lv_font_montserrat_14,
            note_width_clicked_cb, 1U
        );
        make_button(
            tools, 247, 7, 29, 30, LV_SYMBOL_BACKSPACE,
            eraser_enabled ? theme_accent_soft() : theme_surface(),
            eraser_enabled ? theme_accent() : theme_text(),
            9, &lv_font_montserrat_14, note_eraser_clicked_cb, 0U
        );
        make_button(
            tools, 279, 7, 36, 30, LV_SYMBOL_TRASH,
            theme_surface(), theme_text(), 9, &lv_font_montserrat_14,
            note_clear_clicked_cb, 0U
        );
    }

    int32_t canvas_y = primary_height + tools_height;
    note_canvas_width = display_width;
    note_canvas_height = content_height - canvas_y;
    note_canvas_width = LV_MIN(note_canvas_width, NOTE_CANVAS_MAX_WIDTH);
    note_canvas_height = LV_MIN(note_canvas_height, NOTE_CANVAS_MAX_HEIGHT);

    size_t canvas_buffer_size = LV_CANVAS_BUF_SIZE(
        note_canvas_width,
        note_canvas_height,
        16,
        LV_DRAW_BUF_STRIDE_ALIGN
    );
    if(note_canvas_buffer_size < canvas_buffer_size) {
        if(note_canvas_buffer != NULL) {
            lv_free(note_canvas_buffer);
            note_canvas_buffer = NULL;
            note_canvas_buffer_size = 0;
        }
        note_canvas_buffer = lv_malloc(canvas_buffer_size);
        if(note_canvas_buffer == NULL) {
            return;
        }
        note_canvas_buffer_size = canvas_buffer_size;
    }

    note_canvas = lv_canvas_create(content_view);
    lv_canvas_set_buffer(
        note_canvas,
        note_canvas_buffer,
        note_canvas_width,
        note_canvas_height,
        LV_COLOR_FORMAT_RGB565
    );
    lv_obj_set_pos(note_canvas, 0, canvas_y);
    lv_obj_set_style_border_width(note_canvas, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        note_canvas,
        color(dark_mode ? 0x46515C : 0xD9E2E7),
        LV_PART_MAIN
    );
    lv_obj_add_flag(note_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(note_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(note_canvas, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_scrollbar_mode(note_canvas, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(note_canvas, note_canvas_event_cb, LV_EVENT_ALL, NULL);
    note_redraw_canvas();

    make_button(
        content_view,
        display_width - 44,
        canvas_y + 8,
        36,
        36,
        note_focus_mode ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE,
        theme_surface(),
        theme_text(),
        10,
        &lv_font_montserrat_16,
        note_focus_clicked_cb,
        0U
    );
}

static void make_home_geometry(lv_obj_t * hero, int32_t hero_width)
{
    lv_obj_t * shape = make_panel(
        hero,
        hero_width - 120,
        30,
        54,
        82,
        dark_mode ? 0xBA4C68 : 0xFF647F,
        22
    );
    lv_obj_set_style_transform_rotation(shape, 120, LV_PART_MAIN);

    shape = make_panel(
        hero,
        hero_width - 70,
        17,
        48,
        76,
        dark_mode ? 0x19748B : 0x42C0DE,
        23
    );
    lv_obj_set_style_transform_rotation(shape, -100, LV_PART_MAIN);

    make_panel(
        hero,
        hero_width - 59,
        91,
        40,
        40,
        dark_mode ? 0xB42E62 : 0xF34483,
        20
    );

    lv_obj_t * record = make_button(
        hero,
        hero_width - 116,
        21,
        32,
        32,
        "REC",
        0xF2C84B,
        0x5C521E,
        11,
        &lv_font_montserrat_12,
        recording_clicked_cb,
        0U
    );
    waferlog_move_foreground(record);
}

static lv_obj_t * make_home_tile(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t background,
    const char * symbol,
    uint32_t symbol_color,
    const char * title,
    const char * detail,
    lv_event_cb_t callback
)
{
    lv_obj_t * tile = make_panel(parent, x, y, width, height, background, 14);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, callback, LV_EVENT_CLICKED, NULL);

    lv_obj_t * icon = lv_label_create(tile);
    lv_label_set_text(icon, symbol);
    lv_obj_set_pos(icon, 16, 16);
    text_style(icon, &lv_font_montserrat_24, symbol_color);

    lv_obj_t * title_label = lv_label_create(tile);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 16, height - 59);
    text_style(title_label, &lv_font_montserrat_20, theme_text());

    lv_obj_t * detail_label = lv_label_create(tile);
    lv_label_set_text(detail_label, detail);
    lv_obj_set_pos(detail_label, 16, height - 29);
    text_style(detail_label, &lv_font_montserrat_12, theme_muted());
    return tile;
}

static void render_home_portrait(void)
{
    int32_t margin = 16;
    int32_t content_width = display_width - margin * 2;

    lv_obj_t * hero = make_panel(
        content_view,
        margin,
        10,
        content_width,
        142,
        dark_mode ? 0x44358F : 0xD8CEFF,
        14
    );
    lv_obj_set_style_bg_grad_color(
        hero,
        color(dark_mode ? 0x105665 : 0xC5F0F5),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_add_flag(hero, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hero, home_start_clicked_cb, LV_EVENT_CLICKED, NULL);

    uint32_t hero_text = dark_mode ? 0xFFFFFF : 0x121318;
    lv_obj_t * kicker = lv_label_create(hero);
    lv_label_set_text(kicker, tr(TEXT_CAPTURE_TODAY));
    lv_obj_set_pos(kicker, 14, 13);
    text_style(kicker, &lv_font_montserrat_14, hero_text);

    lv_obj_t * hero_title = lv_label_create(hero);
    lv_label_set_text(hero_title, tr(TEXT_KEEP_IDEA));
    lv_obj_set_pos(hero_title, 14, 34);
    text_style(hero_title, &lv_font_montserrat_24, hero_text);

    make_button(
        hero,
        14,
        91,
        118,
        34,
        tr(TEXT_START_WRITING),
        0x121318,
        0xFFFFFF,
        10,
        &lv_font_montserrat_14,
        home_start_clicked_cb,
        0U
    );

    char page_text[16];
    lv_snprintf(page_text, sizeof(page_text), "Page %02u", note_page_index + 1U);
    lv_obj_t * page_label = lv_label_create(hero);
    lv_label_set_text(page_label, page_text);
    lv_obj_set_pos(page_label, 14, 124);
    text_style(page_label, &lv_font_montserrat_12, hero_text);
    make_home_geometry(hero, content_width);

    lv_obj_t * quick_title = lv_label_create(content_view);
    lv_label_set_text(quick_title, tr(TEXT_QUICK_CAPTURE));
    lv_obj_set_pos(quick_title, margin, 166);
    text_style(quick_title, &lv_font_montserrat_24, theme_text());

    lv_obj_t * upload_icon = lv_label_create(content_view);
    lv_label_set_text(upload_icon, LV_SYMBOL_UPLOAD);
    lv_obj_set_pos(upload_icon, display_width - 35, 174);
    text_style(upload_icon, &lv_font_montserrat_14, theme_muted());

    int32_t wifi_width = (content_width * 3) / 5;
    int32_t ble_width = content_width - wifi_width - 10;
    make_home_tile(
        content_view,
        margin,
        202,
        wifi_width,
        132,
        dark_mode ? 0x24616A : 0xAEE3EA,
        LV_SYMBOL_WIFI,
        theme_text(),
        tr(TEXT_WIFI),
        waferlog_wifi_is_connected() ? "WaferLog Lab" : tr(TEXT_TAP_TO_CONNECT),
        wifi_clicked_cb
    );
    make_home_tile(
        content_view,
        margin + wifi_width + 10,
        202,
        ble_width,
        132,
        theme_surface(),
        LV_SYMBOL_BLUETOOTH,
        accent_themes[theme_index].media,
        tr(TEXT_BLUETOOTH),
        waferlog_ble_is_enabled() ? "WaferLog T5AI" : tr(TEXT_TAP_TO_ENABLE),
        bluetooth_clicked_cb
    );

    lv_obj_t * recent = make_panel(
        content_view,
        margin,
        344,
        content_width,
        68,
        theme_surface(),
        12
    );
    lv_obj_add_flag(recent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(recent, recent_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * file_icon = make_panel(
        recent, 12, 12, 44, 44, accent_themes[theme_index].media, 11
    );
    lv_obj_set_style_bg_grad_color(file_icon, color(0xFF7A32), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(file_icon, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_t * file_label = lv_label_create(file_icon);
    lv_label_set_text(file_label, LV_SYMBOL_FILE);
    lv_obj_center(file_label);
    text_style(file_label, &lv_font_montserrat_16, 0xFFFFFF);

    lv_obj_t * recent_title = lv_label_create(recent);
    lv_label_set_text(recent_title, tr(TEXT_LATEST_NOTE));
    lv_obj_set_pos(recent_title, 66, 13);
    text_style(recent_title, &lv_font_montserrat_16, theme_text());

    lv_obj_t * recent_detail = lv_label_create(recent);
    lv_label_set_text(
        recent_detail,
        any_handwriting()
            ? (note_uploaded ? tr(TEXT_UPLOADED_STATE) :
               (note_saved ? tr(TEXT_SAVED_STATE) : tr(TEXT_UNSAVED_STATE)))
            : tr(TEXT_NO_SAVED_PAGE)
    );
    lv_obj_set_pos(recent_detail, 66, 36);
    text_style(recent_detail, &lv_font_montserrat_12, theme_muted());

    lv_obj_t * recent_upload = lv_label_create(recent);
    lv_label_set_text(recent_upload, LV_SYMBOL_UPLOAD);
    lv_obj_set_pos(recent_upload, content_width - 31, 25);
    text_style(recent_upload, &lv_font_montserrat_16, theme_muted());

    lv_obj_t * status = make_panel(
        content_view,
        margin,
        422,
        content_width,
        54,
        theme_surface(),
        12
    );
    char wifi_text[32];
    lv_snprintf(
        wifi_text,
        sizeof(wifi_text),
        LV_SYMBOL_WIFI " %s   BT %s",
        waferlog_wifi_is_connected() ? LV_SYMBOL_OK : "-",
        waferlog_ble_is_enabled() ? LV_SYMBOL_OK : "-"
    );
    lv_obj_t * wifi_state = lv_label_create(status);
    lv_label_set_text(wifi_state, wifi_text);
    lv_obj_set_pos(wifi_state, 20, 19);
    text_style(wifi_state, &lv_font_montserrat_14, theme_muted());

    lv_obj_t * lan_state = lv_label_create(status);
    lv_label_set_text(
        lan_state,
        note_uploaded ? LV_SYMBOL_UPLOAD " Cloud" : LV_SYMBOL_UPLOAD " LAN"
    );
    lv_obj_set_pos(lan_state, content_width / 2 - 17, 19);
    text_style(lan_state, &lv_font_montserrat_14, theme_muted());

    lv_obj_t * battery = lv_label_create(status);
    lv_label_set_text(battery, LV_SYMBOL_BATTERY_FULL " 99%");
    lv_obj_set_pos(battery, content_width - 90, 19);
    text_style(battery, &lv_font_montserrat_14, theme_muted());
}

static void render_home_landscape(void)
{
    int32_t height = lv_obj_get_height(content_view);
    lv_obj_t * hero = make_panel(
        content_view,
        12,
        10,
        270,
        height - 20,
        dark_mode ? 0x44358F : 0xD8CEFF,
        14
    );
    lv_obj_set_style_bg_grad_color(
        hero,
        color(dark_mode ? 0x105665 : 0xC5F0F5),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_add_flag(hero, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hero, home_start_clicked_cb, LV_EVENT_CLICKED, NULL);

    uint32_t hero_text = dark_mode ? 0xFFFFFF : 0x121318;
    lv_obj_t * kicker = lv_label_create(hero);
    lv_label_set_text(kicker, tr(TEXT_CAPTURE_TODAY));
    lv_obj_set_pos(kicker, 16, 16);
    text_style(kicker, &lv_font_montserrat_14, hero_text);
    lv_obj_t * title = lv_label_create(hero);
    lv_label_set_text(title, tr(TEXT_KEEP_IDEA));
    lv_obj_set_pos(title, 16, 43);
    text_style(title, &lv_font_montserrat_24, hero_text);
    make_button(
        hero,
        16,
        112,
        118,
        36,
        tr(TEXT_START_WRITING),
        0x121318,
        0xFFFFFF,
        10,
        &lv_font_montserrat_14,
        home_start_clicked_cb,
        0U
    );
    make_home_geometry(hero, 270);

    int32_t right_x = 294;
    int32_t right_width = display_width - right_x - 12;
    make_home_tile(
        content_view,
        right_x,
        10,
        (right_width - 10) / 2,
        120,
        dark_mode ? 0x24616A : 0xAEE3EA,
        LV_SYMBOL_WIFI,
        theme_text(),
        tr(TEXT_WIFI),
        waferlog_wifi_is_connected() ? "WaferLog Lab" : tr(TEXT_TAP_TO_CONNECT),
        wifi_clicked_cb
    );
    make_home_tile(
        content_view,
        right_x + (right_width - 10) / 2 + 10,
        10,
        (right_width - 10) / 2,
        120,
        theme_surface(),
        LV_SYMBOL_BLUETOOTH,
        accent_themes[theme_index].media,
        tr(TEXT_BLUETOOTH),
        waferlog_ble_is_enabled() ? "WaferLog T5AI" : tr(TEXT_TAP_TO_ENABLE),
        bluetooth_clicked_cb
    );

    lv_obj_t * recent = make_panel(
        content_view,
        right_x,
        140,
        right_width,
        height - 150,
        theme_surface(),
        12
    );
    lv_obj_add_flag(recent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(recent, recent_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * title_label = lv_label_create(recent);
    lv_label_set_text(title_label, tr(TEXT_LATEST_NOTE));
    lv_obj_set_pos(title_label, 16, 14);
    text_style(title_label, &lv_font_montserrat_16, theme_text());
    lv_obj_t * detail = lv_label_create(recent);
    lv_label_set_text(
        detail,
        any_handwriting() ? tr(TEXT_NOTES) : tr(TEXT_NO_SAVED_PAGE)
    );
    lv_obj_set_pos(detail, 16, 42);
    text_style(detail, &lv_font_montserrat_12, theme_muted());
}

static void render_home_page(void)
{
    lv_obj_clean(content_view);
    note_canvas = NULL;
    navigation_update();
    if(is_landscape) {
        render_home_landscape();
    }
    else {
        render_home_portrait();
    }
}

static void render_calendar_page(void)
{
    lv_obj_clean(content_view);
    note_canvas = NULL;
    navigation_update();

    time_t now = time(NULL);
    struct tm local_time;
#ifdef _WIN32
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif

    int32_t content_height = lv_obj_get_height(content_view);
    int32_t margin = 12;
    int32_t hero_width = is_landscape ? 300 : display_width - margin * 2;
    int32_t hero_height = is_landscape ? content_height - margin * 2 : 142;
    lv_obj_t * hero = make_panel(
        content_view,
        margin,
        margin,
        hero_width,
        hero_height,
        dark_mode ? 0x44358F : 0xA993FF,
        14
    );
    lv_obj_set_style_bg_grad_color(
        hero,
        color(dark_mode ? 0x105665 : 0x24D9F4),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_HOR, LV_PART_MAIN);

    lv_obj_t * desk = lv_label_create(hero);
    lv_label_set_text(desk, tr(TEXT_DESK_CALENDAR));
    lv_obj_set_pos(desk, 14, 14);
    text_style(desk, &lv_font_montserrat_16, theme_text());

    char clock_text[8];
    lv_snprintf(
        clock_text,
        sizeof(clock_text),
        "%02d:%02d",
        local_time.tm_hour,
        local_time.tm_min
    );
    lv_obj_t * clock = lv_label_create(hero);
    lv_label_set_text(clock, clock_text);
    lv_obj_set_pos(clock, 14, 38);
    text_style(clock, &lv_font_montserrat_32, theme_text());

    char date_text[16];
    lv_snprintf(
        date_text,
        sizeof(date_text),
        "%04d.%02d.%02d",
        local_time.tm_year + 1900,
        local_time.tm_mon + 1,
        local_time.tm_mday
    );
    lv_obj_t * date = lv_label_create(hero);
    lv_label_set_text(date, date_text);
    lv_obj_set_pos(date, 14, 78);
    text_style(date, &lv_font_montserrat_16, theme_text());

    lv_obj_t * weather = lv_label_create(hero);
    lv_label_set_text(weather, "22 C\nClear");
    lv_obj_set_pos(weather, 14, 102);
    text_style(weather, &lv_font_montserrat_16, theme_text());

    lv_obj_t * agenda_title = lv_label_create(hero);
    lv_label_set_text(agenda_title, tr(TEXT_TODAY_AGENDA));
    lv_obj_set_pos(agenda_title, is_landscape ? 158 : 176, 14);
    text_style(agenda_title, &lv_font_montserrat_14, theme_text());

    lv_obj_t * agenda = lv_label_create(hero);
    lv_label_set_text(agenda, LV_SYMBOL_OK " Review notes\n\n  Upload recap\n\n  Call at 16:00");
    lv_obj_set_pos(agenda, is_landscape ? 158 : 176, 38);
    lv_obj_set_width(agenda, hero_width - (is_landscape ? 168 : 186));
    lv_label_set_long_mode(agenda, LV_LABEL_LONG_WRAP);
    text_style(agenda, &lv_font_montserrat_14, theme_text());

    int32_t calendar_x = is_landscape ? hero_width + margin * 2 : margin;
    int32_t calendar_y = is_landscape ? margin : hero_height + margin * 2;
    int32_t calendar_width =
        is_landscape ? display_width - calendar_x - margin : display_width - margin * 2;
    int32_t calendar_height =
        is_landscape ? content_height - margin * 2 : content_height - calendar_y - margin;

    lv_obj_t * calendar = lv_calendar_create(content_view);
    lv_obj_set_pos(calendar, calendar_x, calendar_y);
    lv_obj_set_size(calendar, calendar_width, calendar_height);
#if LVGL_VERSION_MAJOR >= 9
    lv_calendar_set_today_date(
        calendar,
        local_time.tm_year + 1900,
        local_time.tm_mon + 1,
        local_time.tm_mday
    );
    lv_calendar_set_showed_date(
        calendar,
        local_time.tm_year + 1900,
        local_time.tm_mon + 1
    );
    lv_calendar_header_arrow_create(calendar);
#else
    lv_calendar_set_today_year(calendar, local_time.tm_year + 1900);
    lv_calendar_set_today_month(calendar, local_time.tm_mon + 1);
    lv_calendar_set_today_day(calendar, local_time.tm_mday);
    lv_calendar_set_shown_year(calendar, local_time.tm_year + 1900);
    lv_calendar_set_shown_month(calendar, local_time.tm_mon + 1);
    lv_calendar_add_header_arrow(calendar);
#endif
    lv_obj_set_style_bg_color(calendar, color(theme_surface()), LV_PART_MAIN);
    lv_obj_set_style_text_color(calendar, color(theme_text()), LV_PART_MAIN);
    lv_obj_set_style_text_font(calendar, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_border_color(calendar, color(theme_border()), LV_PART_MAIN);
    lv_obj_set_style_border_width(calendar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(calendar, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(calendar, color(theme_accent()), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(calendar, color(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
}

static void render_current_page(void)
{
    if(current_page == PAGE_NOTES) {
        render_note_page();
    }
    else if(current_page == PAGE_CALENDAR) {
        render_calendar_page();
    }
    else {
        render_home_page();
    }
}

static void rebuild_ui_timer_cb(lv_timer_t * timer)
{
    lv_timer_delete(timer);
    rebuild_pending = false;
    if(clock_timer != NULL) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer != NULL) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    if(app_root != NULL) {
        lv_obj_delete(app_root);
        app_root = NULL;
    }
    content_view = NULL;
    header_clock_label = NULL;
    language_label = NULL;
    note_canvas = NULL;
    toast_label = NULL;
    network_overlay = NULL;
    network_sheet = NULL;
    network_list = NULL;
    network_status_label = NULL;
    network_scan_button = NULL;
    network_scan_label = NULL;
    wifi_credentials_overlay = NULL;
    wifi_ssid_input = NULL;
    wifi_password_input = NULL;
    wifi_password_toggle = NULL;
    wifi_keyboard = NULL;
    for(uint32_t i = 0; i < 3; i++) {
        nav_buttons[i] = NULL;
    }
    waferlog_ui_create();
}

static void request_rebuild(void)
{
    if(rebuild_pending) {
        return;
    }
    rebuild_pending = true;
    lv_timer_create(rebuild_ui_timer_cb, 20, NULL);
}

static void rotate_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    lv_display_t * display = lv_display_get_default();
    bool next_landscape = !is_landscape;
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_0);
    lv_display_set_resolution(
        display,
        next_landscape ? 640 : 360,
        next_landscape ? 360 : 640
    );
    if(next_landscape) {
        current_page = PAGE_CALENDAR;
    }
    request_rebuild();
}

static void language_open_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    show_language_overlay();
}

static void appearance_open_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    show_appearance_overlay();
}

static void overlay_close_clicked_cb(lv_event_t * event)
{
    lv_obj_t * overlay = lv_event_get_user_data(event);
    if(overlay != NULL) {
        lv_obj_delete(overlay);
    }
}

static void language_selected_cb(lv_event_t * event)
{
    language_index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    lv_obj_t * overlay = lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target_obj(event)));
    if(overlay != NULL) {
        lv_obj_delete(overlay);
    }
    language_label = NULL;
    request_rebuild();
}

static lv_obj_t * create_overlay(int32_t sheet_height, const char * title)
{
    lv_obj_t * overlay = make_panel(
        app_root,
        0,
        0,
        display_width,
        display_height,
        0x000000,
        0
    );
    lv_obj_set_style_bg_opa(overlay, LV_OPA_40, LV_PART_MAIN);
    waferlog_move_foreground(overlay);

    int32_t sheet_width = is_landscape ? LV_MIN(460, display_width) : display_width;
    lv_obj_t * sheet = make_panel(
        overlay,
        (display_width - sheet_width) / 2,
        display_height - sheet_height,
        sheet_width,
        sheet_height,
        theme_surface(),
        18
    );
    lv_obj_set_style_radius(sheet, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(sheet, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(sheet, color(theme_border()), LV_PART_MAIN);

    lv_obj_t * title_label = lv_label_create(sheet);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 16, 18);
    text_style(title_label, &lv_font_montserrat_24, theme_text());

    make_button(
        sheet,
        sheet_width - 52,
        12,
        40,
        40,
        LV_SYMBOL_CLOSE,
        theme_surface(),
        theme_text(),
        12,
        &lv_font_montserrat_16,
        overlay_close_clicked_cb,
        (uintptr_t)overlay
    );
    return overlay;
}

static void show_language_overlay(void)
{
    int32_t height = is_landscape ? 300 : 306;
    lv_obj_t * overlay = create_overlay(height, tr(TEXT_LANGUAGE));
    lv_obj_t * sheet = lv_obj_get_child(overlay, 0);
    int32_t sheet_width = is_landscape ? LV_MIN(460, display_width) : display_width;
    static const char * names[] = {
        "English",
        "中文",
        "日本語",
        "Français",
        "Русский"
    };

    for(uint32_t i = 0; i < 5; i++) {
        lv_obj_t * button = make_button(
            sheet,
            14,
            64 + (int32_t)i * 46,
            sheet_width - 28,
            42,
            names[i],
            i == language_index ? theme_accent_soft() : theme_surface(),
            i == language_index ? theme_accent() : theme_text(),
            10,
            &waferlog_font_16,
            language_selected_cb,
            i
        );
        lv_obj_set_style_text_align(button, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }
}

static void appearance_mode_selected_cb(lv_event_t * event)
{
    dark_mode = (bool)(uintptr_t)lv_event_get_user_data(event);
    request_rebuild();
}

static void appearance_color_selected_cb(lv_event_t * event)
{
    theme_index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    request_rebuild();
}

static void show_appearance_overlay(void)
{
    int32_t height = is_landscape ? 270 : 266;
    lv_obj_t * overlay = create_overlay(height, tr(TEXT_APPEARANCE));
    lv_obj_t * sheet = lv_obj_get_child(overlay, 0);
    int32_t sheet_width = is_landscape ? LV_MIN(460, display_width) : display_width;

    lv_obj_t * mode_label = lv_label_create(sheet);
    lv_label_set_text(mode_label, tr(TEXT_DISPLAY_MODE));
    lv_obj_set_pos(mode_label, 16, 62);
    text_style(mode_label, &waferlog_font_14, theme_muted());

    char light_text[64];
    char dark_text[64];
    lv_snprintf(light_text, sizeof(light_text), LV_SYMBOL_EYE_OPEN "  %s", tr(TEXT_LIGHT));
    lv_snprintf(dark_text, sizeof(dark_text), LV_SYMBOL_EYE_CLOSE "  %s", tr(TEXT_DARK));
    make_button(
        sheet,
        16,
        90,
        (sheet_width - 44) / 2,
        46,
        light_text,
        !dark_mode ? theme_accent_soft() : theme_surface(),
        !dark_mode ? theme_accent() : theme_text(),
        10,
        &waferlog_font_16,
        appearance_mode_selected_cb,
        0U
    );
    make_button(
        sheet,
        28 + (sheet_width - 44) / 2,
        90,
        (sheet_width - 44) / 2,
        46,
        dark_text,
        dark_mode ? theme_accent_soft() : theme_surface(),
        dark_mode ? theme_accent() : theme_text(),
        10,
        &waferlog_font_16,
        appearance_mode_selected_cb,
        1U
    );

    lv_obj_t * color_label = lv_label_create(sheet);
    lv_label_set_text(color_label, tr(TEXT_COLOR_THEME));
    lv_obj_set_pos(color_label, 16, 150);
    text_style(color_label, &waferlog_font_14, theme_muted());

    int32_t total_width = 4 * 46 + 3 * 18;
    int32_t start_x = (sheet_width - total_width) / 2;
    for(uint32_t i = 0; i < 4; i++) {
        lv_obj_t * swatch = make_button(
            sheet,
            start_x + (int32_t)i * 64,
            182,
            46,
            46,
            "",
            accent_themes[i].accent,
            0xFFFFFF,
            12,
            &lv_font_montserrat_14,
            appearance_color_selected_cb,
            i
        );
        lv_obj_set_style_border_width(swatch, i == theme_index ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(swatch, color(theme_text()), LV_PART_MAIN);
    }
}

static void create_header(void)
{
    lv_obj_t * header = make_panel(
        app_root, 0, 0, display_width, HEADER_HEIGHT, theme_bg(), 0
    );

    lv_obj_t * logo = make_panel(header, 12, 9, 40, 40, 0xFF4F98, 20);
    lv_obj_set_style_bg_grad_color(logo, color(0xFF8A3D), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(logo, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_t * logo_text = lv_label_create(logo);
    lv_label_set_text(logo_text, "硅");
    lv_obj_center(logo_text);
    text_style(logo_text, &waferlog_font_16, 0x121318);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "硅笺 | WaferLog");
    lv_obj_set_pos(title, 60, 8);
    text_style(title, &waferlog_font_16, theme_text());

    header_clock_label = lv_label_create(header);
    lv_obj_set_pos(header_clock_label, 60, 31);
    text_style(header_clock_label, &lv_font_montserrat_12, theme_muted());
    update_clock();

    static const char * language_codes[] = {"EN", "中", "日", "FR", "RU"};
    lv_obj_t * language = make_button(
        header,
        display_width - 132,
        10,
        36,
        38,
        language_codes[language_index],
        theme_accent_soft(),
        theme_accent(),
        10,
        &waferlog_font_14,
        language_open_clicked_cb,
        0U
    );
    language_label = lv_obj_get_child(language, 0);

    make_button(
        header,
        display_width - 90,
        10,
        36,
        38,
        LV_SYMBOL_TINT,
        theme_surface(),
        theme_text(),
        10,
        &lv_font_montserrat_16,
        appearance_open_clicked_cb,
        0U
    );
    make_button(
        header,
        display_width - 48,
        10,
        36,
        38,
        LV_SYMBOL_REFRESH,
        theme_surface(),
        theme_text(),
        10,
        &lv_font_montserrat_16,
        rotate_clicked_cb,
        0U
    );
}

static void create_footer(void)
{
    lv_obj_t * footer = make_panel(
        app_root,
        0,
        display_height - footer_height,
        display_width,
        footer_height,
        theme_bg(),
        0
    );

    int32_t capsule_height = is_landscape ? 44 : 52;
    int32_t capsule_width = is_landscape ? 300 : display_width - 32;
    lv_obj_t * capsule = make_panel(
        footer,
        (display_width - capsule_width) / 2,
        (footer_height - capsule_height) / 2 - 1,
        capsule_width,
        capsule_height,
        0x111318,
        18
    );

    static const char * icons[] = {
        LV_SYMBOL_EDIT,
        LV_SYMBOL_LIST,
        LV_SYMBOL_HOME
    };
    int32_t item_size = is_landscape ? 34 : 40;
    int32_t slot = capsule_width / 3;
    for(uint32_t i = 0; i < 3; i++) {
        nav_buttons[i] = make_button(
            capsule,
            (int32_t)i * slot + (slot - item_size) / 2,
            (capsule_height - item_size) / 2,
            item_size,
            item_size,
            icons[i],
            0xFFFFFF,
            0x8E949D,
            item_size / 2,
            &lv_font_montserrat_16,
            nav_clicked_cb,
            i
        );
    }
    navigation_update();
}

void waferlog_ui_create(void)
{
    lv_obj_t * screen = lv_screen_active();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    display_width = lv_display_get_horizontal_resolution(lv_display_get_default());
    display_height = lv_display_get_vertical_resolution(lv_display_get_default());
    is_landscape = display_width > display_height;
    footer_height = is_landscape ? LANDSCAPE_FOOTER_HEIGHT : FOOTER_HEIGHT;
    if(!note_data_initialized) {
        for(uint32_t i = 0; i < NOTE_PAGE_COUNT; i++) {
            note_pages[i].paper = PAPER_RULED;
        }
        note_data_initialized = true;
    }

    if(clock_timer != NULL) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }
    if(toast_timer != NULL) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }

    app_root = make_panel(
        screen,
        0,
        0,
        display_width,
        display_height,
        theme_bg(),
        0
    );
    create_header();

    content_view = make_panel(
        app_root,
        0,
        HEADER_HEIGHT,
        display_width,
        display_height - HEADER_HEIGHT - footer_height,
        theme_bg(),
        0
    );
    create_footer();

    toast_label = lv_label_create(app_root);
    lv_label_set_text(toast_label, "");
    lv_obj_set_style_bg_color(toast_label, color(dark_mode ? 0xF6F7F8 : 0x121318), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(toast_label, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_text_color(toast_label, color(dark_mode ? 0x121318 : 0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(toast_label, &waferlog_font_14, LV_PART_MAIN);
    lv_obj_set_style_radius(toast_label, 9, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(toast_label, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(toast_label, 8, LV_PART_MAIN);
    lv_obj_align(toast_label, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT - 4);
    lv_obj_add_flag(toast_label, LV_OBJ_FLAG_HIDDEN);

    render_current_page();
    clock_timer = lv_timer_create(clock_timer_cb, 30000, NULL);
}
