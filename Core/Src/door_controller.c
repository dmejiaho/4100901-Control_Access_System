#include "door_controller.h"
#include "command_handler.h"  // Para la función uart_send_string
#include "main.h"

/*
 * Variable estática para indicar si la puerta está abierta de forma permanente.
 * 0 => apertura temporal o cerrada.
 * 1 => apertura permanente.
 */
static uint8_t door_permanent = 0;

/*
 * Variable estática que almacena el instante (en ms) en el que se abrió la puerta
 * de forma temporal. Se utiliza para cerrar la puerta automáticamente después de 5 s.
 */
static uint32_t temp_open_start = 0;

/*
 * Función: open_door_temp
 * ------------------------
 * Abre la puerta de forma temporal.
 * - Enciende la salida correspondiente a la puerta (LD4) mediante HAL_GPIO_WritePin.
 * - Registra el instante actual en temp_open_start para poder medir el tiempo de apertura.
 * - Marca la apertura como temporal (door_permanent = 0).
 */
void open_door_temp(void) {
  HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET); // Activa LD4 (puerta abierta)
  temp_open_start = HAL_GetTick();                         // Guarda el tiempo de apertura
  door_permanent = 0;                                       // Indica que no es permanente
}

/*
 * Función: open_door_perm
 * ------------------------
 * Abre la puerta de forma permanente.
 * - Enciende la salida correspondiente a la puerta (LD4).
 * - Marca la apertura como permanente (door_permanent = 1).
 * - Reinicia temp_open_start, ya que no se usará el temporizador de 5 s.
 */
void open_door_perm(void) {
  HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET); // Activa LD4 (puerta abierta)
  door_permanent = 1;                                      // Establece la apertura como permanente
  temp_open_start = 0;                                     // Reinicia el temporizador
}

/*
 * Función: close_door
 * -------------------
 * Cierra la puerta.
 * - Apaga la salida correspondiente a la puerta (LD4).
 * - Reinicia door_permanent y temp_open_start para indicar que la puerta está cerrada
 *   y no hay temporización activa.
 */
void close_door(void) {
  HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET); // Apaga LD4 (puerta cerrada)
  door_permanent = 0;                                        // Se marca como no permanente
  temp_open_start = 0;                                       // Se reinicia el temporizador
}

/*
 * Función: door_controller_auto_close_check
 * -------------------------------------------
 * Verifica si se debe cerrar la puerta automáticamente.
 * - Solo se ejecuta si la puerta no está abierta de forma permanente (door_permanent == 0)
 *   y si temp_open_start tiene un valor válido (la puerta fue abierta temporalmente).
 * - Comprueba si han transcurrido 5000 ms (5 segundos) desde que se abrió la puerta.
 * - Si se cumple la condición, cierra la puerta y envía un mensaje UART indicando el cierre.
 */
void door_controller_auto_close_check(void) {
  if (!door_permanent && temp_open_start != 0 &&
      (HAL_GetTick() - temp_open_start >= 5000)) {
    HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET); // Apaga LD4 (puerta cerrada)
    uart_send_string("\r\nDoor auto-closed after temporary open.\r\n");
    temp_open_start = 0; // Reinicia el temporizador para evitar múltiples cierres
  }
}

/*
 * Función: door_is_open
 * ----------------------
 * Retorna el estado de la puerta.
 * - Devuelve 1 (no cero) si LD4 está encendido (puerta abierta).
 * - Devuelve 0 si LD4 está apagado (puerta cerrada).
 */
uint8_t door_is_open(void) {
  return (HAL_GPIO_ReadPin(LD4_GPIO_Port, LD4_Pin) == GPIO_PIN_SET);
}

/*
 * Función: door_is_permanent
 * ---------------------------
 * Retorna si la puerta se encuentra en modo de apertura permanente.
 * - Devuelve 1 si la puerta se abrió permanentemente.
 * - Devuelve 0 en caso contrario.
 */
uint8_t door_is_permanent(void) {
  return door_permanent;
}
