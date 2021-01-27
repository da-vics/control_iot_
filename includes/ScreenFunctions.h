#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "esp_spiffs.h"

#include "esp_log.h"

#define TurnOffScreen gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
#define TurnOnScreen gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
#define ClearScreen TFT_fillScreen(TFT_BLACK);
#define delay(milli) vTaskDelay(milli / portTICK_RATE_MS);
#define SPI_BUS TFT_HSPI_HOST

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
	TFT_print("The Future", CENTER, 90);
    delay(1000);
    ClearScreen;
    TurnOffScreen;
}
