#ifndef LVGL_INTERFACE_H
#define LVGL_INTERFACE_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl/lvgl.h"

#include "freertos/queue.h"

extern QueueHandle_t adc_queue; // Deklaracija vanjskog Queuea za prijenos ADC podataka
extern int adc_value;
void update_meter_value(int adc_value);

extern SemaphoreHandle_t xGuiSemaphore;
extern int pwm_value_r;  // Crvena komponenta
extern int pwm_value_g;  // Zelena komponenta
extern int pwm_value_b;  // Plava komponenta
extern const int LED_pin; // Deklaracija PWM varijable

void set_pwm_duty(int r, int g, int b);
void GUI_task(void *pvParameter); //inicijalizacija GUI

#endif 
