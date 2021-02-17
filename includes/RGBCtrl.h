#ifndef RGB_CTRL_H
#define RGB_CTRL_H

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led_strip.h"

#define LED_TYPE LED_STRIP_SK6812
#define LED_GPIO 15
#define LED_CHANNEL RMT_CHANNEL_0

static const rgb_t colors[] = {
    { .raw = { 0x00, 0x00, 0x2f } },
    { .raw = { 0x00, 0x2f, 0x00 } },
    { .raw = { 0x00, 0x3f, 0x00 } },
    { .raw = { 0x00, 0x4f, 0x00 } },
    { .raw = { 0x00, 0x5f, 0x00 } },
    { .raw = { 0x00, 0x6f, 0x00 } },
    { .raw = { 0x00, 0x7f, 0x00 } },
    { .raw = { 0x00, 0x8f, 0x00 } },
    { .raw = { 0x00, 0x9f, 0x00 } },
    { .raw = { 0x00, 0xff, 0x00 } },
    { .raw = { 0x2f, 0x00, 0x00 } },
};

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
        ESP_ERROR_CHECK(led_strip_fill(&strip, 0, strip.length, colors[c]));
        ESP_ERROR_CHECK(led_strip_flush(&strip));

        delay(1000);
        if (++c >= COLORS_TOTAL)
            c = 0;
        delay(500);
    }
}

#endif //RGB_CTRL_H

