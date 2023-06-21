#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/dac.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UART_DEVICE_NODE DT_NODELABEL(usart1)
#define MSG_SIZE 32

#define DAC_DEVICE_NODE DT_NODELABEL(dac1)
#define DAC_CHANNEL_ID 1 // Use DAC_OUT2 (PA5) as the output pin
#define DAC_RESOLUTION 12 // 12-bit resolution

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
static const struct device *const dac_dev = DEVICE_DT_GET(DT_LABEL(DAC_DEVICE_NODE));

static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

bool response_complete = false;
/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data) {
  uint8_t c;

  if (!uart_irq_update(uart_dev)) {
    return;
  }

  if (!uart_irq_rx_ready(uart_dev)) {
    return;
  }
  
  /* read until FIFO empty */
  while (uart_fifo_read(uart_dev, &c, 1) == 1) {
    if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
      /* terminate string */
      rx_buf[rx_buf_pos] = '\0';

      /* if queue is full, message is silently dropped */
      k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

      /* reset the buffer (it was copied to the msgq) */
      rx_buf_pos = 0;

      /* set flag to indicate response is complete */
      response_complete = true;
    } else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
      rx_buf[rx_buf_pos++] = c;
    }
    /* else: characters beyond buffer size are dropped */
  }
}

void print_uart(char *buf) {
  int msg_len = strlen(buf);
  for (int i = 0; i < msg_len; i++) {
    uart_poll_out(uart_dev, buf[i]);
  }
}

// parse input in this format #MOXY 223011 27125 0
// return a po2 value as double as 0.223011
double parse_po2(const char *input) {
  char buffer[64];
  const char *start = strchr(input, ' ');
  
  if (start == NULL) {
    return 0;
  }

  start++; // Move past the space
  const char *end = strchr(start, ' ');

  if (end == NULL) {
    return 0;
  }

  strncpy(buffer, start, end - start);
  buffer[end - start] = '\0';

  int number = atoi(buffer);

  return (double)number / 1000000;
}

// mV = 50.633(pO2) - 0.633
double po2_to_v(double po2) {
  return (50.633 * po2 - 0.633)/1000;
}

int main(void) {

  if (!dac_dev) {
    printk("Cannot find %s!\n", DAC_DEVICE_NODE);
    return 1;
  }

  if (!device_is_ready(uart_dev)) {
    printk("UART device not found!");
    return 1;
  }

  struct uart_config cfg = {
    .baudrate = 19200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
  };

  struct dac_channel_cfg channel_cfg = {
    .channel_id = DAC_CHANNEL_ID,
    .resolution = DAC_RESOLUTION,
  };

  if (uart_configure(uart_dev, &cfg) < 0) {
    printk("Cannot configure USART1\n");
    return 1;
  }

  if (dac_channel_setup(dac_dev, &channel_cfg) != 0) {
    printk("Setting up of channel %d failed!\n", DAC_CHANNEL_ID);
    return 1;
  }

  uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
  uart_irq_rx_enable(uart_dev);

  while (1) {
    print_uart("#MOXY\r\n");
    /* check if response is complete */
    if (response_complete) {
      response_complete = false;

      /* print the full response */
      char response[MSG_SIZE];
      while (k_msgq_get(&uart_msgq, &response, K_NO_WAIT) == 0) {
        printk("Response: %s\n", response);
        double po2 = parse_po2(response);
        double v  = po2_to_v(po2);
        printk("PO2: %.6f mV: %.6f\n", po2, v);
        uint32_t dac_value = (uint32_t)(v * ((1 << DAC_RESOLUTION) - 1) / 3.3);
        if (dac_write_value(dac_dev, DAC_CHANNEL_ID, dac_value) != 0) {
          printk("Failed to write DAC value!\n");
        }
      }
    }
    k_sleep(K_MSEC(1000)); 
  }

  return 0;
}
