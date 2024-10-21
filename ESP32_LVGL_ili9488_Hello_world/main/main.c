/*  dejan.rakijasic@gmail.com

  primjer: ESP32 + LVGL 8 + ili9488 tft + xpt2046 touch controller - HELLO WORLD! primjer

menuconfig: idf.py menuconfig
  - odabrati display driver ili9488
  - odabrati touch driver xpt2046

  - provjeriti postavke priključaka:
        ESP32 GPIO   DISPLAY GPIO 	
            5.0V 	    VCC 	
            GND 	    GND 	
            GPIO15  	CS 	
            GPIO4 	    RESET 	
            GPIO2 	    D/C 
            GPIO13  	SDI (MOSI)
            GPIO14  	SCK 	
            3.3V 	    LED 	
                        SDO (MISO)
            GPIO18 	    T_CLK	
            GPIO5     	T_CS 	
            GPIO23  	T_DIN 	
            GPIO19 	    T_OUT 	
            GPIO25  	T_IRQ
            
  * LVGL PC simulator: https://docs.lvgl.io/master/integration/ide/pc-simulator.html#select-an-ide
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"
#include "LVGL/logo.h"

#define TAG "LVGL_primjer"
#define LV_TICK_PERIOD_MS 10

static void lv_tick_task(void *arg);
void GUI_task(void *pvParameter);


void app_main() {

    //Task za LVGL
    xTaskCreatePinnedToCore(GUI_task, "LVGL_sucelje", 4096*2, NULL, 0, NULL, 1);
}

// f-ja se periodično poziva za ažuriranje LVGL internih brojača
static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

// LVGL semafor - LGVL 'thread-safety'
SemaphoreHandle_t xGuiSemaphore;

/*f-ja xpt2046_read vraća bool, a LVGL očekuje void povratni tip za read_cb,
stoga dodajemo wrapper funkciju koja poziva xpt2046_read i jednostavno zanemaruje povratnu vrijednost*/
void touch_driver_read_wrapper(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    // Pozovi stvarnu funkciju xpt2046_read i zanemari povratnu vrijednost
    xpt2046_read(indev_drv, data);
}

void GUI_task(void *pvParameter) {
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    //LVGL inicijalizacija
    lv_init();
    
    /* Inicijalizacija SPI ili I2C bus za drivere */
    lvgl_driver_init();
    

    //  display buffer
    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    // Inicijalizacija display drivera
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Inicijalizacija 'input device driver' (touch)
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    //indev_drv.read_cb = touch_driver_read;
    indev_drv.read_cb = touch_driver_read_wrapper;    // Postavljamo 'vlastitu' wrapper funkciju kao read callback
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);

    // Timer za upravljanje sa 'LVGL ticks'
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));


    // Kreiranje zaslona
    lv_obj_t * scr = lv_scr_act();
    // Postavljanje boje pozadine na bijelu
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0); // Bijela boja
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);              // Postavi neprozirnost na 100%


    //ISPIS NASLOVA
    lv_obj_t *label = lv_label_create(lv_scr_act());  
    lv_label_set_text(label, "ESP32 + LVGL + ili9488 tft + xpt2046 touch");     
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);   // Poravnanje
    lv_obj_set_width(label, 480);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_obj_set_style_text_font(label, &lv_font_montserrat_30, 0); // Postavljanje fonta

    //LOGO SSDJ
    lv_obj_t * img = lv_img_create(lv_scr_act());  // Stvara novi objekt slike
    lv_img_set_src(img, &logo);                     // Postavlja izvor slike na 'logo'
    lv_obj_align(img, LV_ALIGN_TOP_RIGHT, -10, 10); // logotip u gornjem desnom kutu (10px margine)



    while (1) {
        vTaskDelay(1);  
        // semafor za 'thread-safety'
        if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE) {
            lv_task_handler();  // upravlja LVGL task-ovima
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    vTaskDelete(NULL);
}
