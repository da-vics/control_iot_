//basic system header
#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <esp_intr_alloc.h>

//basic FreeRTOS headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "driver/can.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "driver/uart.h"

//#ifdef CONFIG_EXAMPLE_USE_WIFI
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_attr.h"
#include <sys/time.h>
#include <unistd.h>
#include "sntp/sntp.h"

#include "lwip/apps/sntp.h"
#include "driver/rmt.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include <ctype.h>

//macro definition
#define CAN_TX 5
#define CAN_RX 4
#define LED_PIN 27
#define BUTTON_PIN 34
#define OVERLOAD_TRIGGER_PIN 17
#define TAG "wifi"

#define POWER_CONTROL_ID 0xA0
#define inverterDATA_MODE_ID 0xA1
#define pcDATA_MODE_ID 0xA2
#define bmsDATA_MODE_ID 0xA3
#define pcDATA_MODE_ID2 0xA4
#define DIAGNOSTIC_MODE_ID 0xA5
#define PC_SYSTEM_DATA_ID 0xB0
#define BMS_SYSTEM_DATA_ID 0xB1
#define BMS_DIAGNOSTIC_MODE_ID 0xB2
#define INVERTER_SYSTEM_DATA_ID 0xB3
#define PC_DATA_2 0xB4
#define PC_DIAGNOSTIC 0xB5

#define ENERGY_REFERENCE 0.00000833 //  * 30/3600 /1000

#define OVERLOAD 4500
#define BATTERY_LOW_CHARGEUP 340  /// CHECK
bool batteryLOWShutdown = false, warningDisplay = false;

#define CONFIG_WIFI_SSID "photizzo"
#define CONFIG_WIFI_PASSSWORD "photizzo"///new 
#define BIT_0 (1 << 0)

#define ConfigMessage "WATTBANK_SETUP_SELF"
#define ProcessingMessage "WATTBANK_SETUP_INITIALISING"

#define SYSTEM_ON_COMMAND 0x01
#define SYSTEM_OFF_COMMAND 0x00

#define BUTTON_PRIORITY 10
#define CAN_TRANSMIT_PRIORITY 8
#define CAN_RECIEVE_PRIORITY 12
#define SYSTEM_CONTROL_PRIORITY 11
#define SCREEN_PRIORITY 12
#define TIMER_TASK_PRIORITY 10
#define RX_PRIORITY 11

#define INCLUDE_vTaskSuspend 1

#define SPI_BUS TFT_HSPI_HOST

#define CONFIG_EXAMPLE_USE_WIFI 1

#define TXD_PIN (19)
#define RXD_PIN (21)

#define BAT_LOW_CONTROL 23
#define REMOTE_CHARGER_CONTROL 22

#define PROJECT_TAG "wattbank D"

//RMT SETUP

// Configure these based on your project needs ********
#define LED_RMT_TX_CHANNEL RMT_CHANNEL_0
#define LED_RMT_TX_GPIO 18

#define BITS_PER_LED_CMD 24
#define LED_BUFFER_ITEMS ((NUM_LEDS * BITS_PER_LED_CMD))

// These values are determined by measuring pulse timing with logic analyzer and adjusting to match datasheet.
#define T0H 14 // 0 bit high time
#define T1H 36 //52  // 1 bit high time
#define T0L 36 //52  // low time for either bit
#define T1L 14

#define RED 0x006400
#define ORANGE 0x426400
#define YELLOW 0x646400
#define GREEN 0x640000
#define BLUE 0x000064
#define INDIGO 0x003264
#define VIOLET 0x306464
#define OFF 0x000000

uint32_t rainbow_sequence[11] ={ OFF, OFF, RED, ORANGE, YELLOW, GREEN, BLUE, INDIGO, VIOLET, OFF, OFF };

#ifndef NUM_LEDS
#define NUM_LEDS 3
#endif

#define DATATX 26
#define DATARX 25

 int8_t counter = 0;
	int8_t Gridcounter = 0;
	int8_t Solarcounter = 0;

struct led_state new_state;
// This is the buffer which the hw peripheral will access while pulsing the output pin
rmt_item32_t led_data_buffer[LED_BUFFER_ITEMS];

// This structure is used for indicating what the colors of each LED should be set to.
// There is a 32bit value for each LED. Only the lower 3 bytes are used and they hold the
// Red (byte 2), Green (byte 1), and Blue (byte 0) values to be set.
struct led_state
{
	uint32_t leds[NUM_LEDS];
};

/*   mqtt details*/
char *mqttURL, *mqttUsername, *mqttPassword, *mqttPort;
uint8_t Data_update = 0;
bool newmqttDetail_available = true;
char *dumpMQTT;

static EventGroupHandle_t s_wifi_event_group;

//enumeration declaration
typedef enum
{
	TX_SYSTEM_POWER_CONTROL,
	TX_PC_DATA_REQUEST,
	TX_BMS_DATA_REQUEST,
	TX_INVERTER_DATA_REQUEST,
	TX_PC2_DATA_REQUEST,
	TX_DIAGNOSTIC,
} tx_task_action;

typedef enum
{
	SCREEN_ON,
	SCREEN_OFF,
	DATA_UPDATE,
	WARNING,
} screen_action_t;

typedef struct details
{
	char *username;
	char *password;
};

typedef enum
{
	RX_DATA_MODE,
	RX_PC_DATA_REQUEST,
	RX_BMS_DATA_REQUEST,
	RX_INVERTER_DATA_REQUEST,
	RX_PC2_DATA_REQUEST,
	RX_DIAGNOSTIC,
} rx_task_action;	

typedef enum
{
	SYSTEM_OFF = 0,
	SYSTEM_ON,
	SYSTEM_DIAGNOSTIC_MODE,
} system_mode_t;

typedef enum
{
	SYSTEM_NOT_CHARGING = 0,
	SYSTEM_CHARGING,
} system_subMode_t;

typedef enum
{
	BAT_NORM = 0,
	LOW_BATTERY,
	BATTERY_FULL,
} battery_status_t;

typedef enum
{
	WATTBANK = 0,
	GRID_SUPPLY,
} powerSource_t;

typedef enum
{
	WARNING_NORM = 0,
	LOW_BATTERY_WARNING,
	THERMAL_SHUTDOWN,
	OVERLOADS,
} warning_status_t;

nvs_handle gateway_flash_data;
struct details detail;
warning_status_t warning_status;
system_subMode_t system_subMode;
powerSource_t powerSource;

typedef struct
{
	uint8_t bms_system_status;
	uint8_t batteryPercentage;
	uint8_t timeLeftMinutes;
	uint8_t timeLeftHours;
	int16_t batteryVoltage;
} bms_data_structure_t;

typedef struct
{
	uint8_t inverterStatus;
	int16_t outputVoltage;
	float outputCurrent;
	uint8_t runTimeHours;
	uint8_t runTimeMinutes;
} inverter_data_structure_t;

typedef struct
{
	int16_t solarCurrent;
	int16_t smpsCurrent;
	int16_t loadCurrent;
	int16_t batteryPower;
	int16_t busVoltage;
	int16_t c_d_current;
	int8_t charging_status;
} pc_data_structure_t;

