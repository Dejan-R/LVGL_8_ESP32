/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 * 
 * 
 * LVLG 8.3.11 - SSD1306 primjer - https://components.espressif.com/components/lvgl/lvgl
 * primjer: https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd/i2c_oled
 * LVGL PC simulator: https://docs.lvgl.io/master/integration/ide/pc-simulator.html#select-an-ide
 */

#include "lvgl.h"

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_label_set_text(label, "SSD1306 OLED - LVGL - ESP32 primjer - Strukovna skola Djurdjevac");
    /* Size of the screen (if you use rotation 90 or 270, please set disp->driver->ver_res) */
    lv_obj_set_width(label, 150);
   
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}
