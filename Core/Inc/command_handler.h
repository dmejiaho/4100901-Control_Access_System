#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "ring_buffer.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* last_activity_tick is maintained in main.c */
extern uint32_t last_activity_tick;

/* Processes the command buffers from UART2, UART3 and Keypad */
void process_all_commands(ring_buffer_t *rx_buffer_uart2, char *cmd_uart2,
                          ring_buffer_t *rx_buffer_uart3, char *cmd_uart3,
                          ring_buffer_t *rx_buffer_keypad, char *cmd_keypad);

/* Inactivity and mistake sleep modes */
void sleep_mode_inactivity(void);
void sleep_mode_mistake(void);



#ifdef __cplusplus
}
#endif

#endif  /* COMMAND_HANDLER_H */
