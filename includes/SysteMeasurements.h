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
#include "driver/ledc.h"
#include "RGBCtrl.h"

#define ScreenTask(action) gpio_set_level(PIN_NUM_BCKL, action);
#define ClearScreen TFT_fillScreen(TFT_BLACK);

#define MaxSampleTime 500
#define ACS_Sensitivity 0.030518
#define lowBattery 12.5f
#define fullBattery 15.0
const float batCal = (fullBattery - lowBattery);

#define OverloadSense 26
#define inverterCtrlBtn 22
#define inverterCtrl 18
#define ChargerCtrl 27
#define ChargerFanCtrl 21
#define InverterFanCtrl 19
#define PwrIndicator 5
#define FlashLightBtn 17
#define FlashLightCtrl 16

// #define LEDC_HS_TIMER LEDC_TIMER_0
// #define LEDC_HS_MODE LEDC_HIGH_SPEED_MODE
// #define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0
// #define LEDC_HS_CH1_GPIO (19)

uint32_t inverterState = 0,
         FlashState = 0,
         InverterFanState = 0;

bool flashUpdate = false,
     defaultLoad = true,
     falseShutdown = true,
     charge = false;

uint32_t screenState = 0;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

float LoadCurrent = 0,
      SolarCurrent = 0,
      SmpsCurrent = 0,
      BatteryVoltage = 0;

const int SolarOffset = 1997,
          SmpsOffset = 2032,
          LoadOffset = 2112;

int SolarPwr = 0,
    LoadPwr = 0,
    SmpsPwr = 0,
    batteryPercentage = 0;

bool sw = true;

void PinSetups(){

    gpio_pad_select_gpio(inverterCtrlBtn);
    gpio_pad_select_gpio(inverterCtrl);
    gpio_pad_select_gpio(ChargerFanCtrl);
    gpio_pad_select_gpio(ChargerCtrl);
    gpio_pad_select_gpio(InverterFanCtrl);
    gpio_pad_select_gpio(PwrIndicator);
    gpio_pad_select_gpio(FlashLightBtn);
    gpio_pad_select_gpio(FlashLightCtrl);
    gpio_pad_select_gpio(OverloadSense);

    gpio_set_direction(inverterCtrlBtn, GPIO_MODE_INPUT);
    gpio_set_direction(OverloadSense, GPIO_MODE_INPUT);
    gpio_set_direction(FlashLightBtn, GPIO_MODE_INPUT);
    gpio_set_direction(ChargerFanCtrl, GPIO_MODE_OUTPUT);
    gpio_set_direction(ChargerCtrl, GPIO_MODE_OUTPUT);
    gpio_set_direction(InverterFanCtrl, GPIO_MODE_OUTPUT);
    gpio_set_direction(inverterCtrl, GPIO_MODE_OUTPUT);
    gpio_set_direction(FlashLightCtrl, GPIO_MODE_OUTPUT);
    gpio_set_direction(PwrIndicator, GPIO_MODE_OUTPUT);

    inverterState = 0;
    gpio_set_level(inverterCtrl, inverterState);
}//

static void GetSystemInputTask(){

    uint32_t Current_tick_count = 0,
            New_tick_count = 0, 
            delay_in_ticks = 0;

    while(1){

        if (gpio_get_level(OverloadSense) == 0 && sysState!=Overload){
            delay(3000);
            if (gpio_get_level(OverloadSense) == 0){
                inverterState = 0;
                gpio_set_level(inverterCtrl, inverterState);
                sysState = Overload;
            }//
        }//
        
        if (gpio_get_level(inverterCtrlBtn) == 1){
            delay(100);
            if (gpio_get_level(inverterCtrlBtn) == 1){

                while (gpio_get_level(inverterCtrlBtn) == 1)
                    ;
                screenState = !screenState;
                inverterState = screenState;
                gpio_set_level(inverterCtrl, inverterState);
                ScreenTask(screenState);
                if(sysState == None){
                    defaultLoad = true;
                }//

                if(inverterState)
                    falseShutdown = false;
                else
                    falseShutdown = true;
             }
        }///

        if (gpio_get_level(FlashLightBtn) == 1){
            delay(100);
             if(gpio_get_level(FlashLightBtn) == 1){

                    Current_tick_count = xTaskGetTickCount();
                   	while (gpio_get_level(FlashLightBtn) ==1 )
                        ;
                    FlashState = !FlashState;
                    gpio_set_level(FlashLightCtrl, FlashState);
                    printf("%s\n", "flash");
                    flashUpdate = true;
                    delay(300);
            }//
        }//
        delay(50);
    }//
}//

