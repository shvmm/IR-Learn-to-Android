#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdatomic.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_rx.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"

// Classic ESP32 lacks RX ping-pong: reserve all 8 x 64 hardware symbols.
// ESP32-S3 uses ping-pong copies into a larger user buffer, without DMA.
#if CONFIG_IDF_TARGET_ESP32
#define HW_SYMBOLS 512
#define CAPTURE_SYMBOLS 512
#elif CONFIG_IDF_TARGET_ESP32S3
#define HW_SYMBOLS 192
#define CAPTURE_SYMBOLS 2048
#else
#error "This project targets ESP32 and ESP32-S3."
#endif

typedef struct {
    size_t count;
    int64_t end_us;
    rmt_symbol_word_t symbols[CAPTURE_SYMBOLS];
} capture_t;
typedef struct { size_t count; int64_t end_us; } done_t;
static QueueHandle_t done_queue, output_queue;
static rmt_symbol_word_t rx_buffer[CAPTURE_SYMBOLS];
static capture_t captured; // Only receiver task writes this staging object.
static atomic_uint dropped;

static bool rx_done(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *event, void *ctx)
{
    (void)channel;
    (void)ctx;
    BaseType_t wake = pdFALSE;
    done_t done = { .count = event->num_symbols, .end_us = esp_timer_get_time() };
    // Partial reception is disabled. The buffer stays valid until rearmed.
    xQueueSendFromISR(done_queue, &done, &wake);
    return wake == pdTRUE;
}

static void hello(void)
{
    printf("{\"type\":\"hello\",\"schema\":1,\"firmware\":\"ir-code-studio/0.1\",\"target\":\"%s\",\"idf\":\"%s\",\"gpio\":%d,\"idle_us\":%d,\"capacity_symbols\":%d,\"dropped\":%u}\n",
           CONFIG_IDF_TARGET, esp_get_idf_version(), CONFIG_IR_RX_GPIO,
           CONFIG_IR_IDLE_US, CAPTURE_SYMBOLS, atomic_load(&dropped));
}

static void serial_task(void *ctx)
{
    (void)ctx;
    // Heap/static storage avoids placing the large capture on the task stack.
    static capture_t frame;
    unsigned sequence = 0;
    hello();
    for (;;) {
        if (xQueueReceive(output_queue, &frame, pdMS_TO_TICKS(2000)) != pdTRUE) {
            hello();
            fflush(stdout);
            continue;
        }
        printf("{\"type\":\"capture\",\"schema\":1,\"sequence\":%u,\"end_us\":%" PRId64 ",\"carrier_hz\":null,\"truncated\":%s,\"dropped\":%u,\"raw_us\":[",
               sequence++, frame.end_us, frame.count >= CAPTURE_SYMBOLS - 1 ? "true" : "false", atomic_load(&dropped));
        bool first = true;
        for (size_t i = 0; i < frame.count; i++) {
            unsigned duration[2] = {frame.symbols[i].duration0, frame.symbols[i].duration1};
            unsigned level[2] = {frame.symbols[i].level0, frame.symbols[i].level1};
            for (unsigned j = 0; j < 2; j++) {
                if (duration[j] == 0) continue;
#ifdef CONFIG_IR_ACTIVE_LOW
                bool mark = level[j] == 0;
#else
                bool mark = level[j] != 0;
#endif
                if (first && !mark) continue; // Ignore any leading idle.
                printf("%s%d", first ? "" : ",", mark ? (int)duration[j] : -(int)duration[j]);
                first = false;
            }
        }
        puts("]}");
        fflush(stdout);
    }
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!GPIO_IS_VALID_GPIO(CONFIG_IR_RX_GPIO)) {
        puts("{\"type\":\"error\",\"message\":\"Invalid IR input GPIO; change IR Code Studio in menuconfig.\"}");
        return;
    }
    done_queue = xQueueCreate(1, sizeof(done_t));
    output_queue = xQueueCreate(6, sizeof(capture_t));
    configASSERT(done_queue && output_queue);
    rmt_channel_handle_t rx;
    rmt_rx_channel_config_t config = {
        .gpio_num = CONFIG_IR_RX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = HW_SYMBOLS,
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&config, &rx));
    rmt_rx_event_callbacks_t callbacks = { .on_recv_done = rx_done };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx, &callbacks, NULL));
    ESP_ERROR_CHECK(rmt_enable(rx));
    configASSERT(xTaskCreate(serial_task, "ir_serial", 4096, NULL, 4, NULL) == pdPASS);
    vTaskPrioritySet(NULL, 10);
    rmt_receive_config_t receive = {
        .signal_range_min_ns = 2000,
        .signal_range_max_ns = CONFIG_IR_IDLE_US * 1000,
    };
    for (;;) {
        ESP_ERROR_CHECK(rmt_receive(rx, rx_buffer, sizeof(rx_buffer), &receive));
        done_t done;
        xQueueReceive(done_queue, &done, portMAX_DELAY);
        captured.count = done.count > CAPTURE_SYMBOLS ? CAPTURE_SYMBOLS : done.count;
        captured.end_us = done.end_us;
        memcpy(captured.symbols, rx_buffer, captured.count * sizeof(rx_buffer[0]));
        if (xQueueSend(output_queue, &captured, 0) != pdTRUE) atomic_fetch_add(&dropped, 1);
        // Rearm before the lower-priority task serialises the queued capture.
    }
}