typedef struct
{
	uint8_t solar2home;
	uint8_t solar2wattbank;
	uint8_t grid2home;
	uint8_t grid2wattbank;
	uint8_t wattbank2home;
	uint8_t gridsense;
} PowerGraph;
PowerGraph powerGraph;

typedef union
{
	float floattype;
	uint8_t int_format[4];
} currentData;

static const int RX_BUF_SIZE = 1024;

// uint8_t wifi_username[31];
// uint8_t wifi_password[60];
bool newWifi = false;

bool warningStatus = false;
float solarPower, smpsPower, loadPower, batteryPowerS;
int16_t solarCurrent, smpsCurrent, loadCurrent, prevLoadCurrent , batteryPower, busVoltage, c_d_current, charging_status;
float energyConsumption = 0.0;
uint32_t columbCount = 0.0;
uint8_t runtimehr = 0, runtimemin = 0, timelefthr = 0, timeleftmin = 0;
bool time_synchronized = false;
bool chargerStatus = false;

//instance declaration
esp_mqtt_client_handle_t client;
static QueueHandle_t tx_Queue;
static QueueHandle_t rx_Queue;
static QueueHandle_t screen_Queue;
static SemaphoreHandle_t settingTime, wifiReset, mqttDetailUpdate;

static TaskHandle_t xHandletime, xHandlewifi, xHandlemqtt;

bool overloadTrigger = false;

bool nvs_started;
pc_data_structure_t pc_data;
bms_data_structure_t bms_data;
inverter_data_structure_t inverter_data;
system_mode_t systemMode;

//variable declaration
uint8_t solarStatus=0, smpsStatus=0, chargingStatus=0, powerFlowGraph =0;
uint8_t PcCommData[1500];
uint8_t data[200];
char newData[210];

uint8_t powerCommand;
bool system_state = false;
uint8_t bmsStatus;
int energyConsumed = 30000;

uint8_t batteryChargingStatus;
char outputVoltage_char[8], 
load_char[8], 
runTime_char[8], 
timeLeft_char[8],
solarPower_char[8], 
batteryPower_char[8], 
energyConsumed_char[8],
smpsPower_char[8],
tempSolarFix_char[8];

char systemPercentage_char[5], systemState[4], warningChar[15];

color_t solar2wattbank, solar2home, grid2wattbank, grid2home, wattbank2home;

uint8_t inverterStatus;
int16_t outputVoltage;
bool upsmode;
float outputCurrent;

int32_t runTimeHours = 0;
int8_t runTimeMinutes =0;

int32_t GridrunTimeHours = 0;
int8_t GridrunTimeMinutes = 0;

int32_t SolarRunTimeHours = 0;
int8_t SolarRunTimeMinutes = 0;

uint8_t batteryPercentage = 0;
uint8_t timeLeftMinutes = 0;
uint8_t timeLeftHours = 0;
bool mqtt_connection = false, wifiConnectionStatus = false;
char *incomingData;
// char defaultconfig[35] = "$username:phottizzo&teamphotizzo:";

typedef struct
{
	uint8_t data0;
	uint8_t data1;
	uint8_t data2;
	uint8_t data3;
	uint8_t data4;
	uint8_t data5;
	uint8_t data6;
	uint8_t data7;

} IncomingUartData;

uint8_t data0, data1, data2, data3, data4, data5, data6, data7;

char strftime_buf[25];
time_t now = 0;
struct tm timeinfo ={ 0 };
struct timeval time_value;

//uint8_t pcSolarH, pcSolarL, pcsmpsH, pcsmpsL, pcloadH, pcloadL, pcVoltH,
//	pcVoltL;
//bool configured = false;
char *subscribeTopic = "/SUB/WATTBANKM2_2", *publishTopic = "/PUB/WATTBANKM2_2";
//==================================================================================
//#ifdef CONFIG_EXAMPLE_USE_WIFI

static const char tag[] = "[TFT Demo]";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
 but we only care about one event - are we connected
 to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;

//------------------------------------------------------------

void Solar_fix(char *temp_data)
{
		for(int i = 0; i<8; ++i)
		{
			temp_data[i] = ' '; 
		}
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{

	switch (event->event_id)
	{
	case SYSTEM_EVENT_STA_START:
	{
		esp_wifi_connect();
		break;
	}
	case SYSTEM_EVENT_STA_GOT_IP:
	{
		ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
		xEventGroupSetBits(s_wifi_event_group, BIT_0);
		wifiConnectionStatus = true;
		if (!time_synchronized)
			xSemaphoreGive(settingTime);
		break;
	}

	case SYSTEM_EVENT_STA_DISCONNECTED:
	{
		wifiConnectionStatus = false;
		esp_wifi_connect();
		xEventGroupClearBits(s_wifi_event_group, BIT_0);
		break;
	}

	default:
		break;
	}
	return ESP_OK;
}

//remote control
void remoteON(void)
{
	gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
	system_state = true;
	systemMode = SYSTEM_ON;
	gpio_set_level(LED_PIN, 1);
	powerCommand = SYSTEM_ON_COMMAND;

	ESP_LOGI("Remote", "%s", "REMOTE ON");

	if (warningDisplay)
	{
		system_state = false;
		warningDisplay = false;
		warningStatus = false;
		warning_status = WARNING_NORM;
		batteryLOWShutdown = false;
		powerCommand = SYSTEM_OFF_COMMAND;
		///screenAction = SCREEN_OFF;
		systemMode = SYSTEM_OFF;
	}
}

void remoteOFF(void)
{
	ESP_LOGI("Remote", "%s", "REMOTE OFF");
	tx_task_action action;
	gpio_set_level(LED_PIN, 0);
	system_state = false;
	systemMode = SYSTEM_OFF;
	powerCommand = SYSTEM_OFF_COMMAND;
	gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
	energyConsumption = 0.0;
	///ws2812_show(OFF);
	runTimeHours = 0;
	runTimeMinutes = 0;
}

//------------------WIFI config-
void wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	wifi_config_t wifi_config =
	{
		.sta ={ .ssid = CONFIG_WIFI_SSID,
		.password = CONFIG_WIFI_PASSSWORD,
		.bssid_set = false },
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSSWORD);
	xEventGroupWaitBits(s_wifi_event_group, BIT_0, false, true, 5000 / portTICK_PERIOD_MS);
}

void wifi_init_()
{

	tcpip_adapter_init();
	s_wifi_event_group = xEventGroupCreate();
	//ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	wifi_config_t wifi_config ={
		.sta ={ .ssid = " ", .password = " ", .bssid_set = false },
	};

	uint8_t username_len = strlen(detail.username);
	uint8_t password_len = strlen(detail.password);
	memcpy(wifi_config.sta.ssid, detail.username, username_len);
	ESP_LOGI(TAG, "connect to ap SSID:%s,%i password:%s", wifi_config.sta.ssid, username_len, wifi_config.sta.password);
	memcpy(wifi_config.sta.password, detail.password, password_len);
	wifi_config.sta.bssid_set = false;
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
	xEventGroupWaitBits(s_wifi_event_group, BIT_0, false, true, 8000 / portTICK_PERIOD_MS);
}

