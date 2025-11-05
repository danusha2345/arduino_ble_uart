/**
 * @file wifi_service.c
 * @brief WiFi AP и TCP сервер для передачи GPS данных
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "config.h"
#include "common.h"

static const char *TAG = "WiFi";

// TCP сервер
static int server_socket = -1;
static int client_sockets[MAX_WIFI_CLIENTS] = {-1, -1, -1, -1};

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/**
 * @brief Инициализация WiFi AP
 */
esp_err_t wifi_service_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi AP...");

    // Инициализация netif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    // WiFi конфигурация
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Регистрация event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));

    // Настройка AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_PASSWORD,
            .max_connection = MAX_WIFI_CLIENTS,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    if (strlen(WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Отключаем power saving
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "WiFi AP started: SSID=%s", WIFI_AP_SSID);

    return ESP_OK;
}

/**
 * @brief Запуск TCP сервера
 */
static int start_tcp_server(void) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(WIFI_PORT);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        return -1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, MAX_WIFI_CLIENTS) < 0) {
        ESP_LOGE(TAG, "Failed to listen: %d", errno);
        close(server_socket);
        return -1;
    }

    ESP_LOGI(TAG, "TCP server listening on port %d", WIFI_PORT);
    return 0;
}

/**
 * @brief Отправка данных всем клиентам
 */
static void send_to_clients(const uint8_t *data, size_t len) {
    for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
        if (client_sockets[i] >= 0) {
            int sent = send(client_sockets[i], data, len, 0);
            if (sent < 0) {
                ESP_LOGW(TAG, "Client %d send failed: %d", i, errno);
                close(client_sockets[i]);
                client_sockets[i] = -1;
            }
        }
    }
}

/**
 * @brief Задача обработки WiFi клиентов
 */
void wifi_task(void *pvParameters) {
    ESP_LOGI(TAG, "WiFi task started on core %d", xPortGetCoreID());

    // Запускаем TCP сервер
    if (start_tcp_server() < 0) {
        ESP_LOGE(TAG, "Failed to start TCP server");
        vTaskDelete(NULL);
        return;
    }

    // Устанавливаем non-blocking режим для сервера
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    uint8_t tx_buffer[512];
    uint8_t rx_buffer[512];

    while (1) {
        // Принимаем новые подключения
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_client = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (new_client >= 0) {
            // Ищем свободный слот
            int slot = -1;
            for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
                if (client_sockets[i] < 0) {
                    slot = i;
                    break;
                }
            }

            if (slot >= 0) {
                client_sockets[slot] = new_client;
                // Устанавливаем non-blocking для клиента
                flags = fcntl(new_client, F_GETFL, 0);
                fcntl(new_client, F_SETFL, flags | O_NONBLOCK);
                // No delay для лучшей производительности
                int nodelay = 1;
                setsockopt(new_client, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                ESP_LOGI(TAG, "Client %d connected from %s", slot,
                         inet_ntoa(client_addr.sin_addr));
            } else {
                ESP_LOGW(TAG, "No free slots for new client");
                close(new_client);
            }
        }

        // Отправляем данные из TX буфера всем клиентам
        if (g_ble_tx_buffer) {
            size_t avail = ring_buffer_available(g_ble_tx_buffer);
            if (avail > 0) {
                size_t to_read = avail > sizeof(tx_buffer) ? sizeof(tx_buffer) : avail;
                size_t read = ring_buffer_read(g_ble_tx_buffer, tx_buffer, to_read);
                if (read > 0) {
                    send_to_clients(tx_buffer, read);
                }
            }
        }

        // Принимаем данные от клиентов
        for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
            if (client_sockets[i] >= 0) {
                int received = recv(client_sockets[i], rx_buffer, sizeof(rx_buffer), 0);

                if (received > 0 && g_ble_rx_buffer) {
                    // Записываем в RX буфер для отправки в GPS
                    ring_buffer_write(g_ble_rx_buffer, rx_buffer, received);
                    ESP_LOGI(TAG, "Client %d sent %d bytes", i, received);
                } else if (received == 0 || (received < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
                    // Клиент отключился
                    ESP_LOGI(TAG, "Client %d disconnected", i);
                    close(client_sockets[i]);
                    client_sockets[i] = -1;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}
