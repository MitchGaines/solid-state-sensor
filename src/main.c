#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

#define UART_DEVICE_NODE DT_NODELABEL(usart1)
#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

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

int main(void) {
  if (!device_is_ready(uart_dev)) {
    printk("UART device not found!");
    return 0;
  }
  struct uart_config cfg = {
    .baudrate = 19200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
  };

  if (uart_configure(uart_dev, &cfg) < 0) {
    printk("Cannot configure USART1\n");
    return 0;
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
      }
    }
    k_sleep(K_MSEC(1000)); 
  }

  return 0;
}