static void GetSystemParams(){

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11); // batVoltage
    adc1_config_channel_atten(ADC1_CHANNEL_3,ADC_ATTEN_DB_11); // ntc_temp

    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); //solar
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); //smps
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); //load

    // ledc_timer_config_t ledc_timer = {
    //     .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    //     .freq_hz = 5000,                      // frequency of PWM signal
    //     .speed_mode = LEDC_HS_MODE,           // timer mode
    //     .timer_num = LEDC_HS_TIMER,            // timer index
    //     .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    // };

    // ledc_timer_config(&ledc_timer);

    // ledc_channel_config_t ledc_channel = {
    //     .channel = LEDC_HS_CH0_CHANNEL,
    //     .duty = 0,
    //     .gpio_num = LEDC_HS_CH1_GPIO,
    //     .speed_mode = LEDC_HS_MODE,
    //     .hpoint = 0,
    //     .timer_sel = LEDC_HS_TIMER};

    // ledc_channel_config(&ledc_channel);
    // ledc_fade_func_install(0);

    while (1){

        int batvol = 0, ntc = 0, sol = 0, smps = 0, load = 0;

        for (int i = 0; i < MaxSampleTime;++i){

            sol += adc1_get_raw(ADC1_CHANNEL_6);
            smps += adc1_get_raw(ADC1_CHANNEL_7);
            load += adc1_get_raw(ADC1_CHANNEL_4);
            batvol += adc1_get_raw(ADC1_CHANNEL_0);
            ntc += adc1_get_raw(ADC1_CHANNEL_3);
        }//

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

            printf("batvol %i\n", batvol);
            // // printf("ntc %i\n", ntc);
            printf("sol %i\n", sol);
            printf("smps %i\n", smps);
            printf("load %i\n", load);

        BatteryVoltage = ((3.3 / 4095.0) * batvol) * 6;
        LoadCurrent = (load - LoadOffset) * ACS_Sensitivity;
        SolarCurrent = (sol - SolarOffset) * ACS_Sensitivity;
        SmpsCurrent = (smps - SmpsOffset) * ACS_Sensitivity;

        // if(BatteryVoltage<14.5)
        //     gpio_set_level(ChargerCtrl,0);
        
        // else 
        //     gpio_set_level(ChargerCtrl,1);

        if(BatteryVoltage < lowBattery && sysState!=None){
            sysState = BatteryLow;
            inverterState = 0;
            gpio_set_level(inverterCtrl, inverterState);
        }//

        else{
            if(sysState !=None){
                sysState = Normal;
            }
        }//

        LoadPwr = (int)(LoadCurrent * BatteryVoltage);
        SolarPwr = (int)(SolarCurrent * BatteryVoltage);
        SmpsPwr = (int)(SmpsCurrent * BatteryVoltage);
        LoadPwr = (LoadPwr < 0) ? 0 : LoadPwr;
        SolarPwr = (SolarPwr < 0) ? 0 : SolarPwr;
        SmpsPwr = (SmpsPwr < 0) ? 0 : SmpsPwr;

        if(SmpsPwr>=10 || SolarPwr>=10){
            if(sysState == None)
                defaultLoad = true;
            sysState = Charging;
            charge = true;
            ScreenTask(1);
        }
        
        else{
            if (charge == true){
                sysState = Normal;
                charge = false;
                if (!screenState){
                    ScreenTask(0);
                }
            }
        }//

        if(LoadPwr>310 && sysState!=None){

            if(LoadPwr>500){
                sysState = Overload;
                inverterState = 0;
                gpio_set_level(inverterCtrl, inverterState);
            }

            else{
                delay(3000);
                if(LoadPwr>310){
                    sysState = Overload;
                    inverterState = 0;
                    gpio_set_level(inverterCtrl, inverterState);
                }
            }
        }//

        if(InverterFanState && falseShutdown == false){
            if(Temp<45){
                InverterFanState = false;
                TFT_fillRoundRect(120, 10, 15, 40, 0, TFT_BLACK);
                gpio_set_level(InverterFanCtrl,0);
            }
        }

        else if(Temp>=55 && falseShutdown == false){
            gpio_set_level(InverterFanCtrl,1);
            if(!InverterFanState)
	            TFT_jpg_image(120, 10,3, SPIFFS_BASE_PATH "/images/Fan.jpg", NULL, 0);
            InverterFanState = true;
        }//

        else{
            if(InverterFanState){
                gpio_set_level(InverterFanCtrl,0);
                TFT_fillRoundRect(120, 10, 15, 40, 0, TFT_BLACK);
                printf("%s\n", "in");
            }
            InverterFanState = false;
        }//

        batteryPercentage = ((BatteryVoltage - lowBattery) / batCal) * 100;

        printf("batvol %f\n", BatteryVoltage);
        // printf("sol %i\n", SolarPwr);
        // printf("smps %i\n", SmpsPwr);
        // printf("load %i\n", LoadPwr);
        printf("Percentage %i\n", batteryPercentage);

        delay(1000);
    }
}

#endif //SYSTEM_MEASUREMENTS_H