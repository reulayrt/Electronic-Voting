// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/param.h>

// RTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

// Wifi Communication
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

// UDP
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#include <ctype.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "soc/rmt_reg.h"
#include "driver/uart.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

// RMT definitions
#define RMT_TX_CHANNEL    1     // RMT channel for transmitter
#define RMT_TX_GPIO_NUM   25    // GPIO number for transmitter signal -- A1
#define RMT_CLK_DIV       100   // RMT counter clock divider
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   // RMT counter value for 10 us.(Source clock is APB clock)
#define rmt_item32_tIMEOUT_US   9500     // RMT receiver timeout value(us)

// UART definitions
#define UART_TX_GPIO_NUM 26 // A0
#define UART_RX_GPIO_NUM 34 // A2
#define BUF_SIZE (1024)

// Hardware interrupt definitions
#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1
#define GPIO_INPUT_IO_2       39

/* Define macros for timer */
#define TIMER_DIVIDER         16                                  
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)      
#define TIMER_INTERVAL_SEC    (0.1)                                
#define TEST_WITH_RELOAD      1                                     
#define TIMER_HOURS           0                                    
#define TIMER_MINUTES         1       

#define TIMEOUT_ELECTION      10

// LED Output pins definitions
#define BLUEPIN   14
#define GREENPIN  32
#define REDPIN    33
#define ONBOARD   13

// Default ID/color
#define ID 2
#define COLOR 'R'

/* Define macros for ADC */
#define DEFAULT_VREF    1100        // Default ADC reference voltage in mV

// #ifdef CONFIG_EXAMPLE_IPV4
#define HOST_IP_ADDR "192.168.1.108"        // FOB1

#define PORT 64209

#define JS_IP_ADDR "192.168.1.112"
#define JS_PORT 3000

/* Tags for log messages */
static const char *TAGTX = "electionTx";
static const char *TAGRX = "electionRx";

// Variables for my ID, minVal and status plus string fragments
char start = 0x1B;
char myID = (char) ID;
char myColor = (char) COLOR;
char senderID = (char) ID;
char senderColor = (char) COLOR;
char recv_rx_buffer[128];
int sendFlag = 0;
int len_out = 4;
int isLeader = 0;
int counter = 0;

int recHeartbeat = 0;

// Mutex (for resources), and Queues (for button)
SemaphoreHandle_t mux = NULL;
static xQueueHandle gpio_evt_queue = NULL;

// System tags
static const char *TAG_SYSTEM = "system";       // For debug logs

// Button interrupt handler -- add to queue
static void IRAM_ATTR gpio_isr_handler(void* arg){
  uint32_t gpio_num = (uint32_t) arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/* Handlers for each task */
TaskHandle_t udpServerHandle;   // UDP receive task
TaskHandle_t udpClientHandle;   // UDP send task

// Utilities ///////////////////////////////////////////////////////////////////

// Checksum
char genCheckSum(char *p, int len) {
  char temp = 0;
  for (int i = 0; i < len; i++){
    temp = temp^p[i];
  }
  // printf("%X\n",temp);

  return temp;
}
bool checkCheckSum(uint8_t *p, int len) {
  char temp = (char) 0;
  bool isValid;
  for (int i = 0; i < len-1; i++){
    temp = temp^p[i];
  }
  // printf("Check: %02X ", temp);
  if (temp == p[len-1]) {
    isValid = true; }
  else {
    isValid = false; }
  return isValid;
}

// Init Functions //////////////////////////////////////////////////////////////
// RMT tx init
static void rmt_tx_init() {
    rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = 50;
    // Carrier Frequency of the IR receiver
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = 1;
    rmt_tx.tx_config.carrier_en = 1;
    // Never idle -> aka ontinuous TX of 38kHz pulses
    rmt_tx.tx_config.idle_level = 1;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);
}

