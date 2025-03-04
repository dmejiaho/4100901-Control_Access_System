#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "main.h"  // For HAL types and hardware defines

// Initialize the state machine module.
void StateMachine_Init(void);

// Called continuously from the main loop.
void StateMachine_Update(void);

// Called from the HAL GPIO EXTI callback.
void StateMachine_GPIO_Callback(uint16_t GPIO_Pin);

// Called from the HAL UART Rx complete callback.
void StateMachine_UART_RxCplt(UART_HandleTypeDef* huart, uint8_t byte);

#endif // STATE_MACHINE_H
