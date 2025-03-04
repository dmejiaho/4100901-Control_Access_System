#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include "ring_buffer.h"

// Define sizes for each ring buffer
#define UART2_BUFFER_SIZE 64
#define UART3_BUFFER_SIZE 64
#define KEYPAD_BUFFER_SIZE 64

// Expose ring buffer instances
extern ring_buffer_t rb_uart2;
extern ring_buffer_t rb_uart3;
extern ring_buffer_t rb_keypad;

void BufferManager_Init(void);

#endif // BUFFER_MANAGER_H
