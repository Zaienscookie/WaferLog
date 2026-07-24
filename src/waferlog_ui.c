#include "waferlog_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "lvgl/lvgl.h"

#define PORTRAIT_HEADER_HEIGHT 72
#define PORTRAIT_FOOTER_HEIGHT 56
#define LANDSCAPE_HEADER_HEIGHT 58
#define LANDSCAPE_FOOTER_HEIGHT 44

#define COLOR_BACKGROUND 0xE9F8F6
#define COLOR_WHITE 0xFFFFFF
#define COLOR_INK 0x202A33
#define COLOR_MUTED 0x6F7C84
#define COLOR_TEAL 0x27B6B1
#define COLOR_TEAL_PALE 0xD5F3EF
#define COLOR_BLUE 0x55B8E8
#define COLOR_BLUE_PALE 0xDDF3FC
#define COLOR_YELLOW 0xF2B84B
#define COLOR_YELLOW_PALE 0xFFF0C7
#define COLOR_PINK 0xF08FA7
#define COLOR_PINK_PALE 0xFCE2EA
#define COLOR_BORDER 0xD5E9E7
#define COLOR_DARK 0x173B48

static lv_obj_t * content_view;
static lv_obj_t * status_label;
static lv_obj_t * home_tab;
static lv_obj_t * calendar_tab;
static bool is_landscape;

static lv_color_t color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

static void text_style(lv_obj_t * object, const lv_font_t * font, uint32_t rgb)
{
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(object, color(rgb), LV_PART_MAIN);
}

static void set_status(const char * message)
{
    if(status_label != NULL) {
        lv_label_set_text(status_label, message);
    }
}

