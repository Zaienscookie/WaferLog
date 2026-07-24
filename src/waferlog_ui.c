#include "waferlog_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "lvgl/lvgl.h"

LV_FONT_DECLARE(waferlog_font_14);
LV_FONT_DECLARE(waferlog_font_16);

#define PORTRAIT_HEADER_HEIGHT 72
#define PORTRAIT_FOOTER_HEIGHT 56
#define LANDSCAPE_HEADER_HEIGHT 58
#define LANDSCAPE_FOOTER_HEIGHT 44
#define NOTE_CANVAS_MAX_WIDTH 464
#define NOTE_CANVAS_MAX_HEIGHT 376

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
static lv_obj_t * note_canvas;
static lv_obj_t * main_header;
static lv_obj_t * main_footer;
static bool is_landscape;
static bool has_handwriting;
static int32_t display_width;
static int32_t display_height;
static int32_t main_header_height;
static int32_t main_footer_height;
static int32_t note_canvas_width;
static int32_t note_canvas_height;
static int32_t brush_size = 5;
static lv_point_t previous_note_point;
static uint32_t previous_note_tick;
static uint8_t note_canvas_buffer[
    LV_CANVAS_BUF_SIZE(
        NOTE_CANVAS_MAX_WIDTH,
        NOTE_CANVAS_MAX_HEIGHT,
        16,
        LV_DRAW_BUF_STRIDE_ALIGN
    )
];

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
static void render_note_editor(void);

static void set_note_fullscreen(bool active)
{
    lv_obj_set_hidden(main_header, active);
    lv_obj_set_hidden(main_footer, active);
    lv_obj_set_pos(content_view, 0, active ? 0 : main_header_height);
    lv_obj_set_size(
        content_view,
        display_width,
        active ? display_height : display_height - main_header_height - main_footer_height
    );
}

static void note_back_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    render_home();
}

static void note_clear_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(note_canvas != NULL) {
        lv_canvas_fill_bg(note_canvas, lv_color_white(), LV_OPA_COVER);
        has_handwriting = false;
    }
}

static void note_save_clicked_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    has_handwriting = note_canvas != NULL && has_handwriting;
    render_home();
}

static void brush_size_changed_cb(lv_event_t * event)
{
    brush_size = lv_slider_get_value(lv_event_get_target_obj(event));
}

static void note_canvas_event_cb(lv_event_t * event)
{
    if(note_canvas == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(event);
    if(code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) {
        return;
    }

    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t area;
    lv_obj_get_coords(note_canvas, &area);
    point.x -= area.x1;
    point.y -= area.y1;
    if(point.x < 0) point.x = 0;
    if(point.y < 0) point.y = 0;
    if(point.x >= note_canvas_width) point.x = note_canvas_width - 1;
    if(point.y >= note_canvas_height) point.y = note_canvas_height - 1;

    if(code == LV_EVENT_PRESSED) {
        previous_note_point = point;
        previous_note_tick = lv_tick_get();
    }

    uint32_t elapsed = lv_tick_elaps(previous_note_tick);
    int32_t distance = LV_ABS(point.x - previous_note_point.x) + LV_ABS(point.y - previous_note_point.y);
    int32_t speed = elapsed > 0 ? distance * 10 / (int32_t)elapsed : distance * 10;
    int32_t stroke_width = brush_size;
    if(speed <= 2) stroke_width += 3;
    else if(speed <= 5) stroke_width += 1;
    else if(speed >= 12) stroke_width -= 1;
    if(stroke_width < 1) stroke_width = 1;
    if(stroke_width > 16) stroke_width = 16;

    lv_layer_t layer;
    lv_canvas_init_layer(note_canvas, &layer);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color(COLOR_INK);
    line.width = stroke_width;
    line.round_start = 1;
    line.round_end = 1;
    line.p1.x = previous_note_point.x;
    line.p1.y = previous_note_point.y;
    line.p2.x = point.x;
    line.p2.y = point.y;
    lv_draw_line(&layer, &line);
    lv_canvas_finish_layer(note_canvas, &layer);
    previous_note_point = point;
    previous_note_tick = lv_tick_get();
    has_handwriting = true;
}

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
        render_note_editor();
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
    text_style(title_label, &waferlog_font_16, COLOR_INK);
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
    text_style(title_label, &waferlog_font_16, COLOR_INK);

    lv_obj_t * summary_label = lv_label_create(card);
    lv_label_set_text(summary_label, summary);
    lv_obj_set_pos(summary_label, 28, 34);
    text_style(summary_label, &waferlog_font_14, COLOR_MUTED);
}

