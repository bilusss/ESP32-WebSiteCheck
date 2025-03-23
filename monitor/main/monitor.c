#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "time.h"
#include "lwip/apps/sntp.h"

// Definicje
#define WIFI_SSID "router"
#define WIFI_PASS "qwerty000"
#define DISCORD_WEBHOOK_STATUS "https://discord.com/api/webhooks/1352055964282388530/xpAbO0FhnFfSdmzvFUqot9doxNdQiBi5NknNCTU1NFCgXPDmcdzJ_dQjMiBSdKIiySi1"
#define DISCORD_WEBHOOK_CHANGE "https://discord.com/api/webhooks/1352043110716411990/tl118UmEJPpnom3ziT3mo8FD7i3OCSqSWPpZzki4Vj-awxtsriR7bFHNdXg79xRenXMD"
#define DISCORD_WEBHOOK_ERRORS "https://discord.com/api/webhooks/1352976327250149487/A6iPf0QEBJ7CS5bv87aAgG-wMOZToIBLzOJV2pPPmytPrG_6RSC6UyEwfAFbKNzmq1bR"
#define DISCORD_TTS "false"
#define TARGET_URL "https://sabre.wd1.myworkdayjobs.com/SabreJobs?locationCountry=131d5ac7e3ee4d7b962bdc96e498e412"
#define SCRAPINGBEE_API_KEY "ATSI3ZGN4G9VEZ0P145YCI52HIR98IBX88Z6YH8VMLX0A5MFKA9G8INPGGNETAQCLAYO7KK8HRJQ1CC7"
#define GRABZIT_API_KEY "YjQwYmE3MDFjOThkNGY5ZWIxYmJjZTM3YmM5NDIzNjM"
#define SCRAPINGBEE_URL "https://app.scrapingbee.com/api/v1/?api_key=" SCRAPINGBEE_API_KEY "&url=" TARGET_URL "&render_js=true"
#define GRABZIT_URL "https://api.grabz.it/services/convert.ashx?key=" GRABZIT_API_KEY "&delay=3000&format=html&url=" TARGET_URL
static const char *TAG_DISCORD = "DISCORD";
static const char *TAG_WIFI = "WIFI";
static const char *TAG_WEBSITE = "WEBSITE";
static const char *TAG_SNTP = "SNTP";

const char *DISCORD_CERT = R"(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)";

// Inicjalizacja NVS (Non-Volatile Storage)
static void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// Obsługa zdarzeń Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG_WIFI, "Disconnected. Reconnecting to Wi-Fi...");
    }
}

// Inicjalizacja Wi-Fi
static void init_wifi(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Wi-Fi initialized. Connecting to %s...", WIFI_SSID);
}

// Funkcja wysyłająca wiadomość do Discorda
static esp_err_t send_discord_message(const char *content, const int discordChannel) {
    esp_http_client_config_t config = {
        .cert_pem = DISCORD_CERT,
        .method = HTTP_METHOD_POST,
    };
    if (discordChannel == 0) {
        config.url = DISCORD_WEBHOOK_STATUS;
    } else if (discordChannel == 1) {
        config.url = DISCORD_WEBHOOK_CHANGE;
    }
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *json_payload = malloc(1024);
    if (json_payload == NULL) {
        ESP_LOGE(TAG_DISCORD, "Failed to allocate json_payload");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    snprintf(json_payload, 1024, "{\"content\":\"%s\",\"tts\":%s}", content, DISCORD_TTS);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_DISCORD, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG_DISCORD, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    free(json_payload);
    esp_http_client_cleanup(client);
    return err;
}

// Funkcja pobierająca HTML z usługi (GrabzIt lub ScrapingBee)
static char *fetch_from_service(const char *service_url, const char *service_name) {
    esp_http_client_config_t config = {  
        .url = service_url,  
        .method = HTTP_METHOD_GET,  
        .crt_bundle_attach = esp_crt_bundle_attach,  
        .user_agent = "Mozilla/5.0 (compatible; ESP32)",  
        .timeout_ms = 10000,  
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG_WEBSITE, "Failed to initialize HTTP client for %s", service_name);
        return NULL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WEBSITE, "Failed to open HTTP connection for %s: %s", service_name, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG_WEBSITE, "Failed to fetch headers for %s: %d", service_name, content_length);
        esp_http_client_cleanup(client);
        return NULL;
    }
    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG_WEBSITE, "%s HTTP status: %d", service_name, status);
    ESP_LOGI(TAG_WEBSITE, "%s Content length: %d", service_name, content_length);

    if (status != 200) {
        ESP_LOGE(TAG_WEBSITE, "Unexpected HTTP status from %s: %d", service_name, status);
        esp_http_client_cleanup(client);
        return NULL;
    }

    char *html_buffer = (char *)malloc(content_length + 1);
    if (html_buffer == NULL) {
        ESP_LOGE(TAG_WEBSITE, "Failed to allocate memory for HTML from %s", service_name);
        esp_http_client_cleanup(client);
        return NULL;
    }

    int html_len = esp_http_client_read(client, html_buffer, content_length);
    if (html_len < 0) {
        ESP_LOGE(TAG_WEBSITE, "Failed to read HTML content from %s: %d", service_name, html_len);
        free(html_buffer);
        esp_http_client_cleanup(client);
        return NULL;
    }

    html_buffer[html_len] = '\0';
    ESP_LOGI(TAG_WEBSITE, "Fetched HTML length from %s: %d bytes", service_name, html_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return html_buffer;
}

