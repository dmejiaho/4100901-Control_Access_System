#include "state_machine.h"
#include "buffer_manager.h"
#include "ring_buffer.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "keypad.h"
#include "ring.h"
#include "locked.h"
#include "unlocked.h"
#include <string.h>
#include <stdio.h>

/* ----- Command definitions ----- */
#define COMMAND_LENGTH 3
static const char CMD_START[]     = "#*#";
static const char CMD_TEMP_OPEN[] = "#0#";   // Temporary open (5 sec)
static const char CMD_CLOSE[]     = "#C#";   // Close door
static const char CMD_STATUS[]    = "#1#";   // Status command
static const char CMD_RESET[]     = "#8#";   // Reset system

/* ----- Internal state variables ----- */
static char current_cmd[COMMAND_LENGTH];
static uint8_t mistake_count = 0;
static uint8_t start_cmd_received = 0;

/* System variables used for door control and timers */
static uint8_t door_permanent = 0;    // 0 = temporary; 1 = permanently open
static uint32_t temp_open_start = 0;    // for temporary open timing (5 sec)

static uint32_t last_activity_tick = 0; // updated on any activity
static uint32_t last_button_press_time = 0;
static uint8_t button_press_count = 0;
static uint32_t debounce_tick = 0;
static uint32_t button_debounce_tick = 0;

/* For keypad external interrupts */
static uint32_t key_pressed_tick = 0;
static uint16_t column_pressed = 0;

/* For OLED and ring button (B2) handling */
static uint8_t prev_b2_state = 0;

/* ----- Local helper: send string via both UARTs ----- */
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
static void uart_send_string(const char *str) {
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
    HAL_UART_Transmit(&huart3, (uint8_t *)str, strlen(str), 100);
}

/* ----- Local helper: process one incoming byte as part of command ----- */
static void process_command_byte(uint8_t byte)
{
    // Slide in the new byte.
    memmove(current_cmd, current_cmd + 1, COMMAND_LENGTH - 1);
    current_cmd[COMMAND_LENGTH - 1] = (char)byte;
    
    // Check for the valid start command.
    if (memcmp(current_cmd, CMD_START, COMMAND_LENGTH) == 0) {
        start_cmd_received = 1;
        uart_send_string("\r\nInput key : Correct.\r\n");
        memset(current_cmd, 0, COMMAND_LENGTH);
        mistake_count = 0;
        return;
    }
    
    // If we are in command mode, compare to valid commands.
    if (start_cmd_received) {
        if (memcmp(current_cmd, CMD_TEMP_OPEN, COMMAND_LENGTH) == 0) {
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
            door_permanent = 0;
            temp_open_start = HAL_GetTick();
            uart_send_string("\r\nDoor opened temporarily (5 sec).\r\n");
            memset(current_cmd, 0, COMMAND_LENGTH);
            mistake_count = 0;
        }
        else if (memcmp(current_cmd, CMD_CLOSE, COMMAND_LENGTH) == 0) {
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);
            door_permanent = 0;
            temp_open_start = 0;
            uart_send_string("\r\nDoor closed.\r\n");
            memset(current_cmd, 0, COMMAND_LENGTH);
            mistake_count = 0;
        }
        else if (memcmp(current_cmd, CMD_STATUS, COMMAND_LENGTH) == 0) {
            uint8_t state = HAL_GPIO_ReadPin(LD4_GPIO_Port, LD4_Pin);
            uart_send_string(state ? "\r\nStatus: OPEN\r\n" : "\r\nStatus: CLOSED\r\n");
            memset(current_cmd, 0, COMMAND_LENGTH);
            mistake_count = 0;
        }
        else if (memcmp(current_cmd, CMD_RESET, COMMAND_LENGTH) == 0) {
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_RESET);
            door_permanent = 0;
            temp_open_start = 0;
            uart_send_string("\r\nSystem reset.\r\n");
            memset(current_cmd, 0, COMMAND_LENGTH);
            mistake_count = 0;
        }
        else {
            // If non-empty and unrecognized, count as mistake.
            if (current_cmd[0] != 0) {
                mistake_count++;
                char msg[50];
                sprintf(msg, "\r\nInvalid command. %d tries remaining until sleep.\r\n", 5 - mistake_count);
                uart_send_string(msg);
                memset(current_cmd, 0, COMMAND_LENGTH);
                if (mistake_count >= 5) {
                    uart_send_string("\r\nToo many invalid commands. Sleeping for 10 sec.\r\n");
                    HAL_SuspendTick();
                    uint32_t sleepStart = HAL_GetTick();
                    while(HAL_GetTick() - sleepStart < 10000) { }
                    HAL_ResumeTick();
                    uart_send_string("\r\nAwake from mistake sleep.\r\n");
                    mistake_count = 0;
                }
            }
        }
    }
}

/* ----- Local helper: process all bytes in a ring buffer ----- */
static void process_ring_buffer(ring_buffer_t* rb)
{
    uint8_t byte;
    while(ring_buffer_read(rb, &byte)) {
        process_command_byte(byte);
    }
}

/* ----- Public API ----- */
void StateMachine_Init(void)
{
    memset(current_cmd, 0, COMMAND_LENGTH);
    mistake_count = 0;
    start_cmd_received = 0;
    door_permanent = 0;
    temp_open_start = 0;
    last_activity_tick = HAL_GetTick();
    button_press_count = 0;
    debounce_tick = 0;
    button_debounce_tick = 0;
    key_pressed_tick = 0;
    column_pressed = 0;
    prev_b2_state = 0;
}

