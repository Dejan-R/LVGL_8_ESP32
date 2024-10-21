#include "lvgl_interface.h"
#include "lvgl_helpers.h"
#include "LVGL/logo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "driver/touch_pad.h"
#include "freertos/queue.h"

#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"
#include "LVGL/logo.h"

const int LED_pin = 0; 
int pwm_value_r = 0;  // Crvena komponenta
int pwm_value_g = 0;  // Zelena komponenta
int pwm_value_b = 0;  // Plava komponenta

lv_obj_t *screen1;  
lv_obj_t *screen2;
lv_meter_indicator_t * indic;
lv_obj_t* meter;
SemaphoreHandle_t xGuiSemaphore; 

#define TAG "LVGL_primjer"
#define LV_TICK_PERIOD_MS 10

static void lv_tick_task(void *arg);

// Vanjski Queue definiran u main.c
extern QueueHandle_t adc_queue;

//Ažuriranje  kazaljke za ADC
void update_meter_value(int adc_value) {
if (indic== NULL) {
    // Labela nije kreirana
    return;
}
    if (adc_value < 0) {
        adc_value = 0; 
    } else if (adc_value > 4095) {
        adc_value = 4095; 
    }

      lv_meter_set_indicator_value(meter, indic, adc_value);  
}


// Touch funkcija
void GUI_task(void *pvParameter);
// f-ja se periodično poziva za ažuriranje LVGL internih brojača
static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void update_pwm(); 

/* Callback f-ja za GUMB */
static void btn_event_cb(lv_event_t *e) {
       lv_obj_t * btn = lv_event_get_target(e);
    // Provjera je li gumb pritisnut
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Promijeni boju ovisno o trenutnom stanju gumba
        if (lv_obj_get_style_bg_color(btn, 0).full == lv_color_hex(0x808080).full) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x007300), 0); //zelena boja (uključeno)
             gpio_set_level(LED_pin, 1); 
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x808080), 0); // siva boja (isključeno)
            gpio_set_level(LED_pin, 0);  
        }
    }
}

/* Callback f-je za ekrane */
void btn1_event_cb(lv_event_t *e) {
    lv_scr_load(screen2);  // Prebacuje na screen2
    lv_obj_invalidate(screen2);  // Osigurava osvježavanje
}
void btn2_event_cb(lv_event_t *e) {
    lv_scr_load(screen1);  // Vraća na screen1
    lv_obj_invalidate(screen1);  // Osigurava osvježavanje
}

/* Callback funkcija za slidere*/
static void slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider); // Dohvati trenutnu vrijednost slidera

    // Provjeri koji slider je pozvao callback
    int slider_id = (int)lv_event_get_user_data(e);

    switch (slider_id) {
        case 0:
            pwm_value_r = value; // Ažuriraj crvenu komponentu
            break;
        case 1:
            pwm_value_g = value; // Ažuriraj zelenu komponentu
            break;
        case 2:
            pwm_value_b = value; // Ažuriraj plavu komponentu
            break;
    }

    update_pwm(); // Ažuriraj PWM izlaz
}


/*f-ja xpt2046_read vraća bool, a LVGL očekuje void povratni tip za read_cb,
stoga dodajemo wrapper funkciju koja poziva xpt2046_read i jednostavno zanemaruje povratnu vrijednost*/
void touch_driver_read_wrapper(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    // Pozovi stvarnu funkciju xpt2046_read i zanemari povratnu vrijednost
    xpt2046_read(indev_drv, data);
}