// Funkcja pobierająca HTML ze strony, najpierw z GrabzIt, potem z ScrapingBee
static char *fetch_website_html(void) {
    char *html = fetch_from_service(GRABZIT_URL, "GrabzIt");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    if (html == NULL || html == 0) {
        ESP_LOGW(TAG_WEBSITE, "Failed to fetch from GrabzIt, trying ScrapingBee");
        html = fetch_from_service(SCRAPINGBEE_URL, "ScrapingBee");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    return html;
}

static uint32_t calculate_checksum(const char *data) {
    if (data == NULL) return 0;
    uint32_t checksum = 0;
    while (*data) {
        checksum += (unsigned char)(*data++);
    }
    return checksum;
}

// Funkcja inicjalizująca synchronizację czasu z NTP
static void initialize_sntp(void) {
    ESP_LOGI(TAG_SNTP, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2025 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG_SNTP, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry < retry_count) {
        ESP_LOGI(TAG_SNTP, "Time synchronized successfully!");
    } else {
        ESP_LOGE(TAG_SNTP, "Failed to synchronize time");
    }
}

// Funkcja pobierająca i formatująca aktualny czas
void get_formatted_time(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    snprintf(buffer, buffer_size, "%02d:%02d:%02d %02d/%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             timeinfo.tm_mday, timeinfo.tm_mon + 1);
}

static bool is_connected_to_internet(void) { return true; }

void app_main(void) {
    char time_str[20];
    uint32_t last_checksum = 0;

    init_nvs();
    init_wifi();
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    send_discord_message("System started", 0);
    initialize_sntp();

    while (true) {
        get_formatted_time(time_str, sizeof(time_str));
        ESP_LOGI(TAG_SNTP, "Current time: %s", time_str);

        char *status_msg = malloc(50);
        if (status_msg == NULL) {
            ESP_LOGE(TAG_DISCORD, "Failed to allocate status_msg");
            continue;
        }
        snprintf(status_msg, 50, "Current time: %s", time_str);
        send_discord_message(status_msg, 0);
        free(status_msg);

        if (is_connected_to_internet()) {
            char *html = fetch_website_html();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            if (html != NULL) {
                uint32_t current_checksum = calculate_checksum(html);
                ESP_LOGI(TAG_WEBSITE, "Checksum: %lu", current_checksum);

                if (last_checksum != 0 && current_checksum != last_checksum) {
                    char *change_msg = malloc(256);
                    if (change_msg == NULL) {
                        ESP_LOGE(TAG_DISCORD, "Failed to allocate change_msg");
                        free(html);
                        continue;
                    }
                    snprintf(change_msg, 256, "Check you the link: %s Checksum changed at %s: %lu -> %lu",
                             TARGET_URL, time_str, last_checksum, current_checksum);
                    send_discord_message(change_msg, 1);
                    free(change_msg);
                }
                last_checksum = current_checksum;
                free(html);
            } else {
                ESP_LOGE(TAG_WEBSITE, "Failed to fetch website HTML from both services");
            }
        } else {
            ESP_LOGE(TAG_WIFI, "No internet connection, skipping website fetch");
        }

        vTaskDelay(9600000 / portTICK_PERIOD_MS); // 9600000 ms = 2h 40m
    }
}