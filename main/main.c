#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"

#include "../includes/ScreenFunctions.h"
#include "../includes/Utilities.h"

void app_main(){

  init_nvs();
  PinSetups();
  init_Screen();
	drawImages();

  xTaskCreate(ScreenUpdateTask, "BatteryTask", 9072, NULL, 13, NULL);
  xTaskCreate(GetSystemInputTask, "SystemInputTask", 9072, NULL, 14, NULL);
  xTaskCreate(GetSystemParams, "SystemParamsTask", 9072, NULL, 12, NULL);
}
