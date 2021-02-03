#ifndef SYSTEM_MEASUREMENTS_H
#define SYSTEM_MEASUREMENTS_H

#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define ScreenTask(action) gpio_set_level(PIN_NUM_BCKL, action);

#define delay(milli) vTaskDelay(milli / portTICK_RATE_MS);
#define MaxSampleTime 300
#define ACS_Sensitivity 0.030518
#define lowBattery 12.5
#define fullBattery 15.5

#define IotControl 22
#define ChargerFanCtrl 21
#define InverterFanCtrl 19
#define PwrIndicator 5
#define FlashLightBtn 17
#define FlashLightCtrl 16

uint32_t FlashState = 0, InverterFanState = 0;
bool flashUpdate = false;
uint8_t screenState = 0;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

float LoadCurrent = 0,
      SolarCurrent = 0,
      SmpsCurrent = 0,
      BatteryVoltage = 0;

const int SolarOffset = 2020,
          SmpsOffset = 2085,
          LoadOffset = 2043;

int SolarPwr = 0,
    LoadPwr = 0,
    SmpsPwr = 0,
    batteryPercentage = 0;


typedef enum{
	Normal,
	Charging
}SystemState;

SystemState sysState = Normal;
bool sw = false;

void PinSetups(){
    
    gpio_pad_select_gpio(IotControl);
    gpio_pad_select_gpio(ChargerFanCtrl);
    gpio_pad_select_gpio(InverterFanCtrl);
    gpio_pad_select_gpio(PwrIndicator);
    gpio_pad_select_gpio(FlashLightBtn);
    gpio_pad_select_gpio(FlashLightCtrl);
  
	gpio_set_direction(IotControl, GPIO_MODE_INPUT);
	gpio_set_direction(FlashLightBtn, GPIO_MODE_INPUT);
	gpio_set_direction(ChargerFanCtrl, GPIO_MODE_OUTPUT);
	gpio_set_direction(InverterFanCtrl, GPIO_MODE_OUTPUT);
	gpio_set_direction(FlashLightCtrl, GPIO_MODE_OUTPUT);
	gpio_set_direction(PwrIndicator, GPIO_MODE_OUTPUT);
}//

static void GetSystemInputTask(){

    uint32_t Current_tick_count = 0,
            New_tick_count = 0, 
            delay_in_ticks = 0;

    while(1){
        if (gpio_get_level(FlashLightBtn) == 1){
            delay(100);
             if(gpio_get_level(FlashLightBtn) == 1){

                    Current_tick_count = xTaskGetTickCount();
                   	while (gpio_get_level(FlashLightBtn) ==1 ){

						New_tick_count = xTaskGetTickCount();
				   		delay_in_ticks = (New_tick_count - Current_tick_count)/1000;

                        if(delay_in_ticks>=1){
						    printf("time %i\n", delay_in_ticks);
                            FlashState = !FlashState;
                            gpio_set_level(FlashLightCtrl, FlashState);
                            delay(300);
                            flashUpdate = true;
                            while (gpio_get_level(FlashLightBtn) ==1 )
                                ;
                        }//

                        delay(100);
                    }//
                    
					if(delay_in_ticks<1){
						screenState = !screenState;
						ScreenTask(screenState);
					}// 

                    delay(300);
            }
        }
        delay(50);
    }

}//

static void GetSystemParams(){

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11); // batVoltage
    adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11); // ntc_temp

    adc2_config_channel_atten(ADC2_CHANNEL_8, ADC_ATTEN_DB_11); //solar
    adc2_config_channel_atten(ADC2_CHANNEL_9, ADC_ATTEN_DB_11); //smps
    adc2_config_channel_atten(ADC2_CHANNEL_7, ADC_ATTEN_DB_11); //load

    while (1){

        int batvol = 0, ntc = 0, sol = 0, smps = 0, load = 0;

        for (int i = 0; i < MaxSampleTime;++i){
        int sol_temp, smps_temp, load_temp;
        adc2_get_raw(ADC2_CHANNEL_7, ADC_WIDTH_12Bit, &load_temp);
        adc2_get_raw(ADC2_CHANNEL_9, ADC_WIDTH_12Bit, &smps_temp);
        adc2_get_raw(ADC2_CHANNEL_8, ADC_WIDTH_12Bit, &sol_temp);
        sol += sol_temp;
        smps += smps_temp;
        load += load_temp;
        batvol += adc1_get_raw(ADC1_CHANNEL_6);
        ntc += adc1_get_raw(ADC1_CHANNEL_7);
        }

        sol /= MaxSampleTime;
        load /= MaxSampleTime;
        smps /= MaxSampleTime;
        batvol /= MaxSampleTime;
        ntc /= MaxSampleTime;

        float r2 = 10000 * (4095 / (float)ntc - 1.0);
        float logr2 = log(r2);
        float Temp = (1.0 / (c1 + c2*logr2 + c3*logr2*logr2*logr2));
        Temp = Temp - 273.15;

        // printf("temp %f\n", Temp);

        // printf("batvol %i\n", batvol);
        // printf("ntc %i\n", ntc);
        // printf("sol %i\n", sol);
        // printf("smps %i\n", smps);
        // printf("load %i\n", load);

        BatteryVoltage = ((3.3 / 4095.0) * batvol) * 5.54;
        LoadCurrent = (load - LoadOffset) * ACS_Sensitivity;
        SolarCurrent = (sol - SolarOffset) * ACS_Sensitivity;
        SmpsCurrent = (smps - SmpsOffset) * ACS_Sensitivity;

        LoadPwr = (int)(LoadCurrent * BatteryVoltage);
        SolarPwr = (int)(SolarCurrent * BatteryVoltage);
        SmpsPwr = (int)(SmpsCurrent * BatteryVoltage);
        LoadPwr = (LoadPwr < 0) ? 0 : LoadPwr;
        SolarPwr = (SolarPwr < 0) ? 0 : SolarPwr;
        SmpsPwr = (SmpsPwr < 0) ? 0 : SmpsPwr;

        if(Temp>=40){
            gpio_set_level(InverterFanCtrl,1);
            if(!InverterFanState)
	            TFT_jpg_image(120, 10,3, SPIFFS_BASE_PATH "/images/Fan.jpg", NULL, 0);
            InverterFanState = true;
        }
        else{
            if(InverterFanState){
                gpio_set_level(InverterFanCtrl,0);
                TFT_fillRoundRect(120, 10, 15, 40, 0, TFT_BLACK);
            }
            InverterFanState = false;
        }//

        batteryPercentage = ((BatteryVoltage - lowBattery) / 3) * 100;

        // printf("batvol %f\n", BatteryVoltage);
        // printf("sol %i\n", SolarPwr);
        // printf("smps %i\n", SmpsPwr);
        // printf("load %i\n", LoadPwr);
        // printf("Percentage %i\n", batteryPercentage);

        delay(1000);
    }
}

#endif //SYSTEM_MEASUREMENTS_H