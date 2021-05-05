#ifndef SCREENFUNCTIONS_H
#define SCREENFUNCTIONS_H

#include "SysteMeasurements.h"

#define SPI_BUS TFT_HSPI_HOST

int PrevLoad = -1, PrevSmps = -1, prevSolar = -1, PrevBa3Percentage = 0;
bool loadAnimate = false, solarAnimate = false, smpsAnimate = false;

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

// void clear_batStates(bool &b1,bool &b2, bool &b3, bool &b4, bool &b5, bool &b6){
// 	b1 = b2 = b3 = b4 = b5 = b6 = false;
// }

//display images
void drawImages(){

	TFT_jpg_image(270, 110,3, SPIFFS_BASE_PATH "/images/AcOutput.jpg", NULL, 0);
	TFT_jpg_image(15, 105,3, SPIFFS_BASE_PATH "/images/GridCharger.jpg", NULL, 0);

	TFT_drawRoundRect(110 ,50,100,150, 20, TFT_DARKGREY);
	TFT_fillRoundRect(110 ,50,100,150, 20, TFT_DARKGREY);
	TFT_fillRoundRect(115 ,55,90,140, 20, TFT_WHITE	);

	
	TFT_drawRoundRect(139, 39, 40, 10, 0, TFT_DARKGREY);
	TFT_fillRoundRect(139, 39, 40, 10, 0, TFT_DARKGREY);
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
		}//

		else 
			TFT_drawRoundRect(265, 105, 33,33, 10, TFT_BLACK);

		if(SmpsPwr>=5){
			smpsAnimate = !smpsAnimate;
			if(smpsAnimate)
				TFT_drawRoundRect(10, 100, 33,35, 10, TFT_WHITE);
			else 
				TFT_drawRoundRect(10, 100, 33,35, 10, TFT_BLACK);
		}//

		else 
			TFT_drawRoundRect(10, 100, 33,35, 10, TFT_BLACK);


		if(LoadPwr!=PrevLoad){ //load
			_fg = TFT_GREEN;
			_bg = TFT_BLACK;
			TFT_fillRect(265, 145, 255,38, TFT_BLACK);
			TFT_print(loadtemp,265 , 145);
			PrevLoad = LoadPwr;
		}//

		if(SmpsPwr!=PrevSmps){ //smps
			_fg = TFT_WHITE;
			_bg = TFT_BLACK;
			TFT_fillRect(15, 140, 255,38, TFT_BLACK);
			TFT_print(smpstemp,15,140);
			PrevSmps = SmpsPwr;
		}//

}//

static void animateShow(){}

static void ScreenUpdateTask(){

	bool bat1 = false,bat2=false,bat3=false,bat4=false,bat5=false,bat6=false;
	while (1){

		if(defaultLoad){

			ClearScreen;
			drawImages();
			// TFT_drawRoundRect(42, 35, 25, 30, 5, TFT_WHITE);
			// TFT_fillRoundRect(42, 35, 25, 30, 5, TFT_WHITE);

			// TFT_drawRoundRect(20, 50, 70, 130, 5, TFT_WHITE);
			// TFT_fillRoundRect(20, 50, 70, 130, 5, TFT_WHITE);
			defaultLoad = false;
			prevSolar = PrevLoad = PrevSmps = -1;
			sysState = Normal;
		}
/**
 * 12.5 13 14 15.3
 * 
*/

		// if(sysState == Charging || sysState == Normal)
			// UpdateVal();

	if(sysState == Overload){
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

	delay(400);
	}
}//

#endif //SCREENFUNCTIONS_H