void newWIFI_connection(void)
{
	detail.username = strtok(NULL, "&");
	if (detail.username != NULL)
	{
		detail.password = strtok(NULL, ":");
		//printf("%s,%s,", detail.username, detail.password);
		ESP_ERROR_CHECK(esp_wifi_stop());
		ESP_LOGI(TAG, "STOPPED WIFI");
		xSemaphoreGive(wifiReset);
	}
	//newWifi = true;
}
bool mqttG = false;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;

	switch (event->event_id)
	{
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		msg_id = esp_mqtt_client_publish(client, publishTopic, "CONNECTED", 0, 1, 0);
		ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		msg_id = esp_mqtt_client_subscribe(client, subscribeTopic, 0);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
		mqtt_connection = true;
		break;

	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		mqtt_connection = false;
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		msg_id = esp_mqtt_client_publish(client, "/connectiontest", "data", 0, 0, 0);
		ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		mqtt_connection = true;
		break;

	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		mqtt_connection = true;
		break;

	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		mqtt_connection = true;
		break;

	case MQTT_EVENT_DATA:
		switch (event->data[0])
		{
		case '1':
		{
			remoteON();
			msg_id = esp_mqtt_client_publish(client, "/STA/WATTBANKM2_2", "1", 0, 0, 0); /// test
			break;
		}
		case '0':
		{
			remoteOFF();
			msg_id = esp_mqtt_client_publish(client, "/STA/WATTBANKM2_2", "0", 0, 0, 0); /// test
			break;
		}
		case 'R':
		{
			gpio_set_level(REMOTE_CHARGER_CONTROL, 1);
			chargerStatus = true;
			msg_id = esp_mqtt_client_publish(client, "/STA/WATTBANKM2_2", "ChargerOn", 0, 0, 0); /// test
			break;
		}

		case 'C':
		{
			gpio_set_level(REMOTE_CHARGER_CONTROL, 0);
			chargerStatus = false;
			msg_id = esp_mqtt_client_publish(client, "/STA/WATTBANKM2_2", "ChargerOFF", 0, 0, 0); /// test
			break;
		}

		case '$':
		{
			incomingData = event->data;
			incomingData = strtok(incomingData, ":");
			if (incomingData != NULL)
				newWIFI_connection();
			break;
		}
		case 'm':
		{
			dumpMQTT = event->data;
			mqttG = true;
			break;
		}
		case '*':
		{
			char *configurationData, dump[10], newSubscribeTopic[30];
			configurationData = event->data;
			sscanf(configurationData, "%s %s %s", dump, publishTopic, newSubscribeTopic);

			msg_id = esp_mqtt_client_unsubscribe(client, subscribeTopic);

			msg_id = esp_mqtt_client_subscribe(client, newSubscribeTopic, 0);
			strcpy(subscribeTopic, newSubscribeTopic);
			//configured = true;
			Data_update = 1;
			break;
		}
		default:
		{
			break;
		}
		}
		break;

	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		mqtt_connection = false;
		break;

	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
	return ESP_OK;
}

