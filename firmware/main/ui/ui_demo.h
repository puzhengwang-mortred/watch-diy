#pragma once

/**
 * 创建表盘 / 设置（列表）/ 关于 三个独立 lv_screen 并加载表盘。
 * 切换仅 lv_scr_load，不反复 new，避免泄漏；须在 lvgl_port_lock 内或 LVGL 线程调用。
 */
void ui_demo_show(void);