/* The main update function. This function should be called continuously
   from the main loop. It polls all events (ring buffers, keypad input, button
   states, inactivity, OLED updates, auto-close door, etc.). */
void StateMachine_Update(void)
{
    uint32_t now = HAL_GetTick();
    
    /* ----- Inactivity Sleep ----- */
    if ((now - last_activity_tick) >= 30000) {
        uart_send_string("\r\nNo activity for 30 sec. Entering sleep mode.\r\n");
        HAL_SuspendTick();
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        HAL_ResumeTick();
        uart_send_string("\r\nAwake from inactivity sleep.\r\n");
        last_activity_tick = HAL_GetTick();
    }
    
    /* ----- Process incoming data from ring buffers ----- */
    process_ring_buffer(&rb_uart2);
    process_ring_buffer(&rb_uart3);
    process_ring_buffer(&rb_keypad);
    
    /* ----- Handle keypad external input if a column interrupt was signaled ----- */
    if (column_pressed != 0 && (key_pressed_tick + 5 < now)) {
        uint8_t key = keypad_scan(column_pressed);
        ring_buffer_write(&rb_keypad, key);
        uart_send_string("\r\nKeypad input received.\r\n");
        column_pressed = 0;
    }
    
    /* ----- Button B1 Handling (Door control) ----- 
           Single press: if door is not permanently open, open temporarily.
           If already permanently open, then close.
           Double press: open door permanently.
    */
    if (button_press_count > 0 && (now - last_button_press_time) >= 500) {
        if (button_press_count == 1) {
            if (door_permanent) {
                HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);
                door_permanent = 0;
                uart_send_string("\r\nButton: Door closed.\r\n");
            } else {
                HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
                temp_open_start = now;
                uart_send_string("\r\nButton: Door opened temporarily (5 sec).\r\n");
            }
        } else if (button_press_count >= 2) {
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
            door_permanent = 1;
            temp_open_start = 0;
            uart_send_string("\r\nButton: Door opened permanently.\r\n");
        }
        button_press_count = 0;
        last_button_press_time = 0;
    }
    
    /* ----- Auto-Close Temporary Door (after 5 seconds if not permanent) ----- */
    if (!door_permanent && temp_open_start != 0 && (now - temp_open_start >= 5000)) {
        HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);
        uart_send_string("\r\nDoor auto-closed after temporary open.\r\n");
        temp_open_start = 0;
    }
    
    /* ----- Ring Button (B2) and OLED Display Handling ----- */
    uint8_t current_b2_state = HAL_GPIO_ReadPin(B2_GPIO_Port, B2_Pin);
    if (current_b2_state == GPIO_PIN_RESET) {  // Active low: button pressed
        if (!prev_b2_state) {
            uart_send_string("\r\nRing pressed.\r\n");
        }
        HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_SET);
        ssd1306_Fill(Black);
        ssd1306_DrawBitmap(0, 0, ring, 128, 64, White);
        ssd1306_UpdateScreen();
        prev_b2_state = 1;
    } else {
        HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_RESET);
        ssd1306_Fill(Black);
        if (HAL_GPIO_ReadPin(LD4_GPIO_Port, LD4_Pin) == GPIO_PIN_SET) {
            ssd1306_DrawBitmap(0, 0, unlocked, 128, 64, White);
        } else {
            ssd1306_DrawBitmap(0, 0, locked, 128, 64, White);
        }
        ssd1306_UpdateScreen();
        prev_b2_state = 0;
    }
    
    /* ----- Update Heartbeat LED (toggled every 500ms) ----- */
    static uint32_t last_heartbeat = 0;
    if ((last_heartbeat + 500) < now) {
        last_heartbeat = now;
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
    
    /* Update activity tick for any action performed */
    last_activity_tick = now;
}

/* ----- Callbacks from interrupts ----- */

/* Called by main HAL_GPIO_EXTI_Callback. */
void StateMachine_GPIO_Callback(uint16_t GPIO_Pin)
{
    uint32_t now = HAL_GetTick();
    last_activity_tick = now;  // Reset inactivity timer on any GPIO interrupt
    
    if (GPIO_Pin == B1_Pin) {
        if ((now - button_debounce_tick) < 200)
            return;
        button_debounce_tick = now;
        button_press_count++;
        last_button_press_time = now;
    }
    else if (GPIO_Pin == B2_Pin) {
        // B2 is handled by polling in StateMachine_Update.
    }
    else {
        // Assume this is a keypad column.
        if ((debounce_tick + 200) > now)
            return;
        debounce_tick = now;
        key_pressed_tick = now;
        column_pressed = GPIO_Pin;
    }
}

/* Called by the HAL UART Rx complete callback.
   The byte is written to the appropriate ring buffer based on the UART instance. */
void StateMachine_UART_RxCplt(UART_HandleTypeDef* huart, uint8_t byte)
{
    last_activity_tick = HAL_GetTick();
    if (huart->Instance == USART2) {
        ring_buffer_write(&rb_uart2, byte);
        HAL_UART_Transmit(&huart3, &byte, 1, 10);
        HAL_UART_Receive_IT(&huart2, &byte, 1);
    }
    else if (huart->Instance == USART3) {
        ring_buffer_write(&rb_uart3, byte);
        HAL_UART_Transmit(&huart2, &byte, 1, 10);
        HAL_UART_Receive_IT(&huart3, &byte, 1);
    }
}
