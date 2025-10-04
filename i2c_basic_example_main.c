#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_slave.h"

static const char *TAG = "i2c_slave";

#define I2C_SLAVE_SCL_IO           40
#define I2C_SLAVE_SDA_IO           39
#define I2C_SLAVE_NUM              I2C_NUM_0
#define I2C_SLAVE_ADDR             0x09
#define DATA_BUFFER_LEN            128

uint8_t i2c_reply_back[5] = {0x00, 0x04, 0x02, 0x00, 0x00};
uint8_t reply_flag;

typedef struct {
    QueueHandle_t event_queue;
    uint8_t data;
    uint32_t length;
    i2c_slave_dev_handle_t handle;
} i2c_slave_context_t;

typedef enum {
    I2C_SLAVE_EVT_RX,
    I2C_SLAVE_EVT_TX,
} i2c_slave_event_t;

static bool i2c_slave_request_cb(i2c_slave_dev_handle_t i2c_slave,
                                 const i2c_slave_request_event_data_t *evt_data,
                                 void *arg){
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;
    i2c_slave_event_t evt = I2C_SLAVE_EVT_TX;
    BaseType_t xTaskWoken = 0;

    uint8_t reply_message = i2c_reply_back[reply_flag];
    uint32_t write_len;
    ESP_ERROR_CHECK(i2c_slave_write(context->handle, &reply_message, 1, &write_len, 1000));

    xQueueSendFromISR(context->event_queue, &evt, &xTaskWoken);
    return xTaskWoken;
}

static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave,
                                 const i2c_slave_rx_done_event_data_t *edata,
                                 void *arg){
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;
    i2c_slave_event_t evt = I2C_SLAVE_EVT_RX;
    BaseType_t xTaskWoken = 0;

    context->data = *edata->buffer;
    switch (context->data){
        case 0xFC:
            reply_flag = 1;                    
            break;
        
        case 0xF9:
            reply_flag = 2;                    
            break;

        case 0xDD:
            reply_flag = 3;
            break;

        case 0xDE:
            reply_flag = 4;
            break;

        default:
            reply_flag = 1;
            break;
    }

    xQueueSendFromISR(context->event_queue, &evt, &xTaskWoken);
    return xTaskWoken;
}

static void i2c_slave_task(void *arg){
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;

    while (1){
        i2c_slave_event_t evt;
        if (xQueueReceive(context->event_queue, &evt, 10) == pdTRUE){
            if (evt == I2C_SLAVE_EVT_RX){
                ESP_LOGI(TAG, "Command data received: 0x%x", context->data);
            }

            if (evt == I2C_SLAVE_EVT_TX){                
                uint8_t reply_message = i2c_reply_back[reply_flag];
                ESP_LOGW(TAG, "I2C replied data 0x%x", reply_message);
            }
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    static i2c_slave_context_t context = {0};
    context.event_queue = xQueueCreate(16, sizeof(i2c_slave_event_t));
    if (!context.event_queue) {
        ESP_LOGE(TAG, "Event Queue failed.");
        return;
    }

    ESP_LOGI(TAG, "I2C initializing.");

    i2c_slave_config_t conf_slave = {
        .i2c_port = I2C_SLAVE_NUM,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = DATA_BUFFER_LEN,
        .receive_buf_depth = DATA_BUFFER_LEN,
        .slave_addr = I2C_SLAVE_ADDR,
        .addr_bit_len = I2C_ADDR_BIT_7,
    };
    ESP_ERROR_CHECK(i2c_new_slave_device(&conf_slave, &context.handle));

    i2c_slave_event_callbacks_t cbs = {
        .on_receive = i2c_slave_receive_cb,
        .on_request = i2c_slave_request_cb,
    };
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(context.handle, &cbs, &context));

    ESP_LOGI(TAG, "I2C initialized.");

    xTaskCreate(i2c_slave_task, "i2c_slave_task", 4096, &context, 10, NULL);
}