//-----------------------------sntp operation--
static void initialize_sntp(void)
{
	ESP_LOGI(tag, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_init();
}

//--------------------------
static int obtain_time(void)
{
	int res;
	initialize_sntp();
	int retry = 0;
	const int retry_count = 10;
	time(&now);
	localtime_r(&now, &timeinfo);

	while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
	{

		if (retry==8)  retry= 0;
		ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}

	if (timeinfo.tm_year < (2016 - 1900))
	{
		ESP_LOGI(tag, "System time NOT set.");
		time_synchronized = false;
		res = 0;
	}
	else
	{
		ESP_LOGI(tag, "System time is set.");
		time_synchronized = true;
		res = 1;
		setenv("TZ", "GMT-1", 1);
		tzset();
	}
	//ESP_ERROR_CHECK( esp_wifi_stop() );
	return res;
}

//--------------------------serial communication initialization------
void init(void)
{
	const uart_config_t uart_config ={

		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE };

	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);

	const uart_config_t uart_config2 =
	{
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE };

	uart_param_config(UART_NUM_2, &uart_config2);
	uart_set_pin(UART_NUM_2, DATATX, DATARX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

//**********************CAN Data checksum arithemtic to eliminate noise in the data**************//
uint8_t computeChecksum()
{
	uint8_t result, val;
	result = data0 + data1 + data2 + data3 + data4 + data5 + data6;
	val = ((int)result / 255) + (result % 255);
	return val;
}

uint8_t computeCheckUart(IncomingUartData *processDat)
{
	uint8_t result, val;
	result = processDat->data0 + processDat->data1 + processDat->data2 +
		processDat->data3 + processDat->data4 + processDat->data5 +
		processDat->data6;

	val = ((int)result / 255) + (result % 255);
	return val;
}

void resetParams()
{
	energyConsumption = 0;
	loadPower = 0.0;
	loadCurrent = 0;
	sprintf(load_char, "%iW", (int)loadPower);
}

void process(const int rxBytes)
{
	uint8_t dataChecksum;
	uint8_t dataChecksum2;

	IncomingUartData pcUartData1;
	IncomingUartData pcUartData2;

	int set = 0;
	bool found = false;

	for (int i = 0; i < rxBytes; ++i)
	{
		if (PcCommData[i] == '~')
		{
			set = i;
			found = true;
			break;
		}
	} ///

	bool found2 = false;
	for (int i = set; i < rxBytes; ++i)
	{
		if (PcCommData[i] == '!')
		{
			found2 = true;
			break;
		}
	}

	bool found3 = false;
	for (int i = set; i < rxBytes; ++i)
	{
		if (PcCommData[i] == '&')
		{
			found3 = true;
			break;
		}
	}

	bool stopProcessing = false;

	if (found && found2 && found3)
	{
		int Getdata[20];
		int setpos = 0;

		int second_step = 0;

		for (int i = set; i < rxBytes; ++i)
		{
			if (stopProcessing)
			{
				break;
			}

			if (PcCommData[i] == '!')
			{
				second_step = i;
				break;
			}

			if (PcCommData[i] == ',' && PcCommData[i + 1] == '!')
			{
				second_step = i + 1;
				break;
			}

			if (PcCommData[i] == ',')
			{
				char temp[5];
				for (int ii = 0; ii < 5; ++ii)
				{
					temp[ii] = 0;
				}

				int t = 0;

				for (int j = i + 1; j < rxBytes; ++j)
				{

					if (PcCommData[j] == ',')
						break;

					if (isdigit(PcCommData[j]))
					{
						temp[t] = (char)PcCommData[j];
						++t;
					}

					else
					{
						stopProcessing = true;
						break;
					}
				}

				if (!stopProcessing)	
				{
					Getdata[setpos] = atoi(temp);
					++setpos;
				}
			}
		}

		if (!stopProcessing)
		{
			for (; second_step < rxBytes; ++second_step)
			{
				if (stopProcessing)
				{
					break;
				}

				if (PcCommData[second_step] == '&')
					break;

				if (PcCommData[second_step] == ',' && PcCommData[second_step + 1] == '&')
					break;

				if (PcCommData[second_step] == ',')
				{
					char temp[5];
					for (int ii = 0; ii < 5; ++ii)
					{
						temp[ii] = 0;
					}

					int t = 0;

					for (int j = second_step + 1; j < rxBytes; ++j)
					{

						if (PcCommData[j] == ',')
							break;

						if (isdigit(PcCommData[j]))
						{
							temp[t] = (char)PcCommData[j];
							++t;
						}

						else
						{
							stopProcessing = true;
							break;
						}
					}///

					if (!stopProcessing)
					{
						Getdata[setpos] = atoi(temp);
						++setpos;
					}
				}
			}
		}

		if (stopProcessing)
		{
			printf("UART: %s\n", "Char Found");
		}///

		else if (!stopProcessing)
		{
			printf("UART: %s\n", "Passed");
		}///

		pcUartData1.data0 = Getdata[0];
		pcUartData1.data1 = Getdata[1];
		pcUartData1.data2 = Getdata[2];
		pcUartData1.data3 = Getdata[3];
		pcUartData1.data4 = Getdata[4];
		pcUartData1.data5 = Getdata[5];
		pcUartData1.data6 = Getdata[6];
		pcUartData1.data7 = Getdata[7];

		pcUartData2.data0 = Getdata[8];
		pcUartData2.data1 = Getdata[9];
		pcUartData2.data2 = Getdata[10];
		pcUartData2.data3 = Getdata[11];
		pcUartData2.data4 = Getdata[12];
		pcUartData2.data5 = Getdata[13];
		pcUartData2.data6 = Getdata[14];
		pcUartData2.data7 = Getdata[15];

		dataChecksum = computeCheckUart(&pcUartData1);
		dataChecksum2 = computeCheckUart(&pcUartData2);

		int16_t tempbusvoltage = ((pcUartData1.data0 << 8) | pcUartData1.data1); /// new fix

		int tempPercentage = pcUartData1.data5;

		if (tempPercentage >= 0 && tempPercentage <= 100 && tempbusvoltage>=0 && tempbusvoltage<=500)
		{
			int16_t loadcurrentTemp = 0;
			int16_t soloarcurrentTemp = 0;

			bool processDat = true;
			uint8_t tempFLow = pcUartData1.data4;

			if((tempFLow & (0x01 << 6)))
			{
				if(!(tempFLow & (0x01 << 2)))
					processDat = false;
			}//

			if((tempFLow & (0x01 << 2)) && (tempFLow & (0x01 << 6)) == 0)
			{
				processDat = false;
			}

			if(tempbusvoltage>=350 && (tempFLow & (0x01 << 7)))
			{
				processDat = false;
			}

			if(tempbusvoltage>=350 && tempPercentage <=0)
			{
				processDat = false;
			}

			if (processDat == true && dataChecksum == pcUartData1.data7 && pcUartData1.data6 == 0)
			{
				
			loadcurrentTemp = ((pcUartData1.data2 << 8) | pcUartData1.data3);
			busVoltage = tempbusvoltage;

			float temploadpwr = ((float)(loadcurrentTemp) / 1000.0) * (float)busVoltage;

				if(temploadpwr<=(float)10000.0)
			{

			if(loadcurrentTemp>=(int16_t)0 && loadcurrentTemp<=(int16_t)30000)  /// new fix
			{
				powerFlowGraph = tempFLow;
				batteryPercentage = pcUartData1.data5;

					if(temploadpwr>=(float)0.0 && temploadpwr<=(float)10000.0)
				{
					if(loadcurrentTemp>=(int16_t)0 && loadcurrentTemp<=(int16_t)30000)
					{
					loadCurrent = loadcurrentTemp;
					}

					else
					{
						loadCurrent = loadCurrent;
					}
				} ///
			}///

			}//end

			}//

			if (dataChecksum2 == pcUartData2.data7 && pcUartData2.data6 == 1)
			{
				soloarcurrentTemp = ((pcUartData2.data2 << 8) | pcUartData2.data3); /// fixed test bug!
			
				if(soloarcurrentTemp>=0 && soloarcurrentTemp<=15000)
				{
				smpsCurrent = ((pcUartData2.data0 << 8) | pcUartData2.data1);
				c_d_current = ((pcUartData2.data4 << 8) | pcUartData2.data5);
				
				float tempsolarpwr = (((float)(soloarcurrentTemp) / 1000.0) * (float)busVoltage);

				if(tempsolarpwr>=(float)0.0 && tempsolarpwr <= (float)6000.0)
				{
					solarCurrent = soloarcurrentTemp;
				}///

				}///
			} ///

			else if (dataChecksum != pcUartData1.data7 || dataChecksum2 != pcUartData2.data7)
			{
				// printf("UART:  %s\n", "Communication Error!");
			}
		}///

		else
		{
			// printf("UART:  %s\n", "data Error!");
		}

	} ///

	memset(PcCommData, 0, sizeof(PcCommData)); /// new

} ///end case

static void DataRx_Task(void *args)
{
	while (1)
	{
		const int rx = uart_read_bytes(UART_NUM_2, PcCommData, RX_BUF_SIZE * 2, 1000 / portTICK_RATE_MS);
		if (rx > 0)
		{
			process(rx);
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void rx_task(void *arg)
{

	static const char *RX_TASK_TAG = "RX_TASK";
	esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
	//uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
	char *year_c, *month_c, *day_c, *hour_c, *minutes_c, *second_c;

	while (1)
	{
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);

		//printf("runrx task");
		if (rxBytes > 0)
		{
				switch (data[0])
				{

				case '!':
				{
					if (!mqtt_connection)
					{
						if (data[1] == '&' && data[2] == '1')
							remoteON();

						else if (data[1] == '&' && data[2] == '0')
							remoteOFF();

						else if(data[1] == '&' && data[2] == 'R')
						{
							gpio_set_level(REMOTE_CHARGER_CONTROL, 1);
							chargerStatus = true;
						}

						else if(data[1] == '&' && data[2] == 'C')
						{
							gpio_set_level(REMOTE_CHARGER_CONTROL, 0);
							chargerStatus = false;
						}
					}
					break;
				}

				case '#':
				{
					char buffer[30], *dump;
					memcpy(buffer, data, rxBytes);
					dump = buffer;
					dump = strtok(dump, "#");
					year_c = strtok(dump, "/");
					month_c = strtok(NULL, "/");
					day_c = strtok(NULL, ",");
					hour_c = strtok(NULL, ":");
					minutes_c = strtok(NULL, ":");
					second_c = strtok(NULL, "+");

					// printf("%s,%s,%s,%s,%s,%s",year_c,month_c,day_c,hour_c,minutes_c,second_c);
					timeinfo.tm_year = (2000 + atoi(year_c)) - 1900;
					timeinfo.tm_mon = atoi(month_c) - 1;
					timeinfo.tm_mday = atoi(day_c);
					timeinfo.tm_hour = atoi(hour_c);
					timeinfo.tm_min = atoi(minutes_c);
					timeinfo.tm_sec = atoi(second_c);
					timeinfo.tm_isdst = 0;
					timeinfo.tm_wday = 1;
					time_value.tv_sec = mktime(&timeinfo);
					time_value.tv_usec = 0;
					settimeofday(&time_value, 0);
					time(&now);
					localtime_r(&now, &timeinfo);
					strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
					ESP_LOGI(TAG, "The current date/time in Nigeria is: %s", strftime_buf);
					time_synchronized = true;
					break;
				}

				default:
					break;
				}

		}
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

void data_formatting()
{
	solarPower = (((float)(solarCurrent) / 1000.0) * (float)busVoltage);
	smpsPower = (((float)(smpsCurrent) / 1000.0) * (float)busVoltage);
	loadPower =	 ((float)(loadCurrent) / 1000.0) * (float)busVoltage;
	batteryPowerS = loadPower - (solarPower + smpsPower);
	energyConsumption += loadPower;

	if (batteryPowerS < 0.0)
		batteryPowerS = 0.0;

	if (powerSource == GRID_SUPPLY || systemMode == SYSTEM_OFF)
	{
		loadPower = 0.0;
		loadCurrent = 0;
	}

	else if(powerSource == WATTBANK || powerGraph.grid2wattbank ==0)
	{
		smpsCurrent = 0;
		smpsPower = 0;
	}

	bool proceed = true;

	if(loadPower>(float)10000.0 || solarPower>(float)6000.0)
	{
		proceed = false;
	}

	if(loadPower<(float)0.0 || solarPower<(float)0.0)
	{
		proceed = false;
	}

	if (proceed == true)
	{
		Solar_fix(solarPower_char);
		Solar_fix(tempSolarFix_char);

	sprintf(systemPercentage_char, "%i%%", batteryPercentage);
	sprintf(load_char, "%iW", (int)loadPower);
	sprintf(runTime_char, "%i:%i", runTimeHours, runTimeMinutes);
	sprintf(solarPower_char, "%iW", (int)solarPower);
	sprintf(smpsPower_char, "%iW", (int)smpsPower);
	sprintf(batteryPower_char, "%iW", (int)batteryPowerS);
}//

}

void userDataPage(void)
{
	TFT_setFont(DEJAVU18_FONT, NULL);
	const uint8_t cl = 12;
	TFT_fillRect(190, 20, 190 + cl, 20, TFT_BLACK);
	TFT_fillRect(10, 70, 10 + cl, 20, TFT_BLACK);
	TFT_fillRect(250, 70, 250 + cl, 20, TFT_BLACK);
	TFT_fillRect(190, 220, 190 + cl, 220, TFT_BLACK);
	TFT_fillRect(190, 200, 190 + cl, 200, TFT_BLACK);
	_fg = TFT_RED;
	TFT_print(load_char, 190, 20);
	//TFT_print(timeLeft_char,65,220);

	TFT_print(tempSolarFix_char, 10, 70);
	TFT_print(solarPower_char, 10, 70);
	TFT_print(smpsPower_char, 250, 70);

	//TFT_print(batteryPower_char, 190, 220);
	_fg = TFT_BLUE;
	//TFT_print(runTime_char,65,190);
	TFT_print(systemPercentage_char, 190, 200);

	if (systemMode == SYSTEM_ON)
		sprintf(systemState, "ON");
	else
		sprintf(systemState, "OFF");

	TFT_print(systemState, 140, 185);
}

static void mainPage()
{
	userDataPage();

	TFT_setFont(DEJAVU18_FONT, NULL);

	TFT_jpg_image(CENTER, 10, 2, SPIFFS_BASE_PATH "/images/home.jpg", NULL, 0);

	TFT_jpg_image(0, CENTER, 2, SPIFFS_BASE_PATH "/images/solar.jpg", NULL, 0);

	TFT_jpg_image(240, CENTER, 2, SPIFFS_BASE_PATH "/images/grid.jpg", NULL, 0);

	TFT_jpg_image(CENTER, 175, 2, SPIFFS_BASE_PATH "/images/wattbank.jpg", NULL, 0);

	TFT_setFont(DEFAULT_FONT, NULL);
	//TFT_print("RUN TIME",55,175);
	//TFT_print("TIME LEFT",55,208);
	TFT_print("LOAD", CENTER, 0);
	TFT_print("GRID", 265, 155);
	TFT_print("SOLAR", 10, 155);
}

void welcomePage(void)
{
	_fg = TFT_RED;
	TFT_setFont(COMIC24_FONT, NULL);
	TFT_print("WELCOME", CENTER, 25);
	TFT_print("PHOTIZZO", CENTER, 90);
	TFT_print("WATTBANK", CENTER, 170);
}

void pinSetup(void)
{
	gpio_pad_select_gpio(LED_PIN);
	gpio_pad_select_gpio(OVERLOAD_TRIGGER_PIN);
	gpio_pad_select_gpio(BUTTON_PIN);
	gpio_pad_select_gpio(REMOTE_CHARGER_CONTROL);
	gpio_pad_select_gpio(BAT_LOW_CONTROL);
	gpio_set_direction(REMOTE_CHARGER_CONTROL, GPIO_MODE_OUTPUT);
	gpio_set_direction(BAT_LOW_CONTROL, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BAT_LOW_CONTROL, GPIO_PULLUP_ONLY);

	/* Set the GPIO as a push/pull output */

	gpio_set_direction(OVERLOAD_TRIGGER_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(OVERLOAD_TRIGGER_PIN, GPIO_PULLDOWN_ONLY);

	/*gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
	 gpio_set_intr_type(OVERLOAD_TRIGGER_PIN,GPIO_INTR_NEGEDGE);
	 gpio_isr_handler_add(OVERLOAD_TRIGGER_PIN,gpio_isr_handler, (void*)OVERLOAD_TRIGGER_PIN);
	 gpio_intr_enable(OVERLOAD_TRIGGER_PIN);*/

	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	//gpio_set_direction(inverterpin, GPIO_MODE_OUTPUT);
	gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
}

void setONparameters()
{
	userDataPage();
	gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
}

void systemOFF()
{
	gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
}

static void buttonTask(void *parameters)
{
	uint32_t Current_tick_count;
	uint32_t New_tick_count;
	uint32_t delay_in_ticks;
	tx_task_action action;
	rx_task_action rx_action;
	screen_action_t screenAction;

	while (1)
	{
		bool update = false;
		bool charging = (powerFlowGraph & (0x01 << 3)) || (powerFlowGraph & (0x01 << 1));

		if (gpio_get_level(BUTTON_PIN) == 1)
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			if (gpio_get_level(BUTTON_PIN) == 1)
			{
				Current_tick_count = xTaskGetTickCount();
				while (gpio_get_level(BUTTON_PIN) == 1)
					;
				New_tick_count = xTaskGetTickCount();
				delay_in_ticks = New_tick_count - Current_tick_count;
				if (delay_in_ticks < 10000)
				{
					if (warningDisplay)
					{
						system_state = false;
						warningDisplay = false;
						warningStatus = false;
						warning_status = WARNING_NORM;
						batteryLOWShutdown = false;
					}
					else
					{
						system_state = !system_state;
					}
					update = true;
				}

				else
				{
					// ESP_ERROR_CHECK(esp_wifi_stop());
					// incomingData = defaultconfig;
					// incomingData = strtok(incomingData, ":");
					// detail.username = strtok(NULL, "&");
					// detail.password = strtok(NULL, ":");
					// ESP_ERROR_CHECK(esp_wifi_stop());
					// //ESP_LOGI(TAG,"STOPPED WIFI");
					// xSemaphoreGive(wifiReset);
				}
			}
		}

		if ((batteryLOWShutdown) && charging)
		{
			systemMode = SYSTEM_OFF;
			screenAction = SCREEN_OFF;
			///TFT_fillWindow(TFT_BLACK);
			xQueueSendToFront(screen_Queue, &screenAction, portMAX_DELAY);
			update = true;
			warningDisplay = false;
			batteryLOWShutdown = false;
			warning_status = WARNING_NORM;
			warningStatus = false;
			//vTaskDelay(300 / portTICK_PERIOD_MS);
		}

		else
		{
		}

		if (update)
		{
			if (system_state)
			{
				powerCommand = SYSTEM_ON_COMMAND;
				screenAction = SCREEN_ON;
				systemMode = SYSTEM_ON;
			}

			else
			{
				powerCommand = SYSTEM_OFF_COMMAND;
				screenAction = SCREEN_OFF;
				systemMode = SYSTEM_OFF;
			}
			runTimeHours = 0;
			runTimeMinutes = 0;

			update = false;
			action = TX_SYSTEM_POWER_CONTROL;
			xQueueSendToFront(screen_Queue, &screenAction, portMAX_DELAY);
		}
		gpio_set_level(LED_PIN, system_state);
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

static void warningTask(void *parameters)
{
	screen_action_t screenAction;
	while (1)
	{
		//Set warning state of the system*********************//
		if ((powerFlowGraph & (0x01 << 7)) && (systemMode == SYSTEM_ON) && (busVoltage < BATTERY_LOW_CHARGEUP))
		{
			warningStatus = true;
			sprintf(warningChar, "BATTERY LOW");
			warning_status = LOW_BATTERY_WARNING;
			batteryLOWShutdown = true;
			powerFlowGraph &= ~(0x01 << 7);
		}

		else if ((gpio_get_level(OVERLOAD_TRIGGER_PIN) == 1) && (systemMode == SYSTEM_ON))
		{
			vTaskDelay(100 / portTICK_PERIOD_MS);
			if (gpio_get_level(OVERLOAD_TRIGGER_PIN) == 1)
			{
				warningStatus = true;
				sprintf(warningChar, "OVERLOAD");
				warning_status = OVERLOADS;
			}
		}

		else
		{
			// if (batteryLOWShutdown)
			// {
			// 	warning_status = WARNING_NORM;
			// 	warningDisplay = false;
			// }
			warningStatus = false;
		}

		//alert the screen task for update about warning state
		if (warningStatus == true)
		{
			screenAction = WARNING;
			xQueueSendToFront(screen_Queue, &screenAction, portMAX_DELAY);
			warningStatus = false;
		}

		vTaskDelay(3000 / portTICK_RATE_MS);
	}
}

void defaultScreen(void)
{
	TFT_fillScreen(TFT_BLACK);
	mainPage();
	data_formatting();
	userDataPage();
}

void warningPage(void)
{

	TFT_fillScreen(TFT_BLACK);
	_fg = TFT_RED;
	TFT_setFont(COMIC24_FONT, NULL);
	TFT_print(warningChar, CENTER, 25);
	TFT_print("SYSTEM DOWN", CENTER, 90);
	//TFT_print("DOWN", CENTER, 120);
}

void powerFlowDirectionTask(void)
{

	powerSource = (powerFlowGraph & (0x01 << 6)) ? GRID_SUPPLY : WATTBANK;

	powerGraph.solar2home = (powerFlowGraph & (0x01)) ? 1 : 0;
	powerGraph.solar2wattbank = (powerFlowGraph & (0x01 << 1)) ? 1 : 0;
	powerGraph.grid2home = (powerFlowGraph & (0x01 << 2)) ? 1 : 0;
	powerGraph.grid2wattbank = (powerFlowGraph & (0x01 << 3)) ? 1 : 0;
	powerGraph.wattbank2home = (powerFlowGraph & (0x01 << 4)) ? 1 : 0;
	powerGraph.gridsense = (powerFlowGraph & (0x01 << 6)) ? 1 : 0;

	if (systemMode == SYSTEM_OFF || powerSource == GRID_SUPPLY) /// test
	{
		powerGraph.wattbank2home = 0;
		runTimeMinutes = 0;
		runTimeHours = 0;
		loadPower = 0.0;
		loadCurrent = 0;
	}

	if(powerGraph.grid2wattbank ==0)
	{
		smpsCurrent =0;
		smpsPower = 0;
	}

	solar2home = (powerGraph.solar2home) ? TFT_GREEN : TFT_BLACK;
	solar2wattbank = (powerGraph.solar2wattbank) ? TFT_GREEN : TFT_BLACK;
	grid2home = (powerGraph.grid2home) ? TFT_GREEN : TFT_BLACK;
	grid2wattbank = (powerGraph.grid2wattbank) ? TFT_GREEN : TFT_BLACK;
	wattbank2home = (powerGraph.wattbank2home) ? TFT_GREEN : TFT_BLACK;

	if (powerSource == GRID_SUPPLY)
	{
		loadPower = 0.0;
		loadCurrent = 0;
		upsmode = true;
	}
	else
	{
		// powerGraph.grid2home = 0;
		upsmode = false;
	}

	//*******************Condition to cater for Charging while in off state***************//
	if ((systemMode == SYSTEM_OFF) && ((powerFlowGraph & (0x01 << 1)) || (powerFlowGraph & (0x01 << 3))))
	{
		if (batteryLOWShutdown || warning_status == LOW_BATTERY_WARNING)
		{
			gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
			defaultScreen();
			gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);

			if (warningDisplay && warning_status == LOW_BATTERY_WARNING)  /// test
			{
				system_state = false;
				warningDisplay = false;
				warningStatus = false;
				warning_status = WARNING_NORM;
				batteryLOWShutdown = false;
			}
		}

		else
		{
			// if (warningDisplay)
			// {
			// 	system_state = false;
			// 	warningDisplay = false;
			// 	warning_status = WARNING_NORM;
			// 	batteryLOWShutdown = false;
			// }
			gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
		}
	}
	else if (systemMode == SYSTEM_OFF)
	{
		gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
	}
	else
	{
	}

	//*****************Set the charging mode of the system******************//
	if ((powerFlowGraph & (0x01 << 1)) || (powerFlowGraph & (0x01 << 3)))
	{
		if (batteryLOWShutdown) {
			if (warning_status == LOW_BATTERY_WARNING) warning_status = WARNING_NORM;
			warningDisplay = false;
		}

		system_subMode = SYSTEM_CHARGING;
	}

	else
	{
		system_subMode = SYSTEM_NOT_CHARGING;
	}

	if (powerSource == WATTBANK)
	{
		GridrunTimeMinutes = 0;
		GridrunTimeHours = 0;
	}

	bool solarCheck = (powerGraph.solar2wattbank || powerGraph.solar2home);
	if (solarCheck == false)
	{
		SolarRunTimeHours = 0;
		SolarRunTimeMinutes = 0;
	}

	//***********************************Power direction graph*****************//
	TFT_drawLine(70, 120, 145, 120, solar2home);
	TFT_drawLine(145, 120, 145, 70, solar2home);
	TFT_drawLine(70, 130, 145, 130, solar2wattbank);
	TFT_drawLine(145, 130, 145, 172, solar2wattbank);

	//grid
	TFT_drawLine(275, 120, 165, 120, grid2home);
	TFT_drawLine(165, 120, 165, 70, grid2home);
	TFT_drawLine(275, 130, 165, 130, grid2wattbank);
	TFT_drawLine(165, 130, 165, 172, grid2wattbank);
	TFT_drawLine(155, 172, 155, 70, wattbank2home);


	//***********************************Power direction graph*****************//
	TFT_drawLine(70, 120, 145, 120, solar2home);
	TFT_drawLine(145, 120, 145, 70, solar2home);
	TFT_drawLine(70, 130, 145, 130, solar2wattbank);
	TFT_drawLine(145, 130, 145, 172, solar2wattbank);

	//grid
	TFT_drawLine(275, 120, 165, 120, grid2home);
	TFT_drawLine(165, 120, 165, 70, grid2home);
	TFT_drawLine(275, 130, 165, 130, grid2wattbank);
	TFT_drawLine(165, 130, 165, 172, grid2wattbank);
	TFT_drawLine(155, 172, 155, 70, wattbank2home);
}

static void systemControlTask(void *arg)
{
	tx_task_action tx_action;
	rx_task_action rx_action;
	screen_action_t screenAction;
	while (1)
	{
		if (!warningDisplay)
		{
			screenAction = DATA_UPDATE;
			xQueueSend(screen_Queue, &screenAction, portMAX_DELAY);
		}

		if (mqttG)
		{
			printf("mqttdetail available");
			xSemaphoreGive(mqttDetailUpdate);
			mqttG = false;
		}
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

static void screenUpdateTask()
{
	screen_action_t screenAction;
	tx_task_action action;
	while (1)
	{
		xQueueReceive(screen_Queue, &screenAction, portMAX_DELAY);
		//printf("screen task");
		switch (screenAction)
		{
		case SCREEN_ON:
		{
			data_formatting();
			userDataPage();
			powerFlowDirectionTask();
			gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
			break;
		}
		case SCREEN_OFF:
		{
			gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
			TFT_fillWindow(TFT_BLACK);
			energyConsumption = 0.0;
			mainPage();
			//loadCurrent = 0;
			break;
		}
		case DATA_UPDATE:
		{
			data_formatting();
			userDataPage();
			powerFlowDirectionTask();
			break;
		}
		case WARNING:
		{
			gpio_set_level(LED_PIN, 0);
			system_state = false;
			systemMode = SYSTEM_OFF;
			powerCommand = SYSTEM_OFF_COMMAND;
			//action = TX_SYSTEM_POWER_CONTROL;
			warningDisplay = true;
			///xQueueSendToFront(tx_Queue, &action, portMAX_DELAY);
			warningPage();
			//vTaskSuspend(buttonTask);
			//vTaskSuspend(systemControlTask);
			vTaskDelay(pdMS_TO_TICKS(2000));

			runTimeHours = 0;
			runTimeMinutes = 0;
			//gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
			//defaultScreen();
			// vTaskResume(buttonTask);
			//vTaskResume(systemControlTask);
			break;
		}
		default:
		{
			break;
		}
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

int sendData(const char *logName, const char *data)
{
	const int len = strlen(data);
	const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
	//ESP_LOGI(logName, "Wrote %d bytes", txBytes);
	return txBytes;
}

static void mqtt_app_start(void)
{
	//state mqtt client details
	esp_mqtt_client_config_t mqtt_cfg ={
		.uri = "mqtt://hairdresser.cloudmqtt.com",
		.event_handle = mqtt_event_handler,
		.port = 18541,
		.username = "epsaduza",
		.password = "PtsRK6hfDTVg" };

	client = esp_mqtt_client_init(&mqtt_cfg);
	// esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	if (esp_mqtt_client_start(client) == ESP_OK)
	{
		printf("mqtt connected ");
	}

	else
	{
		printf("could not start mqtt connection");
	}
}

static void tx_task(void *arg)
{
	static const char *TX_TASK_TAG = "TX_TASK";
	esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
	char gatewayData[200];
	char Data_update_buffer[50];

	while (1)
	{
		//Extract current time
		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%d,%m,%Y,%H,%M,%S", &timeinfo);
		//ESP_LOGI(TAG, "The current date/time in Nigeria is: %s", strftime_buf);

		//enerygy consumed computation
		if (systemMode == SYSTEM_ON)
		{
			energyConsumption += (loadPower * ENERGY_REFERENCE);
		}
		else
		{
			resetParams();
		}

		if (batteryLOWShutdown)
		{
			resetParams();
			powerGraph.wattbank2home = 0;
			batteryPercentage = 0;  // new untested
		}

		if (powerSource == GRID_SUPPLY)
		{
			resetParams();
		}

	bool proceed = true;

	if(loadPower>(float)10000.0 || solarPower>(float)6000.0 || loadCurrent>30000)
	{
		proceed = false;
	}

	if(loadPower<(float)0.0 || solarPower<(float)0.0)
	{
		proceed = false;
	}

	if(proceed)
	{
	//*********************Format Gateway Data for publication*********//
		sprintf(gatewayData, "%i,%i,%i,%i,%4.2f,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%s,%i,%i,%i,%i,%i,%i,%i,%i,%i",
			loadCurrent, solarCurrent, busVoltage, outputVoltage, energyConsumption, batteryPercentage,
			columbCount, runTimeHours, runTimeMinutes, GridrunTimeHours,
			GridrunTimeMinutes, SolarRunTimeHours, SolarRunTimeMinutes, powerSource, systemMode,
			system_subMode, warning_status, strftime_buf, smpsCurrent,
			c_d_current, inverterStatus, chargerStatus,
			powerGraph.solar2home, powerGraph.solar2wattbank,
			powerGraph.grid2home, powerGraph.grid2wattbank,
			powerGraph.wattbank2home);
	}

		if (powerGraph.solar2home || powerGraph.solar2wattbank)
		{
			Solarcounter++;
			if (Solarcounter >= 2)
			{
				Solarcounter = 0;
				SolarRunTimeMinutes += 1;
				if (SolarRunTimeMinutes >= 60)
				{
					SolarRunTimeHours += 1;
					SolarRunTimeMinutes = 0;
				}
			}
		}

		if (powerSource == GRID_SUPPLY)
		{
			Gridcounter++;
			if (Gridcounter >= 2)
			{
				Gridcounter = 0;
				GridrunTimeMinutes += 1;
				if (GridrunTimeMinutes >= 60)
				{
					GridrunTimeHours += 1;
					GridrunTimeMinutes = 0;/// fixed
				}
			}
		}

		else if (systemMode == SYSTEM_ON && powerSource == WATTBANK)
		{
			counter++;
			if (counter >= 2) //*************compute RUN Time while system is ON*********//
			{
				counter = 0;
				runTimeMinutes += 1;
				if (runTimeMinutes >= 60)
				{
					runTimeHours += 1;
					runTimeMinutes = 0;
				}
			}
		}

		else
		{
		}

		// ***************Selection of Module to publish Data***********//
		if (mqtt_connection)
		{ //*****if wifi is available , publish with wifi gateway*****//
			esp_mqtt_client_publish(client, publishTopic, gatewayData, 0, 1, 0);
			printf("uart_tx_task-WIFI");
		}

		else
		{
			memset(newData, 0, sizeof(newData));
			sprintf(newData, "%s%s%s", "<", gatewayData, ">");
			sendData(TX_TASK_TAG, newData); //*****send Data to sim Gateway for publishing***//
		}

		// if (!mqtt_connection && wifiConnectionStatus == true)
		// {
		// 	esp_mqtt_client_destroy(client);
		// 	mqtt_app_start();
		// }

		//*******************************Take care of time Syncronization request from Sim_Gateway*************//

		if ((!time_synchronized) && (wifiConnectionStatus))
		{
			xSemaphoreGive(settingTime);
		}

		///&& (!wifiConnectionStatus)
		if ((!time_synchronized))
		{
			uart_write_bytes(UART_NUM_1, "inactive", 9);
		}

		else
		{
		}

		//**************************For any update of server Data**********************************//
		switch (Data_update)
		{
		case 1:
			///	sprintf(Data_update_buffer, "update1 %s %s", publishTopic, subscribeTopic);
			sendData(TX_TASK_TAG, Data_update_buffer);
			Data_update = 0;
			break;
		case 2:
			sprintf(Data_update_buffer, "update2 ssid:%s password:%s", detail.username, detail.password);
			sendData(TX_TASK_TAG, Data_update_buffer);
			Data_update = 0;
			break;
		case 3:
			sprintf(Data_update_buffer, "%s", dumpMQTT);
			sendData(TX_TASK_TAG, Data_update_buffer);
			Data_update = 0;
			break;
		default:
			break;
		}

		vTaskDelay(30000 / portTICK_PERIOD_MS); //****** allow run of blocking Task*****//
	}
}

static void synchronizeTime(void *parameters)
{
	while (1)
	{
		if (xSemaphoreTake(settingTime, portMAX_DELAY) == pdTRUE)
		{
			if (!time_synchronized)
			{
			}
			// obtain_time();
			// if (time_synchronized) vTaskDelete(&xHandletime);
		}
	}
}

static void wifiResetTask(void *parameter)
{
	while (1)
	{
		if (xSemaphoreTake(wifiReset, portMAX_DELAY) == pdTRUE)
		{
			if (esp_mqtt_client_stop(client) == ESP_OK)
				printf("stopplizing mqtt");
			else
				printf("could stop mqtt");
			wifi_init_();
			printf("init new wifi");
			if (esp_mqtt_client_start(client) == ESP_OK)
				printf("mqtt connected");
			else
				printf("mqtt not connected");
			Data_update = 2;
			//newWifi= false;
		}

		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}

void init_nvs()
{
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

//=============
void app_main()
{
	rx_Queue = xQueueCreate(1, sizeof(rx_task_action));
	tx_Queue = xQueueCreate(1, sizeof(tx_task_action));
	screen_Queue = xQueueCreate(1, sizeof(screen_action_t));
	settingTime = xSemaphoreCreateBinary();
	wifiReset = xSemaphoreCreateBinary();
	mqttDetailUpdate = xSemaphoreCreateBinary();
	rx_task_action rx_action;
	tx_task_action tx_action;
	screen_action_t screenAction;

	init();

	tft_disp_type = DISP_TYPE_ILI9341;
	_width = DEFAULT_TFT_DISPLAY_WIDTH;	  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension

	max_rdclock = 8000000;

	TFT_PinsInit();

	// ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

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
	// ====================================================================================================================
	vTaskDelay(500 / portTICK_RATE_MS);

	ret = spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
	assert(ret == ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
	assert(ret == ESP_OK);
	ret = spi_lobo_device_deselect(spi);
	assert(ret == ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n",
		spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n",
		spi_lobo_uses_native_pins(spi) ? "true" : "false");

	TFT_display_init();
	//printf("OK\r\n");

	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

	// ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
	TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(LANDSCAPE_FLIP);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();

	vfs_spiffs_register();
	if (!spiffs_is_mounted)
	{
		printf("SPIFFS not mounted");
	}
	else
	{
		printf("SPIFFS mounted");
	}

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	init_nvs();

	wifi_init_sta();
	mqtt_app_start();
	///ws2812_control_init();
	//	ws2812_show(OFF);

	powerCommand = SYSTEM_OFF_COMMAND;
	rx_action = RX_DATA_MODE;
	tx_action = TX_SYSTEM_POWER_CONTROL;
	screenAction = SCREEN_OFF;
	warning_status = WARNING_NORM;
	system_subMode = SYSTEM_NOT_CHARGING;
	welcomePage();
	pinSetup();
	gpio_set_level(REMOTE_CHARGER_CONTROL, 0);
	vTaskDelay(5000 / portTICK_RATE_MS);
	systemOFF();
	TFT_fillScreen(TFT_BLACK);
	defaultScreen();

	xTaskCreate(rx_task, "uart_rx_task", 9216, NULL, RX_PRIORITY, NULL);
	xTaskCreate(tx_task, "uart_tx_task", 9096, NULL, RX_PRIORITY - 1, NULL);
	xTaskCreate(buttonTask, "button function", 8096, NULL, BUTTON_PRIORITY, NULL);
	xTaskCreate(DataRx_Task, "DataUart_Rx_Task", 20216, NULL, CAN_RECIEVE_PRIORITY, NULL);
	xTaskCreate(warningTask, "warning function", 8192, NULL, 12, NULL);
	xTaskCreatePinnedToCore(systemControlTask, "system control task", 9096, NULL, SYSTEM_CONTROL_PRIORITY, NULL, tskNO_AFFINITY);
	xTaskCreate(screenUpdateTask, "Screen Update Task", 8192, NULL, SCREEN_PRIORITY, NULL);
	xTaskCreate(synchronizeTime, "Time Synchronization Task", 4096, NULL, TIMER_TASK_PRIORITY, &xHandletime);
	xTaskCreate(wifiResetTask, "Reset Wifi Details", 4096, NULL, TIMER_TASK_PRIORITY, &xHandlewifi);
	// xTaskCreate(mqttUpdateTask, "Update MQTT Details", 4096, NULL, TIMER_TASK_PRIORITY, &xHandlemqtt);

	xQueueSend(screen_Queue, &screenAction, portMAX_DELAY);
	// xQueueSend(rx_Queue, &rx_action, portMAX_DELAY);
	// xQueueSend(tx_Queue, &tx_action, portMAX_DELAY);
	//vTaskStartScheduler();
}