static void update_tabs(bool calendar_active)
{
    if(home_tab == NULL || calendar_tab == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(
        home_tab,
        color(calendar_active ? COLOR_WHITE : COLOR_TEAL),
        LV_PART_MAIN
    );
    lv_obj_set_style_bg_color(
        calendar_tab,
        color(calendar_active ? COLOR_TEAL : COLOR_WHITE),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        home_tab,
        color(calendar_active ? COLOR_MUTED : COLOR_WHITE),
        LV_PART_MAIN
    );
    lv_obj_set_style_text_color(
        calendar_tab,
        color(calendar_active ? COLOR_WHITE : COLOR_MUTED),
        LV_PART_MAIN
    );
}

static void render_home(void);
static void render_calendar(void);

static void tab_clicked_cb(lv_event_t * event)
{
    uintptr_t target = (uintptr_t)lv_event_get_user_data(event);
    if(target == 1U) {
        render_home();
    }
    else {
        render_calendar();
    }
}

static void action_clicked_cb(lv_event_t * event)
{
    uintptr_t action = (uintptr_t)lv_event_get_user_data(event);
    if(action == 1U) {
        set_status("笔记编辑器入口已准备");
    }
    else if(action == 2U) {
        set_status("录音入口已准备");
    }
    else {
        set_status("图片入口已准备");
    }
}

static lv_obj_t * make_card(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t background,
    uint32_t border
)
{
    lv_obj_t * card = lv_button_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, color(background), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color(border), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, color(COLOR_DARK), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    return card;
}

static void add_action_card(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    const char * title,
    const char * detail,
    const char * symbol,
    uint32_t background,
    uintptr_t action
)
{
    lv_obj_t * card = make_card(parent, x, y, width, 76, background, COLOR_WHITE);
    lv_obj_add_event_cb(card, action_clicked_cb, LV_EVENT_CLICKED, (void *)action);

    lv_obj_t * symbol_label = lv_label_create(card);
    lv_label_set_text(symbol_label, symbol);
    lv_obj_set_pos(symbol_label, 14, 12);
    text_style(symbol_label, &lv_font_montserrat_20, COLOR_INK);

    lv_obj_t * title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 14, 39);
    text_style(title_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    lv_obj_t * detail_label = lv_label_create(card);
    lv_label_set_text(detail_label, detail);
    lv_obj_set_pos(detail_label, width - 64, 48);
    text_style(detail_label, &lv_font_source_han_sans_sc_14_cjk, COLOR_MUTED);
}

static void add_note_card(
    lv_obj_t * parent,
    int32_t x,
    int32_t y,
    int32_t width,
    const char * title,
    const char * summary,
    uint32_t accent
)
{
    lv_obj_t * card = make_card(parent, x, y, width, 58, COLOR_WHITE, COLOR_BORDER);

    lv_obj_t * accent_bar = lv_obj_create(card);
    lv_obj_remove_style_all(accent_bar);
    lv_obj_set_pos(accent_bar, 12, 12);
    lv_obj_set_size(accent_bar, 5, 34);
    lv_obj_set_style_radius(accent_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(accent_bar, color(accent), LV_PART_MAIN);

    lv_obj_t * title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 28, 10);
    text_style(title_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    lv_obj_t * summary_label = lv_label_create(card);
    lv_label_set_text(summary_label, summary);
    lv_obj_set_pos(summary_label, 28, 34);
    text_style(summary_label, &lv_font_source_han_sans_sc_14_cjk, COLOR_MUTED);
}

static void render_portrait_home(void)
{
    lv_obj_t * heading = lv_label_create(content_view);
    lv_label_set_text(heading, "工作台");
    lv_obj_set_pos(heading, 16, 12);
    text_style(heading, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    lv_obj_t * heading_detail = lv_label_create(content_view);
    lv_label_set_text(heading_detail, "把想法留下，再决定如何处理");
    lv_obj_set_pos(heading_detail, 16, 37);
    text_style(heading_detail, &lv_font_source_han_sans_sc_14_cjk, COLOR_MUTED);

    lv_obj_t * capture_card = make_card(content_view, 12, 66, 296, 88, COLOR_DARK, COLOR_DARK);
    lv_obj_t * capture_title = lv_label_create(capture_card);
    lv_label_set_text(capture_title, "记录一条新内容");
    lv_obj_set_pos(capture_title, 16, 14);
    text_style(capture_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_WHITE);

    lv_obj_t * capture_detail = lv_label_create(capture_card);
    lv_label_set_text(capture_detail, "文字、语音、图片都可以先保存");
    lv_obj_set_pos(capture_detail, 16, 42);
    text_style(capture_detail, &lv_font_source_han_sans_sc_14_cjk, 0xB9E5E2);

    lv_obj_t * new_button = lv_button_create(capture_card);
    lv_obj_set_pos(new_button, 214, 27);
    lv_obj_set_size(new_button, 66, 34);
    lv_obj_set_style_radius(new_button, 17, LV_PART_MAIN);
    lv_obj_set_style_bg_color(new_button, color(COLOR_TEAL), LV_PART_MAIN);
    lv_obj_set_style_border_width(new_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(new_button, action_clicked_cb, LV_EVENT_CLICKED, (void *)1U);
    lv_obj_t * new_label = lv_label_create(new_button);
    lv_label_set_text(new_label, "新建");
    lv_obj_center(new_label);
    text_style(new_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_WHITE);

    lv_obj_t * quick_title = lv_label_create(content_view);
    lv_label_set_text(quick_title, "快速入口");
    lv_obj_set_pos(quick_title, 16, 171);
    text_style(quick_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    add_action_card(content_view, 12, 198, 94, "笔记", "文字", LV_SYMBOL_EDIT, COLOR_TEAL_PALE, 1);
    add_action_card(content_view, 113, 198, 94, "录音", "声音", "REC", COLOR_YELLOW_PALE, 2);
    add_action_card(content_view, 214, 198, 94, "导入", "文件", LV_SYMBOL_UPLOAD, COLOR_BLUE_PALE, 3);

    lv_obj_t * recent_title = lv_label_create(content_view);
    lv_label_set_text(recent_title, "最近内容");
    lv_obj_set_pos(recent_title, 16, 294);
    text_style(recent_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    add_note_card(content_view, 12, 320, 296, "还没有保存的笔记", "从快速入口开始创建第一条内容", COLOR_TEAL);
}

static void render_landscape_home(void)
{
    lv_obj_t * title = lv_label_create(content_view);
    lv_label_set_text(title, "WaferLog 工作台");
    lv_obj_set_pos(title, 16, 14);
    text_style(title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

    lv_obj_t * left = make_card(content_view, 12, 48, 166, 154, COLOR_WHITE, COLOR_BORDER);
    lv_obj_t * left_title = lv_label_create(left);
    lv_label_set_text(left_title, "快速记录");
    lv_obj_set_pos(left_title, 16, 12);
    text_style(left_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);
    add_action_card(left, 12, 44, 66, "笔记", "", "+", COLOR_TEAL_PALE, 1);
    add_action_card(left, 88, 44, 66, "录音", "", "MIC", COLOR_YELLOW_PALE, 2);

    lv_obj_t * right_title = lv_label_create(content_view);
    lv_label_set_text(right_title, "最近更新");
    lv_obj_set_pos(right_title, 198, 14);
    text_style(right_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);
    add_note_card(content_view, 198, 48, 270, "T5AI 界面想法", "2 个节点 · 待 AI 分析", COLOR_TEAL);
    add_note_card(content_view, 198, 116, 270, "今天的产品灵感", "录音 03:18 · 本地保存", COLOR_BLUE);
}

static void render_home(void)
{
    lv_obj_clean(content_view);
    update_tabs(false);
    if(is_landscape) {
        render_landscape_home();
    }
    else {
        render_portrait_home();
    }
    set_status("WaferLog · 本地模式");
}

static int days_in_month(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return days[month - 1];
}

static void render_calendar(void)
{
    lv_obj_clean(content_view);
    update_tabs(true);

    time_t now = time(NULL);
    struct tm local_time = *localtime(&now);
    int year = local_time.tm_year + 1900;
    int month = local_time.tm_mon + 1;
    int today = local_time.tm_mday;
    char month_text[32];
    lv_snprintf(month_text, sizeof(month_text), "%04d / %02d", year, month);

    if(!is_landscape) {
        lv_obj_t * message = make_card(content_view, 16, 74, 288, 150, COLOR_WHITE, COLOR_BORDER);
        lv_obj_t * message_title = lv_label_create(message);
        lv_label_set_text(message_title, "桌面电子日历");
        lv_obj_set_pos(message_title, 24, 24);
        text_style(message_title, &lv_font_source_han_sans_sc_16_cjk, COLOR_INK);

        lv_obj_t * message_detail = lv_label_create(message);
        lv_label_set_text(message_detail, "横屏启动模拟器查看日历视图");
        lv_obj_set_pos(message_detail, 24, 60);
        text_style(message_detail, &lv_font_source_han_sans_sc_14_cjk, COLOR_MUTED);

        lv_obj_t * command = lv_label_create(message);
        lv_label_set_text(command, "waferlog.exe --landscape");
        lv_obj_set_pos(command, 24, 99);
        text_style(command, &lv_font_montserrat_12, COLOR_TEAL);
        set_status("日历 · 请使用横屏模式");
        return;
    }

    lv_obj_t * date_card = make_card(content_view, 12, 12, 112, 194, COLOR_DARK, COLOR_DARK);
    lv_obj_t * month_label = lv_label_create(date_card);
    lv_label_set_text(month_label, month_text);
    lv_obj_set_pos(month_label, 14, 15);
    text_style(month_label, &lv_font_montserrat_12, 0xB9E5E2);

    char today_text[8];
    lv_snprintf(today_text, sizeof(today_text), "%02d", today);
    lv_obj_t * day_label = lv_label_create(date_card);
    lv_label_set_text(day_label, today_text);
    lv_obj_set_pos(day_label, 13, 44);
    text_style(day_label, &lv_font_montserrat_48, COLOR_WHITE);

    lv_obj_t * date_hint = lv_label_create(date_card);
    lv_label_set_text(date_hint, "TODAY");
    lv_obj_set_pos(date_hint, 16, 105);
    text_style(date_hint, &lv_font_montserrat_12, 0xF5BE66);

    lv_obj_t * event = lv_label_create(date_card);
    lv_label_set_text(event, "20:00  AI 回顾");
    lv_obj_set_pos(event, 16, 140);
    text_style(event, &lv_font_source_han_sans_sc_14_cjk, COLOR_WHITE);

    lv_obj_t * calendar = make_card(content_view, 136, 12, 332, 194, COLOR_WHITE, COLOR_BORDER);
    static const char * weekdays[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    for(int col = 0; col < 7; col++) {
        lv_obj_t * weekday = lv_label_create(calendar);
        lv_label_set_text(weekday, weekdays[col]);
        lv_obj_set_pos(weekday, 12 + col * 45, 10);
        text_style(weekday, &lv_font_montserrat_12, col > 4 ? COLOR_TEAL : COLOR_MUTED);
    }

    struct tm first_day = local_time;
    first_day.tm_mday = 1;
    mktime(&first_day);
    int start_column = (first_day.tm_wday + 6) % 7;
    int month_days = days_in_month(year, month);

    for(int day = 1; day <= month_days; day++) {
        int cell = start_column + day - 1;
        int row = cell / 7;
        int col = cell % 7;
        lv_obj_t * day_button = lv_button_create(calendar);
        lv_obj_set_pos(day_button, 8 + col * 45, 31 + row * 26);
        lv_obj_set_size(day_button, 38, 22);
        lv_obj_set_style_radius(day_button, 11, LV_PART_MAIN);
        lv_obj_set_style_border_width(day_button, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(
            day_button,
            color(day == today ? COLOR_TEAL : COLOR_WHITE),
            LV_PART_MAIN
        );

        char day_text[4];
        lv_snprintf(day_text, sizeof(day_text), "%d", day);
        lv_obj_t * day_label = lv_label_create(day_button);
        lv_label_set_text(day_label, day_text);
        lv_obj_center(day_label);
        text_style(day_label, &lv_font_montserrat_12, day == today ? COLOR_WHITE : COLOR_INK);
    }

    set_status("日历 · 今日有 1 个 AI 回顾");
}

void waferlog_ui_create(void)
{
    lv_obj_t * screen = lv_screen_active();
    int32_t screen_width = lv_display_get_horizontal_resolution(lv_display_get_default());
    int32_t screen_height = lv_display_get_vertical_resolution(lv_display_get_default());
    is_landscape = screen_width > screen_height;
    int32_t header_height = is_landscape ? LANDSCAPE_HEADER_HEIGHT : PORTRAIT_HEADER_HEIGHT;
    int32_t footer_height = is_landscape ? LANDSCAPE_FOOTER_HEIGHT : PORTRAIT_FOOTER_HEIGHT;
    home_tab = NULL;
    calendar_tab = NULL;

    lv_obj_set_style_bg_color(screen, color(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, screen_width, header_height);
    lv_obj_set_style_bg_color(header, color(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "WaferLog");
    lv_obj_set_pos(title, 16, is_landscape ? 9 : 12);
    text_style(title, &lv_font_montserrat_20, COLOR_INK);

    lv_obj_t * brand = lv_label_create(header);
    lv_label_set_text(brand, is_landscape ? "硅笺" : "硅笺 · 本地工作区");
    lv_obj_set_pos(brand, 16, is_landscape ? 34 : 40);
    text_style(brand, &lv_font_source_han_sans_sc_16_cjk, COLOR_TEAL);

    if(is_landscape) {
        home_tab = lv_button_create(header);
        lv_obj_set_pos(home_tab, screen_width - 146, 10);
        lv_obj_set_size(home_tab, 60, 34);
        lv_obj_set_style_radius(home_tab, 17, LV_PART_MAIN);
        lv_obj_set_style_border_width(home_tab, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(home_tab, tab_clicked_cb, LV_EVENT_CLICKED, (void *)1U);
        lv_obj_t * home_label = lv_label_create(home_tab);
        lv_label_set_text(home_label, "首页");
        lv_obj_center(home_label);
        text_style(home_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_MUTED);

        calendar_tab = lv_button_create(header);
        lv_obj_set_pos(calendar_tab, screen_width - 78, 10);
        lv_obj_set_size(calendar_tab, 62, 34);
        lv_obj_set_style_radius(calendar_tab, 17, LV_PART_MAIN);
        lv_obj_set_style_border_width(calendar_tab, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(calendar_tab, tab_clicked_cb, LV_EVENT_CLICKED, (void *)2U);
        lv_obj_t * calendar_label = lv_label_create(calendar_tab);
        lv_label_set_text(calendar_label, "日历");
        lv_obj_center(calendar_label);
        text_style(calendar_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_MUTED);
    }
    else {
        lv_obj_t * local_badge = lv_obj_create(header);
        lv_obj_remove_style_all(local_badge);
        lv_obj_set_pos(local_badge, screen_width - 72, 20);
        lv_obj_set_size(local_badge, 56, 30);
        lv_obj_set_style_radius(local_badge, 15, LV_PART_MAIN);
        lv_obj_set_style_bg_color(local_badge, color(COLOR_TEAL_PALE), LV_PART_MAIN);
        lv_obj_t * local_label = lv_label_create(local_badge);
        lv_label_set_text(local_label, "LOCAL");
        lv_obj_center(local_label);
        text_style(local_label, &lv_font_montserrat_12, COLOR_TEAL);
    }

    content_view = lv_obj_create(screen);
    lv_obj_remove_style_all(content_view);
    lv_obj_set_pos(content_view, 0, header_height);
    lv_obj_set_size(content_view, screen_width, screen_height - header_height - footer_height);
    lv_obj_set_style_bg_color(content_view, color(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content_view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(content_view, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * footer = lv_obj_create(screen);
    lv_obj_remove_style_all(footer);
    lv_obj_set_pos(footer, 0, screen_height - footer_height);
    lv_obj_set_size(footer, screen_width, footer_height);
    lv_obj_set_style_bg_color(footer, color(is_landscape ? COLOR_DARK : COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);

    if(is_landscape) {
        status_label = lv_label_create(footer);
        lv_label_set_text(status_label, "WaferLog · 本地模式");
        lv_obj_set_pos(status_label, 16, 13);
        text_style(status_label, &lv_font_source_han_sans_sc_16_cjk, COLOR_WHITE);

        lv_obj_t * status_right = lv_label_create(footer);
        lv_label_set_text(status_right, "SYNC OFFLINE");
        lv_obj_set_pos(status_right, screen_width - 106, 16);
        text_style(status_right, &lv_font_montserrat_12, 0xB9E5E2);
    }
    else {
        status_label = lv_label_create(footer);
        lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t * navigation = lv_obj_create(footer);
        lv_obj_remove_style_all(navigation);
        lv_obj_set_pos(navigation, 12, 4);
        lv_obj_set_size(navigation, screen_width - 24, 48);
        lv_obj_set_style_radius(navigation, 18, LV_PART_MAIN);
        lv_obj_set_style_bg_color(navigation, color(COLOR_DARK), LV_PART_MAIN);

        static const char * navigation_icons[] = {
            LV_SYMBOL_HOME,
            LV_SYMBOL_EDIT,
            LV_SYMBOL_UPLOAD,
            LV_SYMBOL_LIST,
            LV_SYMBOL_SETTINGS
        };

        for(uint32_t i = 0; i < 5; i++) {
            lv_obj_t * item = lv_button_create(navigation);
            lv_obj_set_pos(item, 13 + (int32_t)i * 55, 6);
            lv_obj_set_size(item, 40, 36);
            lv_obj_set_style_radius(item, 18, LV_PART_MAIN);
            lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
            lv_obj_set_style_bg_color(
                item,
                color(i == 0 ? COLOR_WHITE : COLOR_DARK),
                LV_PART_MAIN
            );

            lv_obj_t * icon = lv_label_create(item);
            lv_label_set_text(icon, navigation_icons[i]);
            lv_obj_center(icon);
            text_style(icon, &lv_font_montserrat_16, i == 0 ? COLOR_DARK : 0xA9C7C5);
        }
    }

    render_home();
}
