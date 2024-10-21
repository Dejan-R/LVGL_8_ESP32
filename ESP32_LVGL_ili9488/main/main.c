/*  dejan.rakijasic@gmail.com

  primjer: ESP32 + LVGL 8 + ili9488 tft + xpt2046 touch controller 


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
#include "lvgl_interface.h"

#include "driver/ledc.h"

#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"


#define ADC2_CHAN0 ADC_CHANNEL_5  // GPIO12 = ADC2_5

QueueHandle_t adc_queue;
adc_oneshot_unit_handle_t adc2_handle; 
int adc_value;

void init_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC2_CHAN0, &config));
}

// ADC task koji čita ADC vrijednosti i šalje ih u Queue
void adc_task(void *arg) {
    while (1) {
        adc_oneshot_read(adc2_handle, ADC2_CHAN0, &adc_value);
xQueueSend(adc_queue, &adc_value, portMAX_DELAY);
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}



void pwm_init() {
   ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_8_BIT,  // 8-bit
    .freq_hz = 5000,                       // 5 kHz
    .clk_cfg = LEDC_USE_APB_CLK
};
ledc_timer_config(&ledc_timer);

ledc_channel_config_t ledc_channel_r = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0, // Početna vrijednost
    .hpoint = 0,
    .gpio_num = 26
};
ledc_channel_config(&ledc_channel_r);


ledc_channel_config_t ledc_channel_g = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0, // Početna vrijednost
    .hpoint = 0,
    .gpio_num = 32
};
ledc_channel_config(&ledc_channel_g);


ledc_channel_config_t ledc_channel_b = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_2,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0, // Početna vrijednost
    .hpoint = 0,
    .gpio_num = 33
};
ledc_channel_config(&ledc_channel_b);
}


void set_pwm_duty(int r, int g, int b) {
    // Postavi PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, r);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, g);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}


// Funkcija za ažuriranje PWM izlaza
void update_pwm() {
    set_pwm_duty(pwm_value_r, pwm_value_g, pwm_value_b); // Postavi PWM izlaz na trenutnu vrijednost
}


void app_main() {
    pwm_init();
    init_adc();

    // Kreiramo Queue za prijenos ADC vrijednosti
    adc_queue = xQueueCreate(10, sizeof(int));
if (adc_queue == NULL) {
}
    // Kreiramo task za čitanje ADC-a
    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);

    esp_rom_gpio_pad_select_gpio(LED_pin);
    gpio_set_direction(LED_pin, GPIO_MODE_OUTPUT);

    //Task za LVGL
    xTaskCreatePinnedToCore(GUI_task, "LVGL_sucelje", 4096*2, NULL, 0, NULL, 1);
}

