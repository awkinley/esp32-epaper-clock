#include <stdlib.h>     //exit()
#include <signal.h>     //signal()
#include <time.h>
#include "GUI_Paint.h"
#include "GUI_BMPfile.h"
#include "ImageData.h"
#include "EPD_2in9.h"
#include "Fonts/fonts.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include <driver/dac.h>
#include "lwip/err.h"
#include "lwip/apps/sntp.h"


/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID ""
#define EXAMPLE_WIFI_PASS ""

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static const char *TAG = "example";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
RTC_DATA_ATTR static int boot_count = 0;

static void obtain_time(void);
static void initialize_sntp(void);
static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);


void update_time(int x, int y, PAINT_TIME paint_time, bool is_am)
{
    char am_pm[] = "AM";

    if (is_am)
    {
        //am_pm = "AM";
    }
    else
    {
        am_pm[0] = 'P';
    }

    Paint_DrawString_EN(x + Anonymous64.Width * 3, y + Anonymous64.Height, am_pm, &Anonymous24, WHITE, BLACK);
    Paint_DrawTime(x, y, &paint_time, &Anonymous64, WHITE, BLACK);
}

void setup_display()
{
    spi_device_handle_t spi = DEV_ModuleInit();

    if (EPD_Init(spi, lut_full_update) != 0) {
        printf("e-Paper init failed\r\n");
    }

    EPD_Clear();
}

void setup_wifi()
{
    ++boot_count;
    ESP_LOGI(TAG, "Boot count: %d", boot_count);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);
}

/* Experimental development board power consumption (with red led powered)
 * Deep sleep waiting on timer: 12mA
 * Deep sleep waiting on timer with epaper display connected : 12 - 13mA
 * Constatnly running with display switching: 44 - 54mA 
 * With wifi running: up to 160mA with wifi going
 * After startup with deep sleep implemented: 
*/ 

int app_main(void)
{
    // printf("about the start deep sleep\r\n");

    // esp_sleep_enable_timer_wakeup(1000000 * 60); //configure deep sleep for 60 seconds
    // esp_deep_sleep_start();
    // printf("done deep sleep\r\n");

    setup_display();
    
    setup_wifi();

    //Create a new image cache
    UBYTE *BlackImage;
    UWORD Imagesize = ((EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8 ) : (EPD_WIDTH / 8 + 1)) * EPD_HEIGHT;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }
    printf("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 270, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    int y = (EPD_WIDTH - Anonymous64.Height) / 2;
    float text_width = Anonymous64.Width * 4.5f;
    int x = (EPD_HEIGHT - (text_width)) / 2;
    x = 16;
    // printf("text width = %f", text_width);
    // printf("x, y (%d, %d)\n", x, y);
    // printf("show time...\r\n");

    PAINT_TIME new_paint_time;
    PAINT_TIME sPaint_time;
    bool is_am;

    time_t now;
    struct tm timeinfo;

    bool do_sleep = false; 
    int sleep_length = 0;
    for (;;) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_hour > 12)
        {
            is_am = false;
            new_paint_time.Hour = timeinfo.tm_hour - 12;
        }
        else
        {
            is_am = true;
            new_paint_time.Hour = timeinfo.tm_hour;
        }

        new_paint_time.Min = timeinfo.tm_min;
        new_paint_time.Sec = timeinfo.tm_sec;

        if (sPaint_time.Min != new_paint_time.Min || sPaint_time.Min != new_paint_time.Min)
        {
            sPaint_time.Hour = new_paint_time.Hour;
            sPaint_time.Min = new_paint_time.Min;

            int64_t start_time = esp_timer_get_time();
            Paint_Clear(WHITE);
            if (EPD_Restart_Display(lut_full_update) != 0) {
                printf("e-Paper restart failed\r\n");
            }

            update_time(x, y, sPaint_time, is_am);

            EPD_Display(BlackImage);

            EPD_Sleep();

            int64_t end_time = esp_timer_get_time();

            int64_t elapsed_time = end_time - start_time;
            printf("elapsed time is: %lld microseconds\r\n", elapsed_time);
            do_sleep = true;
            sleep_length = 60000000 - new_paint_time.Sec * 1000000 - elapsed_time;
            printf("time sec = %d\r\n", new_paint_time.Sec);
            printf("sleep time = %d\r\n", sleep_length);
            fflush(stdout);
        }

        if(do_sleep){
            esp_sleep_enable_timer_wakeup(sleep_length);
            esp_light_sleep_start();
            do_sleep = false;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    printf("Goto Sleep mode...\r\n");
    EPD_Sleep();
    free(BlackImage);
    BlackImage = NULL;

    return 0;
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}
