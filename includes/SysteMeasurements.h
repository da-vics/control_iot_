#ifndef SYSTEM_MEASUREMENTS_H
#define SYSTEM_MEASUREMENTS_H

#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define delay(milli) vTaskDelay(milli / portTICK_RATE_MS);

int batteryPercentage = 100,
    LoadCurrent = 0 ,
    SolarCurrent = 0,
    SmpsCurrent = 0,
    loadPwr = 0,
    SolarPwr = 0,
    SmpsPwr = 0;

typedef enum
{
	Normal,
	Charging
}SystemState;

SystemState sysState = Normal;
bool sw = false;

static void GetSystemParams(){

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11); // batVoltage
    adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11); // ntc_temp

    adc2_config_channel_atten(ADC2_CHANNEL_8, ADC_ATTEN_DB_11); //solar
    adc2_config_channel_atten(ADC2_CHANNEL_9, ADC_ATTEN_DB_11); //smps
    adc2_config_channel_atten(ADC2_CHANNEL_7, ADC_ATTEN_DB_11); //load

    while (1){
        
        // if(sysState == Normal)
        //     sysState = Charging;
        // else if (sysState == Charging){
        //     sw = true;
        //     sysState = Normal;
        // }
        // --SolarPwr;
        // --loadPwr;
        // --SmpsPwr;
        // --batteryPercentage;

       int batvol = adc1_get_raw(ADC1_CHANNEL_6);
       int ntc =   adc1_get_raw(ADC1_CHANNEL_7);

       int sol, smps, load;
       adc2_get_raw( ADC2_CHANNEL_7, ADC_WIDTH_12Bit, &load);
       adc2_get_raw( ADC2_CHANNEL_9, ADC_WIDTH_12Bit, &smps);
       adc2_get_raw( ADC2_CHANNEL_8, ADC_WIDTH_12Bit, &sol);

       printf("%i\n", batvol);
       printf("%i\n", ntc);
       printf("%i\n", sol);
       printf("%i\n", smps);
       printf("%i\n", load);
       delay(3000);
    }
}

#endif //SYSTEM_MEASUREMENTS_H
