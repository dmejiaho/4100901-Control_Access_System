#include "command_handler.h"
#include "door_controller.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* --- Define la máquina de estados para el procesamiento de comandos --- */
// Se utiliza para controlar el flujo de recepción y validación de comandos.
typedef enum {
  STATE_WAIT_FOR_START,
  STATE_WAIT_FOR_COMMAND
} CommandState;
// Variable estática que almacena el estado actual de la máquina de estados.
// Inicialmente, se espera la secuencia de inicio.
static CommandState cmd_state = STATE_WAIT_FOR_START;
// Contador de errores para comandos inválidos. Si se supera un umbral, se activa el modo de suspensión.
static uint8_t mistake_count = 0;

#define COMMAND_LENGTH 3 // Longitud fija de cada comando (3 caracteres)

/* --- Command definitions --- */
const char CMD_START[]     = "#*#";
const char CMD_TEMP_OPEN[] = "#0#";
const char CMD_CLOSE[]     = "#C#";
const char CMD_STATUS[]    = "#1#";
const char CMD_RESET[]     = "#8#";

/* --- Función interna para procesar un buffer circular (ring buffer) --- */
/*
 * Esta función procesa los bytes disponibles en el buffer circular recibido (buffer)
 * y los organiza en un buffer de comando (current_cmd) de longitud COMMAND_LENGTH.
 * Cada vez que se lee un byte:
 *  - Se desplaza el contenido del buffer current_cmd hacia la izquierda.
 *  - Se añade el nuevo byte al final del buffer.
 *
 * Luego, se evalúa el contenido del buffer de comando según el estado de la máquina de estados.
 */
static void process_commands_buffer(ring_buffer_t *buffer, char *current_cmd) {
  uint8_t byte;
  
  while (ring_buffer_read(buffer, &byte)) {
    /* Shift the command buffer left and append the new byte */
    memmove(current_cmd, current_cmd + 1, COMMAND_LENGTH - 1);
    current_cmd[COMMAND_LENGTH - 1] = (char)byte;

    switch(cmd_state) {
      case STATE_WAIT_FOR_START:
        if (memcmp(current_cmd, CMD_START, COMMAND_LENGTH) == 0) {
          cmd_state = STATE_WAIT_FOR_COMMAND;
          uart_send_string("\r\nInput key : Correct.\r\n");
          memset(current_cmd, 0, COMMAND_LENGTH);
          mistake_count = 0;
        }
        break;

      case STATE_WAIT_FOR_COMMAND:
        if (memcmp(current_cmd, CMD_TEMP_OPEN, COMMAND_LENGTH) == 0) {
          open_door_temp();
          uart_send_string("\r\nDoor opened temporarily (5 sec).\r\n");
          memset(current_cmd, 0, COMMAND_LENGTH);
          mistake_count = 0;
          cmd_state = STATE_WAIT_FOR_START;
        }
        else if (memcmp(current_cmd, CMD_CLOSE, COMMAND_LENGTH) == 0) {
          close_door();
          uart_send_string("\r\nDoor closed.\r\n");
          memset(current_cmd, 0, COMMAND_LENGTH);
          mistake_count = 0;
          cmd_state = STATE_WAIT_FOR_START;
        }
        else if (memcmp(current_cmd, CMD_STATUS, COMMAND_LENGTH) == 0) {
          if (door_is_open())
            uart_send_string("\r\nStatus: OPEN\r\n");
          else
            uart_send_string("\r\nStatus: CLOSED\r\n");
          memset(current_cmd, 0, COMMAND_LENGTH);
          mistake_count = 0;
          cmd_state = STATE_WAIT_FOR_START;
        }
        else if (memcmp(current_cmd, CMD_RESET, COMMAND_LENGTH) == 0) {
          close_door();
          HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_RESET);
          uart_send_string("\r\nSystem reset.\r\n");
          memset(current_cmd, 0, COMMAND_LENGTH);
          mistake_count = 0;
          cmd_state = STATE_WAIT_FOR_START;
        }
        else {
          if (current_cmd[0] != 0) {
            mistake_count++;
            char msg[50];
            sprintf(msg, "\r\nInvalid command. %d tries remaining until sleep.\r\n", 5 - mistake_count);
            uart_send_string(msg);
            memset(current_cmd, 0, COMMAND_LENGTH);
            if (mistake_count >= 5) {
              sleep_mode_mistake();
            }
          }
        }
        break;

      default:
        break;
    }
  }
}/*
 * Función: process_all_commands
 * ------------------------------
 * Procesa los comandos provenientes de tres interfaces:
 *  - UART2
 *  - UART3
 *  - Keypad
 *
 * Se llama a process_commands_buffer para cada uno de los buffers correspondientes.
 *
 * Parámetros:
 *  - rx_buffer_uart2: Buffer circular de datos recibidos por UART2.
 *  - cmd_uart2: Buffer de comando de 3 caracteres para UART2.
 *  - rx_buffer_uart3: Buffer circular de datos recibidos por UART3.
 *  - cmd_uart3: Buffer de comando de 3 caracteres para UART3.
 *  - rx_buffer_keypad: Buffer circular de datos recibidos por el keypad.
 *  - cmd_keypad: Buffer de comando de 3 caracteres para el keypad.
 */

void process_all_commands(ring_buffer_t *rx_buffer_uart2, char *cmd_uart2,
                          ring_buffer_t *rx_buffer_uart3, char *cmd_uart3,
                          ring_buffer_t *rx_buffer_keypad, char *cmd_keypad) {
  process_commands_buffer(rx_buffer_uart2, cmd_uart2);
  process_commands_buffer(rx_buffer_uart3, cmd_uart3);
  process_commands_buffer(rx_buffer_keypad, cmd_keypad);
}
/*
 * Función: sleep_mode_inactivity
 * -------------------------------
 * Activa el modo de suspensión debido a inactividad del sistema.
 * Envía un mensaje UART, suspende el tick del sistema, entra en modo sleep
 * y al despertar reanuda el tick y envía un mensaje de aviso.
 * Finalmente, actualiza la marca de tiempo de la última actividad.
 */

void sleep_mode_inactivity(void) {
  uart_send_string("\r\nNo activity for 30 sec. Entering sleep mode.\r\n");
  HAL_SuspendTick();
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  HAL_ResumeTick();
  uart_send_string("\r\nAwake from inactivity sleep.\r\n");
  last_activity_tick = HAL_GetTick();
}
/*
 * Función: sleep_mode_mistake
 * ----------------------------
 * Activa el modo de suspensión debido a demasiados comandos inválidos.
 * Envía un mensaje UART, suspende el tick, entra en modo sleep y espera al menos 10 segundos.
 * Luego, reanuda el tick, envía un mensaje de despertar y reinicia el contador de errores.
 * Finalmente, actualiza la marca de tiempo de la última actividad.
 */
void sleep_mode_mistake(void) {
  uart_send_string("\r\nToo many invalid commands. Sleeping for 10 sec.\r\n");
  HAL_SuspendTick();
  uint32_t sleepStart = HAL_GetTick();
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  while(HAL_GetTick() - sleepStart < 10000) { }
  HAL_ResumeTick();
  uart_send_string("\r\nAwake from mistake sleep.\r\n");
  mistake_count = 0;
  last_activity_tick = HAL_GetTick();
}


