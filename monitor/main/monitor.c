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

// Definicje
#define WIFI_SSID "router"
#define WIFI_PASS "qwerty000"
#define DISCORD_WEBHOOK_STATUS "https://discord.com/api/webhooks/1352055964282388530/xpAbO0FhnFfSdmzvFUqot9doxNdQiBi5NknNCTU1NFCgXPDmcdzJ_dQjMiBSdKIiySi1"
#define DISCORD_WEBHOOK_CHANGE "https://discord.com/api/webhooks/1352043110716411990/tl118UmEJPpnom3ziT3mo8FD7i3OCSqSWPpZzki4Vj-awxtsriR7bFHNdXg79xRenXMD"
#define DISCORD_TTS "false"
#define WEBSITE_URL "https://sabre.wd1.myworkdayjobs.com/SabreJobs?locationCountry=131d5ac7e3ee4d7b962bdc96e498e412"
static const char *TAG = "DISCORD";

// Certyfikat Discord (taki sam jak w Twoim kodzie)
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
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Disconnected. Reconnecting to Wi-Fi...");
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

    ESP_LOGI(TAG, "Wi-Fi initialized. Connecting to %s...", WIFI_SSID);
}

// Funkcja wysyłająca wiadomość do Discorda
static esp_err_t send_discord_message(const char *content) {
    esp_http_client_config_t config = {
        .url = DISCORD_WEBHOOK_STATUS,
        .cert_pem = DISCORD_CERT,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char json_payload[512];
    snprintf(json_payload, sizeof(json_payload), "{\"content\":\"%s\",\"tts\":%s}", content, DISCORD_TTS);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// Funkcja pobierająca HTML ze strony
static char *fetch_website_html(void) {
    esp_http_client_config_t config = {
        .url = WEBSITE_URL,
        .method = HTTP_METHOD_GET,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Bufor na dane HTML
    char *html_buffer = NULL;
    int html_len = 0;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    // Pobierz długość zawartości
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        esp_http_client_cleanup(client);
        return NULL;
    }

    // Przydziel pamięć na HTML
    html_buffer = (char *)malloc(content_length + 1);
    if (html_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML");
        esp_http_client_cleanup(client);
        return NULL;
    }

    // Pobierz dane
    html_len = esp_http_client_read(client, html_buffer, content_length);
    if (html_len < 0) {
        ESP_LOGE(TAG, "Failed to read HTML content");
        free(html_buffer);
        esp_http_client_cleanup(client);
        return NULL;
    }

    // Dodaj znak końca ciągu
    html_buffer[html_len] = '\0';

    ESP_LOGI(TAG, "Fetched HTML length: %d bytes", html_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return html_buffer;
}

void app_main(void) {
    init_nvs();
    init_wifi();

    // Poczekaj na połączenie Wi-Fi (proste opóźnienie, w praktyce użyj zdarzeń)
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Testowe pobranie HTML
    char *html = fetch_website_html();
    if (html != NULL) {
        ESP_LOGI(TAG, "Website HTML: %s", html);
        free(html); // Zwolnij pamięć po użyciu
    } else {
        ESP_LOGE(TAG, "Failed to fetch website HTML");
    }
    // Wyślij wiadomość testową
    send_discord_message("test");
}