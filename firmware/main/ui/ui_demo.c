#include "ui_demo.h"

#include "bsp_board.h"
#include "bsp_lcd.h"
#include "esp_app_desc.h"
#include "lvgl.h"
#include "svc_batt.h"
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
static lv_obj_t *s_watch_batt_lbl;
static lv_obj_t *s_batt_outline;
static lv_obj_t *s_batt_fill;
static lv_obj_t *s_batt_nub;

/** Inner fill max width inside outline (LVGL-drawn icon, no symbol font). */
#define BATT_FILL_MAX_W 20
#define BATT_FILL_MIN_W 4

static void watch_batt_refresh(void)
{
    if (s_watch_batt_lbl == NULL || s_batt_fill == NULL) {
        return;
    }
    uint8_t pct = 0;
    bool low = false;
    if (svc_batt_read_percent(&pct, &low) == ESP_OK) {
        char b[12];
        (void)snprintf(b, sizeof(b), "%u%%", (unsigned)pct);
        lv_label_set_text(s_watch_batt_lbl, b);
        lv_obj_set_style_text_color(s_watch_batt_lbl, low ? lv_color_hex(0xff5544) : lv_color_hex(0x9098a0), 0);

        int fw = BATT_FILL_MIN_W + (int)pct * (BATT_FILL_MAX_W - BATT_FILL_MIN_W) / 100;
        if (fw > BATT_FILL_MAX_W) {
            fw = BATT_FILL_MAX_W;
        }
        lv_obj_set_width(s_batt_fill, fw);

        lv_color_t fill_c = low ? lv_color_hex(0xff5544)
                                : (pct <= 35 ? lv_color_hex(0xffcc44) : lv_color_hex(0x5cdb7a));
        lv_obj_set_style_bg_color(s_batt_fill, fill_c, 0);

        lv_color_t frame_c = low ? lv_color_hex(0xff5544) : lv_color_hex(0xb8c0c8);
        if (s_batt_outline != NULL) {
            lv_obj_set_style_border_color(s_batt_outline, frame_c, 0);
        }
        if (s_batt_nub != NULL) {
            lv_obj_set_style_bg_color(s_batt_nub, frame_c, 0);
        }
    } else {
        lv_label_set_text(s_watch_batt_lbl, "--%");
        lv_obj_set_style_text_color(s_watch_batt_lbl, lv_color_hex(0x9098a0), 0);
        lv_obj_set_width(s_batt_fill, BATT_FILL_MIN_W);
        lv_obj_set_style_bg_color(s_batt_fill, lv_color_hex(0x505860), 0);
        if (s_batt_outline != NULL) {
            lv_obj_set_style_border_color(s_batt_outline, lv_color_hex(0x707880), 0);
        }
        if (s_batt_nub != NULL) {
            lv_obj_set_style_bg_color(s_batt_nub, lv_color_hex(0x707880), 0);
        }
    }
}

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
    watch_batt_refresh();
}

/** Bottom chord at ~y=430 allows ~300px wide centered bar; keep margin inside circle. */
#define ROUND_FACE_BTN_W 280
/** Inscribed content width for labels (avoid corners outside circle). */
#define ROUND_FACE_TEXT_W (BSP_LCD_LVGL_H_RES - 100)

static void apply_scr_style(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

/* Physical circle is the glass bezel; LVGL clip_corner on scr caused AA/font glitches — layout uses ROUND_* only. */

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
    /* Round panel: align from center so title stays inside visible circle */
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -178);

    lv_obj_t *batt_grp = lv_obj_create(scr);
    lv_obj_set_size(batt_grp, 118, 30);
    lv_obj_set_style_bg_opa(batt_grp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(batt_grp, 0, 0);
    lv_obj_set_style_pad_all(batt_grp, 0, 0);
    lv_obj_clear_flag(batt_grp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(batt_grp, LV_ALIGN_CENTER, 96, -162);

    lv_obj_t *outline = lv_obj_create(batt_grp);
    s_batt_outline = outline;
    lv_obj_remove_style_all(outline);
    lv_obj_set_size(outline, 34, 17);
    lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(outline, 2, 0);
    lv_obj_set_style_border_color(outline, lv_color_hex(0xb8c0c8), 0);
    lv_obj_set_style_radius(outline, 3, 0);
    lv_obj_clear_flag(outline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(outline, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *fill = lv_obj_create(outline);
    s_batt_fill = fill;
    lv_obj_remove_style_all(fill);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fill, lv_color_hex(0x5cdb7a), 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_set_height(fill, 9);
    lv_obj_set_width(fill, BATT_FILL_MIN_W);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nub = lv_obj_create(batt_grp);
    s_batt_nub = nub;
    lv_obj_remove_style_all(nub);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(nub, lv_color_hex(0xb8c0c8), 0);
    lv_obj_set_style_radius(nub, 1, 0);
    lv_obj_set_size(nub, 3, 10);
    lv_obj_clear_flag(nub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(nub, outline, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

    lv_obj_t *batt_lbl = lv_label_create(batt_grp);
    lv_label_set_text(batt_lbl, "--%");
    font_ui(batt_lbl);
    lv_obj_set_style_text_color(batt_lbl, lv_color_hex(0x9098a0), 0);
    lv_obj_align_to(batt_lbl, nub, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    s_watch_batt_lbl = batt_lbl;

    lv_obj_t *time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "--:--:--");
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xe8ecf0), 0);
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, -16);
    s_watch_time_lbl = time_lbl;

    lv_obj_t *date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, "---");
    style_hint(date_lbl);
    lv_obj_set_width(date_lbl, ROUND_FACE_TEXT_W);
    lv_label_set_long_mode(date_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, 56);
    s_watch_date_lbl = date_lbl;

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_width(btn, ROUND_FACE_BTN_W);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
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
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -178);

    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, ROUND_FACE_TEXT_W, 200);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 4);
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
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -178);

    const esp_app_desc_t *app = esp_app_get_description();
    char buf[224];
    const char *name = app->project_name[0] != '\0' ? app->project_name : "watch-diy";
    snprintf(buf, sizeof(buf), "%s\n\nFirmware: %s\n\nIDF: %s", name, app->version, app->idf_ver);

    lv_obj_t *body = lv_label_create(scr);
    lv_label_set_text(body, buf);
    font_ui(body);
    lv_obj_set_style_text_color(body, lv_color_hex(0xc8d0d8), 0);
    lv_obj_set_width(body, ROUND_FACE_TEXT_W);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_width(btn, ROUND_FACE_BTN_W);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
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
    watch_batt_refresh();
    lv_timer_create(watch_clock_timer_cb, 1000, NULL);

    s_done = true;
}
