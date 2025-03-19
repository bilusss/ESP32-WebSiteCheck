#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_http_client.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/md5.h>
#include <esp_spiffs.h>

char WIFI_SSID[32] = "default_ssid";
char WIFI_PASS[64] = "default_pass";
char WEBHOOK_STATUS[128] = ""; // Domyślnie pusty
char WEBHOOK_CHANGE[128] = ""; // Domyślnie pusty

#define TARGET_URL "https://sabre.wd1.myworkdayjobs.com/SabreJobs?locationCountry=131d5ac7e3ee4d7b962bdc96e498e412"

static const char *TAG = "MONITOR";
static char prev_checksum[33] = "00000000000000000000000000000000";

void load_config(void) {
    FILE *f = fopen("/spiffs/config.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Nie udało się otworzyć pliku config.txt");
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = 0;
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (key && value) {
            if (strcmp(key, "SSID") == 0) {
                strncpy(WIFI_SSID, value, sizeof(WIFI_SSID) - 1);
            } else if (strcmp(key, "PASSWORD") == 0) {
                strncpy(WIFI_PASS, value, sizeof(WIFI_PASS) - 1);
            } else if (strcmp(key, "WEBHOOK_STATUS") == 0) {
                strncpy(WEBHOOK_STATUS, value, sizeof(WEBHOOK_STATUS) - 1);
            } else if (strcmp(key, "WEBHOOK_CHANGE") == 0) {
                strncpy(WEBHOOK_CHANGE, value, sizeof(WEBHOOK_CHANGE) - 1);
            }
        }
    }
    fclose(f);

    // Sprawdzenie, czy webhooki zostały wczytane
    if (strlen(WEBHOOK_STATUS) == 0) {
        ESP_LOGE(TAG, "WEBHOOK_STATUS nie został wczytany z config.txt");
    }
    if (strlen(WEBHOOK_CHANGE) == 0) {
        ESP_LOGE(TAG, "WEBHOOK_CHANGE nie został wczytany z config.txt");
    }

    ESP_LOGI(TAG, "Konfiguracja załadowana: SSID=%s", WIFI_SSID);
}

void spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        sleep(5);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Połączono z Wi-Fi i uzyskano IP");
    }
}

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
            .ssid = "",
            .password = "",
        },
    };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

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

static void send_webhook(const char *url, const char *message) {
    // Sprawdź, czy URL jest pusty
    if (strlen(url) == 0) {
        ESP_LOGE(TAG, "URL webhooka jest pusty, pomijam wysyłanie");
        return;
    }

    // Przygotuj dane JSON
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "{\"content\":\"%s\"}", message);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Ustaw nagłówki
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Wysłanie danych
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Webhook wysłany, status=%d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "Błąd wysyłania webhooka: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    spiffs_init();
    load_config();

    wifi_init();

    char checksum[33];
    char message[128];

    while (1) {
        get_page_and_checksum(checksum);

        time_t now;
        time(&now);
        char *time_str = ctime(&now);
        time_str[strlen(time_str) - 1] = '\0';

        // Webhook statusu
        snprintf(message, sizeof(message), "Status: %s, Checksum: %s", time_str, checksum);
        send_webhook(WEBHOOK_STATUS, message);

        // Webhook zmiany
        if (strcmp(checksum, prev_checksum) != 0) {
            snprintf(message, sizeof(message), "@everyone Suma kontrolna zmieniła się: %s", checksum);
            send_webhook(WEBHOOK_CHANGE, message);
            strcpy(prev_checksum, checksum);
        }

        vTaskDelay(300000 / portTICK_PERIOD_MS); // 5 minut
    }
}