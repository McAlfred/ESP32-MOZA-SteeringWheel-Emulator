#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// Include I2C slave driver header
#include "driver/i2c_slave.h"

// Define static tag for logging purposes
static const char *TAG = "i2c_slave";

// Define I2C slave SCL (clock) pin number
#define I2C_SLAVE_SCL_IO           40
// Define I2C slave SDA (data) pin number
#define I2C_SLAVE_SDA_IO           39
// Define which I2C port to use (0 or 1)
#define I2C_SLAVE_NUM              I2C_NUM_0
// Define I2C slave address (7-bit address)
#define I2C_SLAVE_ADDR             0x09
// Define data buffer length in bytes
#define DATA_BUFFER_LEN            128

// Array to hold reply data for I2C responses
uint8_t i2c_reply_back[5] = {0x00, 0x04, 0x02, 0x00, 0x00};
// Flag to indicate which reply data to send
uint8_t reply_flag;

// Structure to hold I2C slave context data
typedef struct {
    QueueHandle_t event_queue;       // Queue for handling I2C events
    uint8_t data;                    // Variable to store received data
    uint32_t length;                 // Length of data transaction
    i2c_slave_dev_handle_t handle;   // I2C slave device handle
} i2c_slave_context_t;

// Enumeration for different I2C slave events
typedef enum {
    I2C_SLAVE_EVT_RX,    // Receive event
    I2C_SLAVE_EVT_TX,    // Transmit event
} i2c_slave_event_t;

// Callback function for handling I2C master request events
static bool i2c_slave_request_cb(i2c_slave_dev_handle_t i2c_slave,
                                 const i2c_slave_request_event_data_t *evt_data,
                                 void *arg){
    // Cast the argument to our context structure
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;
    // Set event type to transmit
    i2c_slave_event_t evt = I2C_SLAVE_EVT_TX;
    // Variable to track if task should be woken
    BaseType_t xTaskWoken = 0;

    // Get the appropriate reply message based on flag
    uint8_t reply_message = i2c_reply_back[reply_flag];
    // Variable to track number of bytes written
    uint32_t write_len;
    // Send the reply message over I2C with 1000ms timeout
    ESP_ERROR_CHECK(i2c_slave_write(context->handle, &reply_message, 1, &write_len, 1000));

    // Send event to queue from ISR context
    xQueueSendFromISR(context->event_queue, &evt, &xTaskWoken);
    // Return whether task should be woken
    return xTaskWoken;
}

// Callback function for handling completed I2C receive events
static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave,
                                 const i2c_slave_rx_done_event_data_t *edata,
                                 void *arg){
    // Cast the argument to our context structure
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;
    // Set event type to receive
    i2c_slave_event_t evt = I2C_SLAVE_EVT_RX;
    // Variable to track if task should be woken
    BaseType_t xTaskWoken = 0;

    // Store the received data from buffer
    context->data = *edata->buffer;
    // Set reply flag based on received command
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

    // Send event to queue from ISR context
    xQueueSendFromISR(context->event_queue, &evt, &xTaskWoken);
    // Return whether task should be woken
    return xTaskWoken;
}

// Task function for processing I2C slave events
static void i2c_slave_task(void *arg){
    // Cast the argument to our context structure
    i2c_slave_context_t *context = (i2c_slave_context_t *)arg;

    // Infinite loop for task
    while (1){
        // Variable to hold received event
        i2c_slave_event_t evt;
        // Wait for event from queue with 10ms timeout
        if (xQueueReceive(context->event_queue, &evt, 10) == pdTRUE){
            // Handle receive event
            if (evt == I2C_SLAVE_EVT_RX){
                ESP_LOGI(TAG, "Command data received: 0x%x", context->data);
            }

            // Handle transmit event
            if (evt == I2C_SLAVE_EVT_TX){                
                // Get the sent reply message
                uint8_t reply_message = i2c_reply_back[reply_flag];
                ESP_LOGW(TAG, "I2C replied data 0x%x", reply_message);
            }
        }
    }
    // Delete task (unreachable in infinite loop)
    vTaskDelete(NULL);
}

// Main application entry point
void app_main(void)
{
    // Create static instance of context structure initialized to zero
    static i2c_slave_context_t context = {0};
    // Create event queue with 16 items, each size of i2c_slave_event_t
    context.event_queue = xQueueCreate(16, sizeof(i2c_slave_event_t));
    // Check if queue creation failed
    if (!context.event_queue) {
        ESP_LOGE(TAG, "Event Queue failed.");
        return;
    }

    ESP_LOGI(TAG, "I2C initializing.");

    // Configure I2C slave parameters
    i2c_slave_config_t conf_slave = {
        .i2c_port = I2C_SLAVE_NUM,          // I2C port number
        .sda_io_num = I2C_SLAVE_SDA_IO,     // SDA pin number
        .scl_io_num = I2C_SLAVE_SCL_IO,     // SCL pin number
        .clk_source = I2C_CLK_SRC_DEFAULT,  // Clock source selection
        .send_buf_depth = DATA_BUFFER_LEN,  // Transmit buffer depth
        .receive_buf_depth = DATA_BUFFER_LEN,// Receive buffer depth
        .slave_addr = I2C_SLAVE_ADDR,       // Slave address
        .addr_bit_len = I2C_ADDR_BIT_7,     // 7-bit address mode
    };
    // Create new I2C slave device with configuration
    ESP_ERROR_CHECK(i2c_new_slave_device(&conf_slave, &context.handle));

    // Configure I2C slave event callbacks
    i2c_slave_event_callbacks_t cbs = {
        .on_receive = i2c_slave_receive_cb,  // Callback for receive completion
        .on_request = i2c_slave_request_cb,  // Callback for master request
    };
    // Register callbacks with the I2C slave device
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(context.handle, &cbs, &context));

    ESP_LOGI(TAG, "I2C initialized.");

    // Create I2C slave task with 4096 bytes stack, priority 10
    xTaskCreate(i2c_slave_task, "i2c_slave_task", 4096, &context, 10, NULL);
}
