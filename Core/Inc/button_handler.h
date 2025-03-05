#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called from the EXTI callback to handle button events */
void button_handler_exti_callback(uint16_t GPIO_Pin);

/* Called in the main loop to process B1 button presses */
void button_handler_process(void);

/* Retrieve and clear the pressed column value (for keypad scanning) */
uint16_t button_handler_get_column(void);

/* Process the ring button (B2) and update the OLED display */
void button_handler_process_ring(void);

#ifdef __cplusplus
}
#endif

#endif  /* BUTTON_HANDLER_H */