static void render_portrait_home(void)
{
    lv_obj_t * heading = lv_label_create(content_view);
    lv_label_set_text(heading, "工作台");
    lv_obj_set_pos(heading, 16, 12);
    text_style(heading, &waferlog_font_16, COLOR_INK);

    lv_obj_t * heading_detail = lv_label_create(content_view);
    lv_label_set_text(heading_detail, "把想法留下，再决定如何处理");
    lv_obj_set_pos(heading_detail, 16, 37);
    text_style(heading_detail, &waferlog_font_14, COLOR_MUTED);

    lv_obj_t * capture_card = make_card(content_view, 12, 66, 296, 88, COLOR_DARK, COLOR_DARK);
    lv_obj_t * capture_title = lv_label_create(capture_card);
    lv_label_set_text(capture_title, "记录一条新内容");
    lv_obj_set_pos(capture_title, 16, 14);
    text_style(capture_title, &waferlog_font_16, COLOR_WHITE);

    lv_obj_t * capture_detail = lv_label_create(capture_card);
    lv_label_set_text(capture_detail, "文字、语音、图片都可以先保存");
    lv_obj_set_pos(capture_detail, 16, 42);
    text_style(capture_detail, &waferlog_font_14, 0xB9E5E2);

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
    text_style(new_label, &waferlog_font_16, COLOR_WHITE);

    lv_obj_t * quick_title = lv_label_create(content_view);
    lv_label_set_text(quick_title, "快速入口");
    lv_obj_set_pos(quick_title, 16, 171);
    text_style(quick_title, &waferlog_font_16, COLOR_INK);

    add_action_card(content_view, 12, 198, 94, "笔记", LV_SYMBOL_EDIT, COLOR_TEAL_PALE, 1);
    add_action_card(content_view, 113, 198, 94, "录音", "REC", COLOR_YELLOW_PALE, 2);
    add_action_card(content_view, 214, 198, 94, "导入", LV_SYMBOL_UPLOAD, COLOR_BLUE_PALE, 3);

    lv_obj_t * recent_title = lv_label_create(content_view);
    lv_label_set_text(recent_title, "最近内容");
    lv_obj_set_pos(recent_title, 16, 294);
    text_style(recent_title, &waferlog_font_16, COLOR_INK);

    if(has_handwriting) {
        add_note_card(content_view, 12, 320, 296, "本地笔记", "已保存到本地", COLOR_TEAL);
    }
    else {
        add_note_card(content_view, 12, 320, 296, "还没有保存的笔记", "从快速入口开始创建第一条内容", COLOR_TEAL);
    }
}

