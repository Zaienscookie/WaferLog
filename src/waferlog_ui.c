#include "waferlog_ui.h"

#include "lvgl/lvgl.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#define HEADER_HEIGHT 54
#define FOOTER_HEIGHT 42

static lv_obj_t * node_buttons[3];
static lv_obj_t * status_label;

// 注入魔法少女的灵魂!!!
static const lv_color_t color_canvas = LV_COLOR_MAKE(247, 245, 240);
static const lv_color_t color_ink = LV_COLOR_MAKE(36, 39, 45);
static const lv_color_t color_muted = LV_COLOR_MAKE(108, 112, 120);
static const lv_color_t color_teal = LV_COLOR_MAKE(24, 130, 133);
static const lv_color_t color_teal_soft = LV_COLOR_MAKE(216, 239, 235);
static const lv_color_t color_amber = LV_COLOR_MAKE(224, 154, 62);
static const lv_color_t color_paper = LV_COLOR_MAKE(255, 253, 248);

static void set_label_font(lv_obj_t * label, const lv_font_t * font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

// 更新魔法少女
static void update_selected_node(lv_obj_t * selected)
{
    for(uint32_t i = 0; i < 3; i++) {
        const bool is_selected = node_buttons[i] == selected;
        lv_obj_set_style_bg_color(
            node_buttons[i],
            is_selected ? color_teal_soft : color_paper,
            LV_PART_MAIN
        );
        lv_obj_set_style_border_color(
            node_buttons[i],
            is_selected ? color_teal : LV_COLOR_MAKE(225, 221, 212),
            LV_PART_MAIN
        );
    }
}

// Ciallo～(∠・ω< )⌒★
static void node_clicked_cb(lv_event_t * event)
{
    lv_obj_t * selected = lv_event_get_target(event);
    update_selected_node(selected);

    if(selected == node_buttons[0]) {
        lv_label_set_text(status_label, "已选中：灵感采集");
    }
    else if(selected == node_buttons[1]) {
        lv_label_set_text(status_label, "已选中：项目拆解");
    }
    else {
        lv_label_set_text(status_label, "已选中：待办清单");
    }
}

static lv_obj_t * create_node(
    lv_obj_t * parent,
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const char * title,
    const char * detail,
    lv_color_t accent,
    uint32_t index
)
{
    lv_obj_t * button = lv_button_create(parent);
    node_buttons[index] = button;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, color_paper, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, LV_COLOR_MAKE(225, 221, 212), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(button, LV_COLOR_MAKE(130, 125, 115), LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(button, node_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * accent_bar = lv_obj_create(button);
    lv_obj_remove_style_all(accent_bar);
    lv_obj_set_size(accent_bar, 5, h - 24);
    lv_obj_set_pos(accent_bar, 0, 12);
    lv_obj_set_style_radius(accent_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(accent_bar, accent, LV_PART_MAIN);

    lv_obj_t * title_label = lv_label_create(button);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 18, 11);
    set_label_font(title_label, &lv_font_source_han_sans_sc_16_cjk, color_ink);

    lv_obj_t * detail_label = lv_label_create(button);
    lv_label_set_text(detail_label, detail);
    lv_obj_set_pos(detail_label, 18, 36);
    set_label_font(detail_label, &lv_font_montserrat_12, color_muted);

    return button;
}

// 少女祷告中......
static void create_connector(
    lv_obj_t * parent,
    const lv_point_precise_t * points,
    uint32_t count
)
{
    lv_obj_t * connector = lv_line_create(parent);
    lv_obj_remove_style_all(connector);
    lv_line_set_points(connector, points, count);
    lv_obj_set_style_line_width(connector, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(connector, LV_COLOR_MAKE(190, 183, 170), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(connector, true, LV_PART_MAIN);
}

void waferlog_ui_create(void)
{
    lv_obj_t * screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, color_canvas, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_style_bg_color(header, color_ink, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "WaferLog | 硅笺");
    lv_obj_set_pos(title, 18, 9);
    set_label_font(title, &lv_font_source_han_sans_sc_16_cjk, LV_COLOR_WHITE);

    lv_obj_t * subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, "思维导图工作台");
    lv_obj_set_pos(subtitle, 18, 31);
    set_label_font(subtitle, &lv_font_montserrat_12, LV_COLOR_MAKE(190, 213, 210));

    lv_obj_t * sync = lv_label_create(header);
    lv_label_set_text(sync, "LOCAL");
    lv_obj_set_pos(sync, 263, 19);
    set_label_font(sync, &lv_font_montserrat_12, LV_COLOR_MAKE(245, 190, 102));

    lv_obj_t * canvas = lv_obj_create(screen);
    lv_obj_remove_style_all(canvas);
    lv_obj_set_pos(canvas, 0, HEADER_HEIGHT);
    lv_obj_set_size(canvas, SCREEN_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(canvas, color_canvas, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, LV_PART_MAIN);

    static const lv_point_precise_t left_link[] = {{155, 120}, {102, 90}, {48, 70}};
    static const lv_point_precise_t right_link[] = {{165, 120}, {218, 90}, {272, 70}};
    static const lv_point_precise_t bottom_link[] = {{160, 138}, {160, 218}, {160, 258}};
    create_connector(canvas, left_link, 3);
    create_connector(canvas, right_link, 3);
    create_connector(canvas, bottom_link, 3);

    lv_obj_t * center = lv_button_create(canvas);
    lv_obj_set_pos(center, 86, 83);
    lv_obj_set_size(center, 148, 56);
    lv_obj_set_style_radius(center, 28, LV_PART_MAIN);
    lv_obj_set_style_bg_color(center, color_ink, LV_PART_MAIN);
    lv_obj_set_style_border_width(center, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(center, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(center, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(center, color_ink, LV_PART_MAIN);

    lv_obj_t * center_title = lv_label_create(center);
    lv_label_set_text(center_title, "今日记录");
    lv_obj_center(center_title);
    set_label_font(center_title, &lv_font_source_han_sans_sc_16_cjk, LV_COLOR_WHITE);

    create_node(canvas, 12, 36, 88, 44, "灵感", "3 条新想法", color_amber, 0);
    create_node(canvas, 220, 36, 88, 44, "项目", "2 个进行中", color_teal, 1);
    create_node(canvas, 78, 258, 164, 58, "待办清单", "下一步：deepsleep", color_amber, 2);
    update_selected_node(node_buttons[0]);

    lv_obj_t * hint = lv_label_create(canvas);
    lv_label_set_text(hint, "点击节点查看上下文");
    lv_obj_set_pos(hint, 94, 331);
    set_label_font(hint, &lv_font_montserrat_12, color_muted);

    lv_obj_t * footer = lv_obj_create(screen);
    lv_obj_remove_style_all(footer);
    lv_obj_set_pos(footer, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_size(footer, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer, color_paper, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(footer, LV_COLOR_MAKE(230, 226, 217), LV_PART_MAIN);

    status_label = lv_label_create(footer);
    lv_label_set_text(status_label, "已选中：灵感采集");
    lv_obj_set_pos(status_label, 16, 11);
    set_label_font(status_label, &lv_font_source_han_sans_sc_16_cjk, color_ink);

    lv_obj_t * counter = lv_label_create(footer);
    lv_label_set_text(counter, "3 nodes");
    lv_obj_set_pos(counter, 260, 13);
    set_label_font(counter, &lv_font_montserrat_12, color_muted);
}
