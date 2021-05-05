#ifndef RGB_CTRL_H
#define RGB_CTRL_H

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led_strip.h"

#define delay(milli) vTaskDelay(milli / portTICK_RATE_MS);

#define LED_TYPE LED_STRIP_SK6812
#define LED_GPIO 15
#define LED_CHANNEL RMT_CHANNEL_0

typedef enum
{
    None,
    Normal,
    Charging,
    Overload,
    BatteryLow
} SystemState;

SystemState sysState = Normal;

static const rgb_t colors[] = {
    { .raw = { 255, 0, 0 } },
    { .raw = { 255, 127, 0 } },
    { .raw = { 255, 255, 0 } },
    { .raw = { 0, 255, 0 } },
    { .raw = { 0, 0, 255 } },
    { .raw = { 75, 0, 130 } },
    { .raw = { 143, 0, 255 } }
};

const rgb_t colorsDef = {.raw = {255, 0, 0}};

#define COLORS_TOTAL (sizeof(colors) / sizeof(rgb_t))

static void test(void *pvParameters)
{
    led_strip_t strip = {
        .type = LED_TYPE,
        .length = 1,
        .gpio = LED_GPIO,
        .channel = LED_CHANNEL,
        .buf = NULL
    };

    ESP_ERROR_CHECK(led_strip_init(&strip));
    size_t c = 0;

    while (1)
    {
        if(sysState == Charging){
            ESP_ERROR_CHECK(led_strip_fill(&strip, 0, strip.length, colors[c]));
            ESP_ERROR_CHECK(led_strip_flush(&strip));

            delay(500);
            if (++c >= COLORS_TOTAL)
                c = 0;
        }
        else{
            ESP_ERROR_CHECK(led_strip_fill(&strip, 0, strip.length, colorsDef));
            ESP_ERROR_CHECK(led_strip_flush(&strip));
        }
        delay(500);
    }
}//

#endif //RGB_CTRL_H

