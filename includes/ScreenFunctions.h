#ifndef SCREENFUNCTIONS_H
#define SCREENFUNCTIONS_H

#include "SysteMeasurements.h"

#define SPI_BUS TFT_HSPI_HOST

int PrevLoad = -1, PrevSmps = -1, prevSolar = -1;
bool loadAnimate = false, solarAnimate = false, smpsAnimate = false;
bool colorchange = false, colorchangEvent = false;

typedef enum{
	clear_state,bat1_state, bat2_state, 
	bat3_state, bat4_state, 
	bat5_state, bat6_state,Empty
}prevBatState;
prevBatState prevstate = clear_state;

typedef enum{
	clear_charge,bat1_charge, bat2_charge, 
	bat3_charge, bat4_charge, 
	bat5_charge, bat6_charge
}prevBatCharge;
prevBatCharge prevcharge = clear_charge;

//screen initialisations
void init_Screen(){

    tft_disp_type = DISP_TYPE_ILI9341;
	_width = DEFAULT_TFT_DISPLAY_WIDTH;	  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension

	max_rdclock = 8000000;
	TFT_PinsInit();

	spi_lobo_device_handle_t spi;

	spi_lobo_bus_config_t buscfg ={
		.miso_io_num = PIN_NUM_MISO, // set SPI MISO pin
		.mosi_io_num = PIN_NUM_MOSI, // set SPI MOSI pin
		.sclk_io_num = PIN_NUM_CLK,	 // set SPI CLK pin
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 6 * 1024,
	};

	spi_lobo_device_interface_config_t devcfg ={
		.clock_speed_hz = 8000000,		   // Initial clock out at 8 MHz
		.mode = 0,						   // SPI mode 0
		.spics_io_num = -1,				   // we will use external CS pin
		.spics_ext_io_num = PIN_NUM_CS,	   // external CS pin
		.flags = LB_SPI_DEVICE_HALFDUPLEX, // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
	};

	esp_err_t ret;
    ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
	assert(ret == ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	ret = spi_lobo_device_select(spi, 1);
	assert(ret == ESP_OK);
	ret = spi_lobo_device_deselect(spi);
	assert(ret == ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n",
		spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n",
		spi_lobo_uses_native_pins(spi) ? "true" : "false");

	TFT_display_init();

	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
	TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(LANDSCAPE_FLIP);
	TFT_setFont(TestFont, NULL);
	TFT_resetclipwin();

	vfs_spiffs_register();
	if (!spiffs_is_mounted)
		printf("SPIFFS not mounted");
	else
		printf("SPIFFS mounted");

    _fg = TFT_CYAN;
	TFT_setFont(TestFont, NULL);
	TFT_print("PowerFlow", CENTER, CENTER);
    delay(1000);
	ClearScreen;
	screenState = 0;
	ScreenTask(screenState);
}

void clear_batGauage(int x, int y, int w, int h, int r){
	TFT_fillRoundRect(x, y, w, h, r, TFT_WHITE);
}

void clearAllbatGauage(){
	TFT_fillRoundRect(120 ,67,80,23,7,TFT_WHITE); //100%
	TFT_fillRoundRect(120 ,92,80,23,7,TFT_WHITE); //80%
	TFT_fillRoundRect(120 ,117,80,23,7,TFT_WHITE);//60%
	TFT_fillRoundRect(120 ,142,80,23,7,TFT_WHITE);//40%
	TFT_fillRoundRect(120 ,167,80,23,7,TFT_WHITE);//20%
	TFT_fillRoundRect(130 ,190,60,5,1,TFT_WHITE);//10%
}

//display images
void drawImages(){

	TFT_jpg_image(270, 110,3, SPIFFS_BASE_PATH "/images/AcOutput.jpg", NULL, 0);
	TFT_jpg_image(15, 105,3, SPIFFS_BASE_PATH "/images/GridCharger.jpg", NULL, 0);

	TFT_drawRoundRect(110 ,50,100,150, 20, TFT_DARKGREY);//outer-body
	TFT_fillRoundRect(110 ,50,100,150, 20, TFT_DARKGREY);

	TFT_fillRoundRect(115 ,55,90,140, 20, TFT_WHITE	); //inner-body
	
	TFT_drawRoundRect(139, 39, 40, 10, 0, TFT_DARKGREY);//head
	TFT_fillRoundRect(139, 39, 40, 10, 0, TFT_DARKGREY);

	// TFT_drawCircle(60, 120, 2, TFT_WHITE);
}

//update values
void UpdateVal(){	

		if(flashUpdate){
   			if(FlashState)
   		   		TFT_jpg_image(100, 10,3, SPIFFS_BASE_PATH "/images/FlashLight.jpg", NULL, 0);
   			else
   		    	TFT_fillRoundRect(100, 10, 15, 40, 0, TFT_BLACK);
			flashUpdate = false;
		}//

		char loadtemp[6],smpstemp[6];
		sprintf((char *)loadtemp, "%iW", LoadPwr);
		sprintf((char *)smpstemp, "%iW", SmpsPwr);

		TFT_setFont(DEJAVU18_FONT, NULL);

		if(LoadPwr>=5){
			loadAnimate = !loadAnimate;
			if(loadAnimate)
				TFT_drawRoundRect(265,105, 33,33, 10, TFT_GREEN);
			else 
				TFT_drawRoundRect(265, 105, 33,33, 10, TFT_BLACK);

			delay(100);
		}//

		else 
			TFT_drawRoundRect(265, 105, 33,33, 10, TFT_BLACK);

		if(SmpsPwr>=5){
			smpsAnimate = !smpsAnimate;
			if(smpsAnimate)
				TFT_drawRoundRect(10, 100, 33,35, 10, TFT_WHITE);
			else 
				TFT_drawRoundRect(10, 100, 33,35, 10, TFT_BLACK);
			delay(100);
		}//

		else 
			TFT_drawRoundRect(10, 100, 33,35, 10, TFT_BLACK);

		if(LoadPwr!=PrevLoad){ //load
			_fg = TFT_GREEN;
			_bg = TFT_BLACK;
			TFT_fillRect(265,145,255,140, TFT_BLACK);
			TFT_print(loadtemp,265 , 145);
			PrevLoad = LoadPwr;
		}//

		if(SmpsPwr!=PrevSmps){ //smps
			_fg = TFT_WHITE;
			_bg = TFT_BLACK;
			TFT_fillRect(15, 140, 45,160, TFT_BLACK);
			TFT_print(smpstemp,15,140);
			PrevSmps = SmpsPwr;
		}//
}//

void fillBat(){
	TFT_fillRoundRect(120 ,67,80,23,7,TFT_GREEN); //100%
	TFT_fillRoundRect(120 ,92,80,23,7,TFT_GREEN); //80%
	TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%
	TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
	TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
}
static void ScreenUpdateTask(){

	bool bat1 = false, bat2 = false, bat3 = false,
		 bat4 = false, bat5 = false, bat6 = false, bat7 = false;

	while (1){

		if(defaultLoad){
			ClearScreen;
			drawImages();
			defaultLoad = false;
			prevSolar = PrevLoad = PrevSmps = -1;
			sysState = Normal;
		}
/**
 * 12.2 13 14 15.0
 * 
*/
		if(sysState == Charging || sysState == Normal)
			UpdateVal();
		
		if(sysState == Normal){

			bat1 = (batteryPercentage >= 86 && batteryPercentage <= 100) ? true : false;
			bat2 = (batteryPercentage >= 66 && batteryPercentage <= 85) ? true : false;
			bat3 = (batteryPercentage >= 46 && batteryPercentage <= 65) ? true : false;
			bat4 = (batteryPercentage >= 26 && batteryPercentage <= 45) ? true : false;
			bat5 = (batteryPercentage >= 10 && batteryPercentage <= 25) ? true : false;
			bat6 = (batteryPercentage>=1 && batteryPercentage <= 9) ? true : false;
			bat7 = (batteryPercentage == 0) ? true : false;

			if(batteryPercentage >= 15 && batteryPercentage <= 20)
				colorchange = true;	
			else{
				colorchangEvent = false;
				colorchange = false;
			}

			if(bat1 == true && prevstate!=bat1_state){
				prevstate = bat1_state;
				clearAllbatGauage();
				fillBat();
			}//

			else if(bat2 == true && prevstate!= bat2_state){
				clearAllbatGauage();
				TFT_fillRoundRect(120 ,92,80,23,7,TFT_GREEN); //80%
				TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%
				TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
				TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
				prevstate = bat2_state;
			}//
			
			else if(bat3 == true && prevstate!=bat3_state){
				clearAllbatGauage();
				TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%
				TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
				TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
				prevstate = bat3_state;
			}//

			else if(bat4 == true && prevstate!=bat4_state){
				clearAllbatGauage();
				TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
				TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
				prevstate = bat4_state;
			}//

			else if((bat5 == true && prevstate!=bat5_state) || colorchange){
				if(!colorchange){
					clearAllbatGauage();
					TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
				}
				else if(colorchange && colorchangEvent == false){
					clearAllbatGauage();
					TFT_fillRoundRect(120 ,167,80,23,7,TFT_RED);//20%
					colorchangEvent = true;
				}
				prevstate = bat5_state;
			}//

			else if(bat6 == true && prevstate!=bat6_state){
				clearAllbatGauage();
				TFT_fillRoundRect(130 ,190,60,5,1,TFT_RED);//14%
				prevstate = bat6_state;
			}

			else if(bat7 == true && prevstate!=Empty){
				clearAllbatGauage();
				clear_batGauage(130, 190, 60, 5, 1);
				prevstate = Empty;
			}

		}//end

		if(sysState !=Charging)
			prevcharge = clear_charge;

		if(sysState == Charging){

			prevstate = clear_state;
			if(prevcharge == clear_charge)
				clearAllbatGauage();

			bool seg5_6 = (batteryPercentage >=10  && batteryPercentage <=25) ? true : false;
			bool seg5_4 = (batteryPercentage >= 26 && batteryPercentage <=45) ? true : false;
			bool seg5_3 = (batteryPercentage >= 46 && batteryPercentage <=65) ? true : false;
			bool seg5_2 = (batteryPercentage >= 66 && batteryPercentage <85) ? true : false;
			bool seg5_1 = (batteryPercentage >= 86 && batteryPercentage <=100) ? true : false;

			if(!seg5_6)
				bat6 = false;
			if(seg5_6)
				bat6 = true;

			else if(seg5_4)
				bat5 = true;

			else if(seg5_3)
				bat5 = bat4 = true;

			else if(seg5_2)
				bat5 = bat4 = bat3 = true;

			else if(seg5_1)
				bat5 = bat4 = bat3 = bat2 = true;

			if(seg5_6 ){
				bat5 = !bat5;
				if(bat5)
					TFT_fillRoundRect(120, 142, 80, 23, 7, TFT_GREEN);

				else
					clear_batGauage(120 ,142,80,23,7);

				if(bat6 && prevcharge != bat6_charge){
					TFT_fillRoundRect(120, 167, 80, 23, 7, TFT_GREEN);
					prevcharge = bat6_charge;
				}
			}//

			else if(seg5_4){
				bat4 = !bat4;
				if(bat4)
					TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%

				else clear_batGauage(120 ,117,80,23,7);//

				if(bat5 && prevcharge != bat5_charge){
					TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
					TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
					prevcharge = bat5_charge;
				}
			}//

			else if(seg5_3){
				bat3 = !bat3;
				if(bat3)
					TFT_fillRoundRect(120 ,92,80,23,7,TFT_GREEN);

				else clear_batGauage(120 ,92,80,23,7);

				if(bat5 && bat4 && prevcharge != bat4_charge){
					TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%
					TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
					TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
					prevcharge = bat4_charge;
				}
			}//

			else if(seg5_2){
				bat2 = !bat2;
				if(bat2)
					TFT_fillRoundRect(120 ,67,80,23,7, TFT_GREEN);
	
				else clear_batGauage(120 ,67,80,23,7);

				if(bat5 && bat4 && bat3 && prevcharge != bat3_charge){
					TFT_fillRoundRect(120 ,92,80,23,7,TFT_GREEN); //80%
					TFT_fillRoundRect(120 ,117,80,23,7,TFT_GREEN);//60%
					TFT_fillRoundRect(120 ,142,80,23,7,TFT_GREEN);//40%
					TFT_fillRoundRect(120 ,167,80,23,7,TFT_GREEN);//20%
					prevcharge = bat3_charge;
				}
			}//

			else if(seg5_1 && prevcharge == bat2_charge){
				fillBat();
				prevcharge = bat2_charge;

			}//	

		}//end

		else if(sysState == Overload){
			ClearScreen;
			_fg = TFT_RED;
			TFT_setFont(TestFont, NULL);
			TFT_print("Overload", CENTER, CENTER);
			sysState = None;
		}

		else if(sysState == BatteryLow){
			ClearScreen;
			_fg = TFT_RED;
			TFT_setFont(TestFont, NULL);
			TFT_print("BatterylOW", CENTER, CENTER);
			sysState = None;
		}//

		delay(500);
	}//
}//

#endif //SCREENFUNCTIONS_H