static void render_note_editor(void)
{
    set_note_fullscreen(true);
    lv_obj_clean(content_view);
    update_tabs(false);

    lv_obj_t * title = lv_label_create(content_view);
    lv_label_set_text(title, "新建笔记");
    lv_obj_set_pos(title, 16, 16);
    text_style(title, &waferlog_font_16, COLOR_INK);

    note_canvas_width = display_width - 16;
    note_canvas_height = display_height - 104;
    note_canvas = lv_canvas_create(content_view);
    lv_canvas_set_buffer(
        note_canvas,
        note_canvas_buffer,
        note_canvas_width,
        note_canvas_height,
        LV_COLOR_FORMAT_RGB565
    );
    if(!has_handwriting) {
        lv_canvas_fill_bg(note_canvas, color(COLOR_WHITE), LV_OPA_COVER);
    }
    lv_obj_set_pos(note_canvas, 8, 42);
    lv_obj_set_clickable(note_canvas, true);
    lv_obj_add_event_cb(note_canvas, note_canvas_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * back_button = lv_button_create(content_view);
    lv_obj_set_pos(back_button, 8, display_height - 54);
    lv_obj_set_size(back_button, 52, 42);
    lv_obj_set_style_radius(back_button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_button, color(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_width(back_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(back_button, color(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_add_event_cb(back_button, note_back_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_icon = lv_label_create(back_button);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    text_style(back_icon, &lv_font_montserrat_16, COLOR_MUTED);

    lv_obj_t * clear_button = lv_button_create(content_view);
    lv_obj_set_pos(clear_button, 68, display_height - 54);
    lv_obj_set_size(clear_button, 52, 42);
    lv_obj_set_style_radius(clear_button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(clear_button, color(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_border_width(clear_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(clear_button, color(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_add_event_cb(clear_button, note_clear_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * clear_icon = lv_label_create(clear_button);
    lv_label_set_text(clear_icon, LV_SYMBOL_CLOSE);
    lv_obj_center(clear_icon);
    text_style(clear_icon, &lv_font_montserrat_16, COLOR_MUTED);

    lv_obj_t * size_label = lv_label_create(content_view);
    lv_label_set_text(size_label, "SIZE");
    lv_obj_set_pos(size_label, 132, display_height - 44);
    text_style(size_label, &lv_font_montserrat_12, COLOR_MUTED);

    lv_obj_t * size_slider = lv_slider_create(content_view);
    lv_obj_set_pos(size_slider, 168, display_height - 45);
    lv_obj_set_size(size_slider, 92, 14);
    lv_slider_set_range(size_slider, 2, 12);
    lv_slider_set_value(size_slider, brush_size, LV_ANIM_OFF);
    lv_obj_add_event_cb(size_slider, brush_size_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * save_button = lv_button_create(content_view);
    lv_obj_set_pos(save_button, display_width - 82, display_height - 54);
    lv_obj_set_size(save_button, 74, 42);
    lv_obj_set_style_radius(save_button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(save_button, color(COLOR_TEAL), LV_PART_MAIN);
    lv_obj_set_style_border_width(save_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(save_button, note_save_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * save_label = lv_label_create(save_button);
    lv_label_set_text(save_label, "保存");
    lv_obj_center(save_label);
    text_style(save_label, &waferlog_font_16, COLOR_WHITE);
    set_status("笔记编辑");
}

static void render_landscape_home(void)
{
    lv_obj_t * title = lv_label_create(content_view);
    lv_label_set_text(title, "WaferLog 工作台");
    lv_obj_set_pos(title, 16, 14);
    text_style(title, &waferlog_font_16, COLOR_INK);

    lv_obj_t * left = make_card(content_view, 12, 48, 166, 154, COLOR_WHITE, COLOR_BORDER);
    lv_obj_t * left_title = lv_label_create(left);
    lv_label_set_text(left_title, "快速记录");
    lv_obj_set_pos(left_title, 16, 12);
    text_style(left_title, &waferlog_font_16, COLOR_INK);
    add_action_card(left, 12, 44, 66, "笔记", "+", COLOR_TEAL_PALE, 1);
    add_action_card(left, 88, 44, 66, "录音", "MIC", COLOR_YELLOW_PALE, 2);

    lv_obj_t * right_title = lv_label_create(content_view);
    lv_label_set_text(right_title, "最近更新");
    lv_obj_set_pos(right_title, 198, 14);
    text_style(right_title, &waferlog_font_16, COLOR_INK);
    add_note_card(content_view, 198, 48, 270, "T5AI 界面想法", "2 个节点 · 待 AI 分析", COLOR_TEAL);
    add_note_card(content_view, 198, 116, 270, "今天的产品灵感", "录音 03:18 · 本地保存", COLOR_BLUE);
}

static void render_home(void)
{
    set_note_fullscreen(false);
    lv_obj_clean(content_view);
    note_canvas = NULL;
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
    set_note_fullscreen(false);
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
        text_style(message_title, &waferlog_font_16, COLOR_INK);

        lv_obj_t * message_detail = lv_label_create(message);
        lv_label_set_text(message_detail, "横屏启动模拟器查看日历视图");
        lv_obj_set_pos(message_detail, 24, 60);
        text_style(message_detail, &waferlog_font_14, COLOR_MUTED);

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
    text_style(event, &waferlog_font_14, COLOR_WHITE);

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
    display_width = lv_display_get_horizontal_resolution(lv_display_get_default());
    display_height = lv_display_get_vertical_resolution(lv_display_get_default());
    is_landscape = display_width > display_height;
    main_header_height = is_landscape ? LANDSCAPE_HEADER_HEIGHT : PORTRAIT_HEADER_HEIGHT;
    main_footer_height = is_landscape ? LANDSCAPE_FOOTER_HEIGHT : PORTRAIT_FOOTER_HEIGHT;
    home_tab = NULL;
    calendar_tab = NULL;

    lv_obj_set_style_bg_color(screen, color(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    main_header = lv_obj_create(screen);
    lv_obj_remove_style_all(main_header);
    lv_obj_set_size(main_header, display_width, main_header_height);
    lv_obj_set_style_bg_color(main_header, color(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_header, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * title = lv_label_create(main_header);
    lv_label_set_text(title, "WaferLog");
    lv_obj_set_pos(title, 16, is_landscape ? 9 : 12);
    text_style(title, &lv_font_montserrat_20, COLOR_INK);

    lv_obj_t * brand = lv_label_create(main_header);
    lv_label_set_text(brand, is_landscape ? "硅笺" : "硅笺 · 本地工作区");
    lv_obj_set_pos(brand, 16, is_landscape ? 34 : 40);
    text_style(brand, &waferlog_font_16, COLOR_TEAL);

    if(is_landscape) {
        home_tab = lv_button_create(main_header);
        lv_obj_set_pos(home_tab, display_width - 146, 10);
        lv_obj_set_size(home_tab, 60, 34);
        lv_obj_set_style_radius(home_tab, 17, LV_PART_MAIN);
        lv_obj_set_style_border_width(home_tab, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(home_tab, tab_clicked_cb, LV_EVENT_CLICKED, (void *)1U);
        lv_obj_t * home_label = lv_label_create(home_tab);
        lv_label_set_text(home_label, "首页");
        lv_obj_center(home_label);
        text_style(home_label, &waferlog_font_16, COLOR_MUTED);

        calendar_tab = lv_button_create(main_header);
        lv_obj_set_pos(calendar_tab, display_width - 78, 10);
        lv_obj_set_size(calendar_tab, 62, 34);
        lv_obj_set_style_radius(calendar_tab, 17, LV_PART_MAIN);
        lv_obj_set_style_border_width(calendar_tab, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(calendar_tab, tab_clicked_cb, LV_EVENT_CLICKED, (void *)2U);
        lv_obj_t * calendar_label = lv_label_create(calendar_tab);
        lv_label_set_text(calendar_label, "日历");
        lv_obj_center(calendar_label);
        text_style(calendar_label, &waferlog_font_16, COLOR_MUTED);
    }
    else {
        lv_obj_t * local_badge = lv_obj_create(main_header);
        lv_obj_remove_style_all(local_badge);
        lv_obj_set_pos(local_badge, display_width - 72, 20);
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
    lv_obj_set_pos(content_view, 0, main_header_height);
    lv_obj_set_size(content_view, display_width, display_height - main_header_height - main_footer_height);
    lv_obj_set_style_bg_color(content_view, color(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(content_view, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(content_view, LV_SCROLLBAR_MODE_OFF);

    main_footer = lv_obj_create(screen);
    lv_obj_remove_style_all(main_footer);
    lv_obj_set_pos(main_footer, 0, display_height - main_footer_height);
    lv_obj_set_size(main_footer, display_width, main_footer_height);
    lv_obj_set_style_bg_color(main_footer, color(is_landscape ? COLOR_DARK : COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_footer, LV_OPA_COVER, LV_PART_MAIN);

    if(is_landscape) {
        status_label = lv_label_create(main_footer);
        lv_label_set_text(status_label, "WaferLog · 本地模式");
        lv_obj_set_pos(status_label, 16, 13);
        text_style(status_label, &waferlog_font_16, COLOR_WHITE);

        lv_obj_t * status_right = lv_label_create(main_footer);
        lv_label_set_text(status_right, "SYNC OFFLINE");
        lv_obj_set_pos(status_right, display_width - 106, 16);
        text_style(status_right, &lv_font_montserrat_12, 0xB9E5E2);
    }
    else {
        status_label = lv_label_create(main_footer);
        lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t * navigation = lv_obj_create(main_footer);
        lv_obj_remove_style_all(navigation);
        lv_obj_set_pos(navigation, 12, 4);
        lv_obj_set_size(navigation, display_width - 24, 48);
        lv_obj_set_style_radius(navigation, 18, LV_PART_MAIN);
        lv_obj_set_style_bg_color(navigation, color(COLOR_DARK), LV_PART_MAIN);

        static const char * navigation_icons[] = {
            LV_SYMBOL_HOME,
            LV_SYMBOL_EDIT,
            LV_SYMBOL_LIST,
            LV_SYMBOL_SETTINGS
        };

        for(uint32_t i = 0; i < 4; i++) {
            lv_obj_t * item = lv_button_create(navigation);
            lv_obj_set_pos(item, 18 + (int32_t)i * 70, 6);
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
