#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/md5.h>

// Konfiguracja Wi-Fi
#define WIFI_SSID "twoja_siec"
#define WIFI_PASS "twoje_haslo"

// URL strony do monitorowania
#define TARGET_URL "https://sabre.wd1.myworkdayjobs.com/SabreJobs?locationCountry=131d5ac7e3ee4d7b962bdc96e498e412"

// Webhooki IFTTT (GET, bo Bruce może nie obsługiwać POST natywnie)
#define WEBHOOK_STATUS "https://maker.ifttt.com/trigger/esp32_status/with/key/twoj_klucz_ifttt"
#define WEBHOOK_CHANGE "https://maker.ifttt.com/trigger/esp32_change/with/key/twoj_klucz_ifttt"

static const char *TAG = "MONITOR";
static char prev_checksum[33] = "00000000000000000000000000000000"; // 32 znaki + null

// Funkcja obsługująca zdarzenia Wi-Fi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Połączono z Wi-Fi i uzyskano IP");
    }
}

// Inicjalizacja Wi-Fi
void wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

// Pobieranie strony i obliczanie sumy kontrolnej
static void get_page_and_checksum(char *checksum) {
    esp_http_client_config_t config = {
        .url = TARGET_URL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_open(client, 0);
    char buffer[1024];
    int len = esp_http_client_fetch_headers(client);
    int total_len = 0;

    unsigned char md5_result[16];
    mbedtls_md5_context md5_ctx;
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);

    while ((len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        mbedtls_md5_update(&md5_ctx, (unsigned char *)buffer, len);
        total_len += len;
    }

    mbedtls_md5_finish(&md5_ctx, md5_result);
    mbedtls_md5_free(&md5_ctx);

    for (int i = 0; i < 16; i++) {
        sprintf(checksum + (i * 2), "%02x", md5_result[i]);
    }
    esp_http_client_cleanup(client);
}

// Wysyłanie webhooka
static void send_webhook(const char *url, const char *message) {
    char full_url[256];
    snprintf(full_url, sizeof(full_url), "%s?value1=%s", url, message);

    esp_http_client_config_t config = {
        .url = full_url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

// Główna funkcja aplikacji
void app_main(void) {
    // Inicjalizacja NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init();

    char checksum[33];
    char message[128];

    while (1) {
        // Pobierz sumę kontrolną
        get_page_and_checksum(checksum);

        // Pobierz aktualny czas (prosty timestamp)
        time_t now;
        time(&now);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0'; // Usuń znak nowej linii

        // Webhook statusu
        snprintf(message, sizeof(message), "Status: %s, Checksum: %s", time_str, checksum);
        send_webhook(WEBHOOK_STATUS, message);

        // Sprawdź zmianę sumy kontrolnej
        if (strcmp(checksum, prev_checksum) != 0) {
            snprintf(message, sizeof(message), "@everyone Suma kontrolna zmieniła się: %s", checksum);
            send_webhook(WEBHOOK_CHANGE, message);
            strcpy(prev_checksum, checksum);
        }

        // Czekaj 5 minut
        vTaskDelay(300000 / portTICK_PERIOD_MS); // 300000 ms = 5 minut
    }
}
