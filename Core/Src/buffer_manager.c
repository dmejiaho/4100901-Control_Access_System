#include "buffer_manager.h"
#include <string.h>

static uint8_t uart2_buffer_mem[UART2_BUFFER_SIZE];
static uint8_t uart3_buffer_mem[UART3_BUFFER_SIZE];
static uint8_t keypad_buffer_mem[KEYPAD_BUFFER_SIZE];

ring_buffer_t rb_uart2;
ring_buffer_t rb_uart3;
ring_buffer_t rb_keypad;

void BufferManager_Init(void)
{
    ring_buffer_init(&rb_uart2, uart2_buffer_mem, UART2_BUFFER_SIZE);
    ring_buffer_init(&rb_uart3, uart3_buffer_mem, UART3_BUFFER_SIZE);
    ring_buffer_init(&rb_keypad, keypad_buffer_mem, KEYPAD_BUFFER_SIZE);
}
