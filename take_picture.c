#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_http_client.h"


// === Camera GPIO Configuration ===
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// === SPI Slave Pins (choose pins that don’t conflict with camera/flash) ===
#define PIN_MISO  15   // SPI MISO
#define PIN_MOSI   2   // SPI MOSI
#define PIN_CLK    4   // SPI Clock
#define PIN_CS    16   // Chip select from STM32

// === Constants ===
#define MAX_TRANSFER_SIZE 64

static const char *TAG = "ESP32-CAM";

// === Initialize Camera ===
void init_camera() {
    camera_config_t config = {
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer = LEDC_TIMER_0,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_sscb_sda = SIOD_GPIO_NUM,
        .pin_sscb_scl = SIOC_GPIO_NUM,
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .xclk_freq_hz = 20000000,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 1
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Camera initialized");
}

// === Upload image to Flask server and get status ===
bool send_image_and_get_status(camera_fb_t *fb) {
    esp_http_client_config_t config = {
        .url ="http://192.168.1.19:5000/upload",  // ✅ Update this to your PC IP 
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP client init failed");
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "image/jpeg");
    esp_http_client_set_post_field(client, (const char *)fb->buf, fb->len);

    esp_err_t err = esp_http_client_perform(client);
    bool valid = false;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            char response[32] = {0};
            esp_http_client_read(client, response, sizeof(response) - 1);
            ESP_LOGI(TAG, "Server response: %s", response);
            if (strcmp(response, "valid") == 0) {
                valid = true;
            }
        } else {
            ESP_LOGW(TAG, "HTTP status code: %d", status);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return valid;
}

// === Main app ===
void app_main(void) {
    // Initialize camera
    init_camera();

    // SPI Bus config
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX_TRANSFER_SIZE
    };

    // SPI Slave config
    spi_slave_interface_config_t slave_config = {
        .spics_io_num = PIN_CS,
        .flags = 0,
        .queue_size = 2,
        .mode = 0,
        .post_setup_cb = NULL,
        .post_trans_cb = NULL
    };

    // Enable pullups (important for SPI lines)
    gpio_set_pull_mode(PIN_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_CS, GPIO_PULLUP_ONLY);

    // Initialize SPI slave
    esp_err_t ret = spi_slave_initialize(HSPI_HOST, &bus_config, &slave_config, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SPI slave: %s", esp_err_to_name(ret));
        return;
    }

    // Word-aligned SPI buffers
    WORD_ALIGNED_ATTR char send_buffer[MAX_TRANSFER_SIZE];
    WORD_ALIGNED_ATTR char receive_buffer[MAX_TRANSFER_SIZE];

    while (1) {
        // Clear buffers
        memset(send_buffer, 0, sizeof(send_buffer));
        memset(receive_buffer, 0, sizeof(receive_buffer));

        // Receive transaction from STM32
        spi_slave_transaction_t trans = {
            .length = 8 * sizeof(send_buffer),
            .tx_buffer = send_buffer,
            .rx_buffer = receive_buffer,
        };

        // Wait for command (blocking)
        ret = spi_slave_transmit(SPI2_HOST, &trans, pdMS_TO_TICKS(1000)); // 1 second timeout
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI receive failed: %s", esp_err_to_name(ret));
            continue;
        }

        // Check if master sent 'P' for "Present car"
        if (receive_buffer[0] == 'P') {
            ESP_LOGI(TAG, "Command received: capturing image...");

            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
                continue;
            }

            // Upload image and get validation
            bool valid = send_image_and_get_status(fb);
            esp_camera_fb_return(fb);

            // Prepare response
            if (valid) {
                snprintf(send_buffer, sizeof(send_buffer), "V");  // Valid
            } else {
                snprintf(send_buffer, sizeof(send_buffer), "R");  // Rejected
            }

            // Send response back to STM32
            spi_slave_transaction_t response_trans = {
                .length = 8 * sizeof(send_buffer),
                .tx_buffer = send_buffer,
                .rx_buffer = receive_buffer,
            };

            // Small delay to avoid overwhelming SPI master
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}