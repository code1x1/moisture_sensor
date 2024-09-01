/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_wnm.h"
#include "esp_rrm.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "driver/adc.h"

#define MAX_HTTP_REQ_BUFFER 128
#define MAX_HTTP_OUTPUT_BUFFER 128

#define US_SECOND 1000000
#define US_MINUTE US_SECOND * 60
#define US_HOUR US_MINUTE * 60

static const char *TAG = "moisture sensor";

static EventGroupHandle_t wifi_event_group;

typedef struct {
    uint16_t average;
    uint16_t single;
} Moisture;

void restart()
{
    ESP_ERROR_CHECK( esp_wifi_stop() );
    ESP_LOGI(TAG, "Light Sleep 30 minutes...\n");
    ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(30 * US_MINUTE) );
    ESP_ERROR_CHECK( esp_light_sleep_start() );
    ESP_LOGI(TAG, "Device wake up...\n");

    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void send_data(void* event_data)
{
    Moisture* moisture = (Moisture*) event_data;
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    char local_request_buffer[MAX_HTTP_REQ_BUFFER] = {0};

    esp_http_client_config_t config = {
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .host = CONFIG_PUSHGATEWAY_HOST,
        .port = 9091,
        .path = "/metrics/job/moneytree",
        .user_data = local_response_buffer,
        .is_async = false
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    sprintf(local_request_buffer,
    "# TYPE moisture_average untyped\n"
    "moisture_average %d\n"
    "# TYPE moisture_single untyped\n"
    "moisture_single %d\n\n"
    , moisture->average
    , moisture->single);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "text/plain");
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_post_field(client, local_request_buffer, strlen(local_request_buffer));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Station connected.\n");
        send_data(arg);
        restart();
	}
}

void connect_wifi(void* event_data)
{

	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK( esp_netif_init() );

	ESP_ERROR_CHECK( esp_event_loop_create_default() );

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&config) );

	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, event_data) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, event_data) );

	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASSWORD,
			.rm_enabled = 1,
			.btm_enabled = 1,
		},
	};

	ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...\n", wifi_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void app_main()
{
    ESP_LOGI(TAG, "Starting " CONFIG_APP_PROJECT_VER "\r\n");

    uint16_t single = 0;
    uint16_t average = 0;
    {

        uint16_t messurement[100];
        adc_config_t config = {
            .mode = ADC_READ_TOUT_MODE,
            .clk_div = 8};
        ESP_ERROR_CHECK(adc_init(&config));

        int x;
        if (ESP_OK == adc_read(&single))
        {
            ESP_LOGI(TAG, "adc_read: %d", single);
        }

        if (ESP_OK == adc_read_fast(messurement, 100))
        {
            for (x = 0; x < 100; x++)
            {
                average += messurement[x];
                if (x > 0)
                {
                    average /= 2;
                }
            }

            ESP_LOGI(TAG, "average of adc_read_fast 100 messurements %d", average);
        }
    }

    Moisture *moisture = malloc(sizeof(Moisture));
    moisture->average = average;
    moisture->single = single;
    connect_wifi(moisture);
}
