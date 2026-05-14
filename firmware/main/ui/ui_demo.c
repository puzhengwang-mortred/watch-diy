#include "ui_demo.h"

#include "esp_app_desc.h"
#include "lvgl.h"
#include "svc_rtc.h"

#include <stdio.h>

/* Large clock digits: Montserrat 48; UI labels: Montserrat 14 (see sdkconfig.defaults). */
LV_FONT_DECLARE(lv_font_montserrat_48)
LV_FONT_DECLARE(lv_font_montserrat_14)

static void font_ui(lv_obj_t *obj)
{
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
}

/* List row may hold icon + label; set UI font on self and all children. */
static void font_ui_list_btn(lv_obj_t *btn)
{
    font_ui(btn);
    uint32_t n = lv_obj_get_child_cnt(btn);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(btn, (int32_t)i);
        if (c != NULL) {
            font_ui(c);
        }
    }
}

static lv_obj_t *s_scr_watch;
static lv_obj_t *s_scr_settings;
static lv_obj_t *s_scr_about;

static lv_obj_t *s_watch_time_lbl;
static lv_obj_t *s_watch_date_lbl;

static void watch_clock_refresh(void)
{
    if (s_watch_time_lbl == NULL || s_watch_date_lbl == NULL) {
        return;
    }

    svc_rtc_datetime_t dt;
    char tbuf[16];
    char dbuf[24];

    if (svc_rtc_have_chip() && svc_rtc_read_local(&dt) == ESP_OK && dt.valid) {
        (void)snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", dt.hour, dt.min, dt.sec);
        (void)snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d", dt.year, dt.mon, dt.mday);
        lv_label_set_text(s_watch_time_lbl, tbuf);
        lv_label_set_text(s_watch_date_lbl, dbuf);
    } else {
        lv_label_set_text(s_watch_time_lbl, "--:--:--");
        lv_label_set_text(s_watch_date_lbl, svc_rtc_have_chip() ? "RTC invalid" : "RTC offline");
    }
}

static void watch_clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    watch_clock_refresh();
}

static void apply_scr_style(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

static void on_nav(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *dest = lv_event_get_user_data(e);
    if (dest != NULL) {
        lv_scr_load(dest);
    }
}

static void style_title(lv_obj_t *lbl)
{
    font_ui(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
}

static void style_hint(lv_obj_t *lbl)
{
    font_ui(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x9098a0), 0);
}

static void style_btn(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x303848), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_pad_top(btn, 10, 0);
    lv_obj_set_style_pad_bottom(btn, 10, 0);
}

static void build_watch(lv_obj_t *scr)
{
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "Watch face");
    style_title(t);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "--:--:--");
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xe8ecf0), 0);
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, -16);
    s_watch_time_lbl = time_lbl;

    lv_obj_t *date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, "---");
    style_hint(date_lbl);
    lv_obj_set_width(date_lbl, LV_HOR_RES - 32);
    lv_label_set_long_mode(date_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, 52);
    s_watch_date_lbl = date_lbl;

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_width(btn, LV_HOR_RES - 48);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -36);
    style_btn(btn);
    lv_obj_add_event_cb(btn, on_nav, LV_EVENT_CLICKED, s_scr_settings);

    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Settings");
    font_ui(bl);
    lv_obj_center(bl);
}

static void build_settings(lv_obj_t *scr)
{
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "Settings");
    style_title(t);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, LV_HOR_RES - 40, 220);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 8);
    font_ui(list);

    lv_obj_t *it_about = lv_list_add_btn(list, NULL, "About");
    font_ui_list_btn(it_about);
    lv_obj_add_event_cb(it_about, on_nav, LV_EVENT_CLICKED, s_scr_about);

    lv_obj_t *it_back = lv_list_add_btn(list, NULL, "Watch face");
    font_ui_list_btn(it_back);
    lv_obj_add_event_cb(it_back, on_nav, LV_EVENT_CLICKED, s_scr_watch);
}

static void build_about(lv_obj_t *scr)
{
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "About");
    style_title(t);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 28);

    const esp_app_desc_t *app = esp_app_get_description();
    char buf[224];
    const char *name = app->project_name[0] != '\0' ? app->project_name : "watch-diy";
    snprintf(buf, sizeof(buf), "%s\n\nFirmware: %s\n\nIDF: %s", name, app->version, app->idf_ver);

    lv_obj_t *body = lv_label_create(scr);
    lv_label_set_text(body, buf);
    font_ui(body);
    lv_obj_set_style_text_color(body, lv_color_hex(0xc8d0d8), 0);
    lv_obj_set_width(body, LV_HOR_RES - 40);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_width(btn, LV_HOR_RES - 48);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -36);
    style_btn(btn);
    lv_obj_add_event_cb(btn, on_nav, LV_EVENT_CLICKED, s_scr_settings);

    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Back");
    font_ui(bl);
    lv_obj_center(bl);
}

void ui_demo_show(void)
{
    static bool s_done;
    if (s_done) {
        return;
    }

    lv_obj_t *prev = lv_scr_act();

    s_scr_watch = lv_obj_create(NULL);
    apply_scr_style(s_scr_watch);
    s_scr_settings = lv_obj_create(NULL);
    apply_scr_style(s_scr_settings);
    s_scr_about = lv_obj_create(NULL);
    apply_scr_style(s_scr_about);

    build_watch(s_scr_watch);
    build_settings(s_scr_settings);
    build_about(s_scr_about);

    lv_scr_load(s_scr_watch);

    if (prev != NULL && prev != s_scr_watch && prev != s_scr_settings && prev != s_scr_about) {
        lv_obj_del(prev);
    }

    watch_clock_refresh();
    lv_timer_create(watch_clock_timer_cb, 1000, NULL);

    s_done = true;
}
