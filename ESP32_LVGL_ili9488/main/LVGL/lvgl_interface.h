#ifndef LVGL_INTERFACE_H
#define LVGL_INTERFACE_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl/lvgl.h"
#include "freertos/queue.h"

// Definicija strukture za PWM podatke
typedef struct {
    int red;
    int green;
    int blue;
} pwm_data_t;

extern QueueHandle_t pwm_queue; // Deklaracija vanjskog Queue-a za PWM
extern QueueHandle_t adc_queue; // Deklaracija vanjskog Queue-a za prijenos ADC podataka
extern QueueHandle_t led_queue; // Deklaracija vanjskog Queue-a za GPIO toggle (LED)

extern SemaphoreHandle_t xGuiSemaphore;
void GUI_task(void *pvParameter); // inicijalizacija GUI

#endif 
