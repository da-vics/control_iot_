#include "nvs_flash.h"

bool nvs_started = false;

void init_nvs(){
    
	esp_err_t err_v;
	err_v = nvs_flash_init();
	if (err_v == ESP_ERR_NVS_NO_FREE_PAGES || err_v == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err_v = nvs_flash_init();
	}
	if (err_v == ESP_OK)
	{
		nvs_started = true;
	}
	ESP_ERROR_CHECK(err_v);
}