// Configure UART
static void uart_init() {
  // Basic configs
  uart_config_t uart_config = {
      .baud_rate = 1200, // Slow BAUD rate
      .data_bits = UART_DATA_8_BITS,
      .parity    = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(UART_NUM_1, &uart_config);

  // Set UART pins using UART0 default pins
  uart_set_pin(UART_NUM_1, UART_TX_GPIO_NUM, UART_RX_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  // Reverse receive logic line
  uart_set_line_inverse(UART_NUM_1,UART_SIGNAL_RXD_INV);

  // Install UART driver
  uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
}


// GPIO init for LEDs
static void led_init() {
    gpio_pad_select_gpio(13);
    gpio_pad_select_gpio(BLUEPIN);
    gpio_pad_select_gpio(GREENPIN);
    gpio_pad_select_gpio(REDPIN);
    gpio_pad_select_gpio(ONBOARD);
    gpio_set_direction(BLUEPIN, 13);
    gpio_set_direction(BLUEPIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREENPIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(REDPIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(ONBOARD, GPIO_MODE_OUTPUT);
    gpio_set_level(13, 0);
}

// Button interrupt init
static void button_init() {
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_intr_enable(GPIO_INPUT_IO_1 );
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task

    gpio_pad_select_gpio(GPIO_INPUT_IO_2);
    gpio_set_direction(GPIO_INPUT_IO_2,GPIO_MODE_INPUT);
}

/* Function to initialize Wifi */
void init_wifi(){
    // Initialize Wifi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
}

////////////////////////////////////////////////////////////////////////////////

// Tasks ///////////////////////////////////////////////////////////////////////
// Button task -- rotate through myIDs
void button_task(){
  uint32_t io_num;
  while(1) {
    if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      // xSemaphoreTake(mux, portMAX_DELAY);
      // if (myID == 3) {
      //   myID = 1;
      // }
      // else {
      //   myID++;
      // }
      // xSemaphoreGive(mux);
      char *data_out = (char *) malloc(len_out);
      xSemaphoreTake(mux, portMAX_DELAY);
      data_out[0] = start;
      data_out[1] = (char) myColor;
      data_out[2] = (char) myID;
      data_out[3] = genCheckSum(data_out,len_out-1);
      // ESP_LOG_BUFFER_HEXDUMP(TAG_SYSTEM, data_out, len_out, ESP_LOG_INFO);

      gpio_set_level(13, 1);
      
      uart_write_bytes(UART_NUM_1, data_out, len_out);
      xSemaphoreGive(mux);
      printf("Button pressed Send Data.\n");
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(13, 0);
  }
}

//change color button
void button_task_change(){
  uint32_t io_num;
  while(1) {
    if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      xSemaphoreTake(mux, portMAX_DELAY);
      if (myColor == 'R') {
      myColor = 'G';
        }
        else if (myColor == 'G') {
          myColor = 'Y';
        }
        else if (myColor == 'Y') {
          myColor = 'R';
      }
      xSemaphoreGive(mux);
      printf("Button pressed Change Color.\n");
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

void second_button(){
  int buttonflag;
  while(1){
    buttonflag = gpio_get_level(GPIO_INPUT_IO_2);
    if (!buttonflag){
      printf("Button pressed Change Color.\n");
      counter += 1;
      if (myColor == 'R') {
      myColor = 'G';
        }
        else if (myColor == 'G') {
          myColor = 'Y';
        }
        else if (myColor == 'Y') {
          myColor = 'R';
      }
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    if (counter >= 3) {
        gpio_set_level(13,1);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}


// Receives task -- looks for Start byte then stores received values
void recv_task(){
    // Buffer for input data
  uint8_t *data_in = (uint8_t *) malloc(BUF_SIZE);
  while (1) {
    sendFlag = 0;
    int len_in = uart_read_bytes(UART_NUM_1, data_in, BUF_SIZE, 20 / portTICK_RATE_MS);
    if (len_in >0) {
      if (data_in[0] == start) {
        if (checkCheckSum(data_in,len_out)) {
          ESP_LOG_BUFFER_HEXDUMP(TAG_SYSTEM, data_in, len_out, ESP_LOG_INFO);
          //myColor = (char)data_in[1];
          gpio_set_level(13, 1);
          switch((char)data_in[1]){
            case 'R' : // Red
                gpio_set_level(GREENPIN, 0);
                gpio_set_level(REDPIN, 1);
                gpio_set_level(BLUEPIN, 0);
                // printf("Current state: %c\n",myColor);
                break;
            case 'Y' : // Yellow
                gpio_set_level(GREENPIN, 0);
                gpio_set_level(REDPIN, 0);
                gpio_set_level(BLUEPIN, 1);
                // printf("Current state: %c\n",myColor);
                break;
            case 'G' : // Green
                gpio_set_level(GREENPIN, 1);
                gpio_set_level(REDPIN, 0);
                gpio_set_level(BLUEPIN, 0);
                // printf("Current state: %c\n",myColor);
                break;
            }
          vTaskDelay(50 / portTICK_PERIOD_MS);
          senderID = (char)data_in[2];
          senderColor = (char)data_in[1];
          sendFlag = 1;
          if (isLeader) {
              sprintf(recv_rx_buffer, "%x%x%x", start, senderID, senderColor);
          }
        }
      }
    }
    else{
      // printf("Nothing received.\n");
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
    gpio_set_level(13, 0);
  }
  free(data_in);
}

// LED task to light LED based on traffic state
void led_task(){
  while(1) {
    switch((int)myColor){
      case 'R' : // Red
        gpio_set_level(GREENPIN, 0);
        gpio_set_level(REDPIN, 1);
        gpio_set_level(BLUEPIN, 0);
        // printf("Current state: %c\n",myColor);
        break;
      case 'Y' : // Yellow
        gpio_set_level(GREENPIN, 0);
        gpio_set_level(REDPIN, 0);
        gpio_set_level(BLUEPIN, 1);
        // printf("Current state: %c\n",myColor);
        break;
      case 'G' : // Green
        gpio_set_level(GREENPIN, 1);
        gpio_set_level(REDPIN, 0);
        gpio_set_level(BLUEPIN, 0);
        // printf("Current state: %c\n",myColor);
        break;
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// LED task to blink onboard LED based on ID
void id_task(){
  while(1) {
    for (int i = 0; i < (int) myID; i++) {
      gpio_set_level(ONBOARD,1);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      gpio_set_level(ONBOARD,0);
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void udp_client_task(void *pvParameters)
{
    // First FOB
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    struct sockaddr_in dest_addr;
    int sock = 0;

    while (1) {
        if (!isLeader) {
            //socket for fob connections
            //struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(PORT);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
            inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

            sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
            if (sock < 0) {
                ESP_LOGE(TAGTX, "Unable to create socket: errno %d", errno);
                break;
            }
            ESP_LOGI(TAGTX, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);
        }

        if (isLeader) {
            //struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(JS_IP_ADDR);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(JS_PORT);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
            inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

            sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
            if (sock < 0) {
                ESP_LOGE(TAGTX, "Unable to create socket: errno %d", errno);
                break;
            }
            ESP_LOGI(TAGTX, "Socket created, sending to %s:%d", JS_IP_ADDR, JS_PORT);
        }


        while (1) {

            // Reset payload
            char payload[200];

            // Set payload
            if (!isLeader) {
                sprintf(payload, "%x%x%x%x", start, senderID, senderColor, (char)isLeader);
                if (sendFlag == 1) {
                    // Send payload to First FOB
                    int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    if (err < 0) {
                        ESP_LOGE(TAGTX, "Error occurred during sending: errno %d", errno);
                        break;
                    }
                    ESP_LOGI(TAGTX, "Message sent: %s", payload);
                }
            }
        
            if (isLeader && *recv_rx_buffer) {
                int err = sendto(sock, recv_rx_buffer, strlen(recv_rx_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (err < 0) {
                    ESP_LOGE(TAGTX, "Error occurred during sending: errno %d", errno);
                    break;
                }
                
                ESP_LOGI(TAGTX, "Message sent: %s", recv_rx_buffer);
                memset(recv_rx_buffer, '\0', sizeof(recv_rx_buffer));
            }
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAGTX, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}


/* Task to send payload to server (UDP Server) */
static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    while (1) {

        if (addr_family == AF_INET) {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(PORT);
            ip_protocol = IPPROTO_IP;
        } else if (addr_family == AF_INET6) {
            bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
            dest_addr.sin6_family = AF_INET6;
            dest_addr.sin6_port = htons(PORT);
            ip_protocol = IPPROTO_IPV6;
        }

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAGRX, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAGRX, "Socket created");

#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
        if (addr_family == AF_INET6) {
            // Note that by default IPV6 binds to both protocols, it is must be disabled
            // if both protocols used at the same time (used in CI)
            int opt = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
        }
#endif

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAGRX, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAGRX, "Socket bound, port %d", PORT);

        while (1) {

            ESP_LOGI(TAGRX, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAGTX, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAGRX, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAGRX, "%s", rx_buffer);

                // Set heartbeat flag
                if(rx_buffer[3] == leaderID) {recHeartbeat = 1;}
                
                // Set new leader if leader already chosen or received ID < Fob ID
                if(strlen(rx_buffer) == 5 && rx_buffer[4] == '1') {leaderID = rx_buffer[3];}
                else if(rx_buffer[3] < myID+'0') {leaderID = rx_buffer[3];}

                memcpy(recv_rx_buffer, rx_buffer, sizeof(recv_rx_buffer));
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAGRX, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

// Election  

/* Main election function */
void election(){
    election_ongoing = 1;
    printf(">> Running election...\n");

    // Wait for election to timeout
    for(int i=0; i<TIMEOUT_ELECTION;i++){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Pause election tasks
    vTaskSuspend(udpServerHandle);
    vTaskSuspend(udpClientHandle);
    printf(" - Election tasks paused\n");

    // Change leaderID
    if(myID == leaderID)
        {isLeader = 1;}
    else
        {isLeader = 0;}

    printf(" - Election complete. LeaderID:");
    if(leaderID < 48) {printf("%c", leaderID+'0');}
    else if(leaderID > 57) {printf("%c", leaderID-'0');}
    else {printf("%c", leaderID);}
    printf("\n\n");
    election_ongoing = 0;
}

/* Function to restart the election */
void election_restart(){
    printf(">> Restarting election...\n");

    // Reset payload values
    myID = (char) ID;
    leaderID = (char) ID;
    isLeader = 0;

    // Resume election tasks
    vTaskResume(udpServerHandle);
    vTaskResume(udpClientHandle);

    // Call election
    election();
}

void app_main() {

    // Mutex for current values when sending
    mux = xSemaphoreCreateMutex();

    // Initialize all the things
    init_wifi();
    rmt_tx_init();
    uart_init();
    led_init();
    button_init();

    // Create tasks for receive, send, set gpio, and button
    xTaskCreate(udp_server_task, "udp_server", 4096, (void*)AF_INET, 6, &udpServerHandle);
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, &udpClientHandle);
    xTaskCreate(recv_task, "uart_rx_task", 1024*4, NULL, configMAX_PRIORITIES, NULL);
    // xTaskCreate(send_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(led_task, "set_traffic_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    // xTaskCreate(id_task, "set_id_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(button_task, "button_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(second_button, "button_task_change", 1024*2, NULL, configMAX_PRIORITIES, NULL);

    election();

    // If you are the leader, send out your heartbeat. Else open your ears to hear heartbeat
    if(isLeader) {vTaskResume(udpClientHandle);}
    else {vTaskResume(udpServerHandle);}

    while(1){

        // If you're not the leader, check for heartbeat
        if(!isLeader){
            if(recHeartbeat){
                printf(">> Heartbeat received\n");
                recHeartbeat = 0;   // Reset heartbeat flag
            }
            else{
                printf(">> No heartbeat received in time\n");
                election_restart();
                if(isLeader) {vTaskResume(udpClientHandle);}
                else {vTaskResume(udpServerHandle);}
            }
        }

        else{
            printf("I am the leader.\n");
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