void GUI_task(void *pvParameter) {
    
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();
      
    // Inicijalizacija LVGL
    lv_init();
    
    /* Initializacija SPI ili I2C bus za drivere */
    lvgl_driver_init();
    
    //Kreiranje display buffera
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

    
      // ========================= PRVI EKRAN (default)=========================
    screen1 = lv_scr_act();  // globalna varijabla
    // Postavljanje boje pozadine na bijelu
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0xFFFFFF), 0); // Bijela boja
    lv_obj_set_style_bg_opa(screen1, LV_OPA_COVER, 0);              // Postavi neprozirnost na 100%

    //LOGO SSDJ
     lv_obj_t * label_logo1 = lv_img_create(screen1); // Stvara novi objekt slike
    lv_img_set_src(label_logo1, &logo);                     // Postavlja izvor slike na 'logo'
    lv_obj_align(label_logo1, LV_ALIGN_TOP_RIGHT, -10, 10); // logotip u gornjem desnom kutu (10px margine)
    
    //ISPIS NASLOVA
    lv_obj_t *label_naslov1 = lv_label_create(screen1);  
    lv_label_set_text(label_naslov1, "ESP32 + LVGL + 3.5 inch touch screen ili9488/xpt2046");     
    lv_obj_align(label_naslov1, LV_ALIGN_TOP_LEFT, 20, 20);       
    lv_label_set_long_mode(label_naslov1, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_obj_set_style_text_font(label_naslov1, &lv_font_montserrat_22, 0); // Postavljanje fonta
    lv_obj_set_width(label_naslov1, 300);


   // Naslov za ADC
    lv_obj_t *label_ADC_naslov = lv_label_create(screen1);  
    lv_label_set_text(label_ADC_naslov, "ADC (GPIO12):");
    lv_obj_align(label_ADC_naslov, LV_ALIGN_CENTER, 0, -80); 
    lv_obj_set_style_text_font(label_ADC_naslov, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_decor(label_ADC_naslov, LV_TEXT_DECOR_UNDERLINE, 0);

    // Gauge za ADC
    meter = lv_meter_create(screen1);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 160, 160);
    lv_obj_set_pos(meter, 40, 40);
    //skala
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, scale, 0, 4095, 265, 135);  //opseg  0-4095
    // oznaka za početak i kraj skale
    lv_meter_set_scale_ticks(meter, scale, 2, 0, 0, lv_palette_main(LV_PALETTE_DEEP_ORANGE));
    lv_meter_set_scale_major_ticks(meter, scale, 1, 0, 0, lv_color_black(), 0); 
    indic = lv_meter_add_arc(meter, scale, 5, lv_palette_main(LV_PALETTE_BLUE), -10);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 4095);
    //kazaljka
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_palette_main(LV_PALETTE_DEEP_ORANGE), -10);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 4095);


    lv_obj_t *label_GPIO_naslov = lv_label_create(screen1);  
    lv_label_set_text(label_GPIO_naslov, "LED toggle:");
    lv_obj_align(label_GPIO_naslov, LV_ALIGN_CENTER, -170, -80); 
    lv_obj_set_style_text_font(label_GPIO_naslov, &lv_font_montserrat_22, 0); 
    lv_obj_set_style_text_decor(label_GPIO_naslov, LV_TEXT_DECOR_UNDERLINE, 0);

    // GPIO gumb
    lv_obj_t * led_button = lv_btn_create(screen1);
    lv_obj_set_size(led_button, 100, 100);
    lv_obj_align(led_button, LV_ALIGN_CENTER, -170, 20); //Poravnanje ispod labela
    lv_obj_set_style_bg_color(led_button, lv_color_hex(0x808080), 0); // siva boja
    lv_obj_add_event_cb(led_button, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(led_button);
    lv_label_set_text(btn_label, "GPIO 0");
    lv_obj_center(btn_label);

    //gumb na prvom ekranu za prebacivanje na screen2
    lv_obj_t *btn_right = lv_btn_create(screen1);
    lv_obj_set_size(btn_right, 60, 60); // Veličina gumba
    lv_obj_align(btn_right, LV_ALIGN_RIGHT_MID, -10, 0); 
    lv_obj_t *label_right = lv_label_create(btn_right);
    lv_label_set_text(label_right, LV_SYMBOL_RIGHT);  //strelica desno
    lv_obj_center(label_right);
    lv_obj_add_event_cb(btn_right, btn1_event_cb, LV_EVENT_CLICKED, NULL);  // Dodavanje callback funkcije

// ========================= DRUGI EKRAN =========================
    screen2 = lv_obj_create(NULL);  // Kreiraj drugi ekran
    // Postavljanje boje pozadine na bijelu
    lv_obj_set_style_bg_color(screen2, lv_color_hex(0xFFFFFF), 0); // Bijela boja
    lv_obj_set_style_bg_opa(screen2, LV_OPA_COVER, 0);              // Postavi neprozirnost na 100%
   
    //LOGO SSDJ
    lv_obj_t * label_logo2= lv_img_create(screen2);  // Stvara novi objekt slike
    lv_img_set_src(label_logo2, &logo);                     // Postavlja izvor slike na 'logo'
    lv_obj_align(label_logo2, LV_ALIGN_TOP_RIGHT, -10, 10); // logotip u gornjem desnom kutu (10px margine)


       //ISPIS NASLOVA
    lv_obj_t *label_naslov2 = lv_label_create(screen2);  
    lv_label_set_text(label_naslov2, "ESP32 + LVGL + 3.5 inch touch screen ili9488/xpt2046");     
    lv_obj_align(label_naslov2, LV_ALIGN_TOP_LEFT, 20, 20);       
    lv_label_set_long_mode(label_naslov2, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
    lv_obj_set_style_text_font(label_naslov2, &lv_font_montserrat_22, 0); // Postavljanje fonta
    lv_obj_set_width(label_naslov2, 300);

     lv_obj_t *label_info2 = lv_label_create(screen2);
    lv_label_set_text(label_info2, "GPIO - PWM outputs");
    lv_obj_align(label_info2, LV_ALIGN_TOP_LEFT, 20, 60);    
    lv_obj_set_style_text_font(label_info2, &lv_font_montserrat_22, 0); // Postavljanje fonta

    lv_obj_t *label_pwm_naslov = lv_label_create(screen2);  
    lv_label_set_text(label_pwm_naslov, "RGB LED upravljanje:");
    lv_obj_align(label_pwm_naslov, LV_ALIGN_CENTER, -50, -40); 
    lv_obj_set_style_text_font(label_pwm_naslov, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_decor(label_pwm_naslov, LV_TEXT_DECOR_UNDERLINE, 0);

    // Slider za crvenu komponentu
    lv_obj_t *slider_r = lv_slider_create(screen2);
    lv_obj_align(slider_r, LV_ALIGN_CENTER, 0, 0); 
    lv_slider_set_range(slider_r, 0, 255);          
    lv_obj_add_event_cb(slider_r, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0); 
    lv_obj_set_style_bg_color(slider_r, lv_color_hex(0xFF0000), 0); // slider crvene boje
    lv_obj_set_style_bg_color(slider_r, lv_color_hex(0xFF0000), LV_PART_KNOB); // klizač crvene boje
    lv_obj_t *label_r = lv_label_create(screen2);
    lv_label_set_text(label_r, "R (GPIO 26)");
    lv_obj_align_to(label_r, slider_r, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Poravnaj s desne strane slajdera

    // Slider za zelenu komponentu
    lv_obj_t *slider_g = lv_slider_create(screen2);
    lv_obj_align(slider_g, LV_ALIGN_CENTER, 0, 40);
    lv_slider_set_range(slider_g, 0, 255);          
    lv_obj_add_event_cb(slider_g, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1); 
    lv_obj_set_style_bg_color(slider_g, lv_color_hex(0x00FF00), 0); //slider zelene boje
    lv_obj_set_style_bg_color(slider_g, lv_color_hex(0x00FF00), LV_PART_KNOB); // Klizač zelene boje
    lv_obj_t *label_g = lv_label_create(screen2);
    lv_label_set_text(label_g, "G (GPIO 32)");
    lv_obj_align_to(label_g, slider_g, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Poravnaj s desne strane slidera

    // Slider za plavu komponentu
    lv_obj_t *slider_b = lv_slider_create(screen2);
    lv_obj_align(slider_b, LV_ALIGN_CENTER, 0, 80);
    lv_slider_set_range(slider_b, 0, 255);           
    lv_obj_add_event_cb(slider_b, slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)2);
    lv_obj_set_style_bg_color(slider_b, lv_color_hex(0x0000FF), 0); // slider plave boje
    lv_obj_set_style_bg_color(slider_b, lv_color_hex(0x0000FF), LV_PART_KNOB); // Klizač plave boje
    lv_obj_t *label_b = lv_label_create(screen2);
    lv_label_set_text(label_b, "B (GPIO 33)");
    lv_obj_align_to(label_b, slider_b, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Poravnaj s desne strane slidera


    // GUMB ZA VRAĆANJE NA PRVI EKRAN
    lv_obj_t *btn_left = lv_btn_create(screen2);
    lv_obj_set_size(btn_left, 60, 60); // Veličina gumba
    lv_obj_align(btn_left, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t *label_left = lv_label_create(btn_left);
    lv_label_set_text(label_left, LV_SYMBOL_LEFT);  // strelica lijevo
    lv_obj_center(label_left);
    lv_obj_add_event_cb(btn_left, btn2_event_cb, LV_EVENT_CLICKED, NULL);  // Dodavanje callback funkcije


   while (1) {
    if (xQueueReceive(adc_queue, &adc_value, portMAX_DELAY)) {
        update_meter_value(adc_value);  // Ažuriraj labelu
    }

    if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE) {
        lv_task_handler();  // Osvježi GUI
        xSemaphoreGive(xGuiSemaphore);
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Održavaj interval osvježavanja GUI-ja
}
    vTaskDelete(NULL);
}
