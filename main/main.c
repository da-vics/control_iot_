#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <esp_intr_alloc.h>

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

#include <ctype.h>

#include "../includes/ScreenFunctions.h"
#include "../includes/Utilities.h"


void app_main(){

	init_nvs();
  init_Screen();

}
