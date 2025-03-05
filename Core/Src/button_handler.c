#include "button_handler.h"
#include "command_handler.h"
#include "door_controller.h"
#include "locked.h"
#include "ring.h"
#include "unlocked.h"
#include "keypad.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

/* 
 * Variables estáticas para el control de rebote (debouncing) de los botones
 * y para llevar la cuenta de pulsaciones.
 */
static uint32_t button_debounce_tick = 0;       // Marca de tiempo para evitar rebotes en B1
static uint32_t debounce_tick = 0;              // Marca de tiempo para evitar rebotes en keypad
static uint32_t key_pressed_tick = 0;           // Marca de tiempo cuando se detecta columna pulsada
static uint16_t column_pressed = 0;             // Identifica qué pin de columna (del keypad) se pulsó
static uint32_t last_button_press_time = 0;     // Último instante en que se pulsó el botón B1
static uint8_t button_press_count = 0;          // Cuenta el número de pulsaciones consecutivas en B1

/*
 * Función llamada desde HAL_GPIO_EXTI_Callback cuando ocurre una interrupción EXTI.
 * Se utiliza para distinguir entre la pulsación de B1, B2 y las columnas del keypad.
 */
void button_handler_exti_callback(uint16_t GPIO_Pin) {
  // Obtenemos la marca de tiempo actual (en milisegundos)
  uint32_t current_tick = HAL_GetTick();

  // Referencia a la variable last_activity_tick, definida en main.c
  // Esto se hace para indicar que ha habido actividad y evitar que el sistema
  // entre en modo de inactividad (sleep) si ha pasado mucho tiempo sin eventos.
  extern uint32_t last_activity_tick;
  last_activity_tick = current_tick;
  
  // Verificamos si la interrupción proviene del botón B1
  if (GPIO_Pin == B1_Pin) {
    // Chequeo de rebote: si no han pasado al menos 200 ms desde la última pulsación,
    // ignoramos este evento para no contar pulsaciones falsas.
    if ((current_tick - button_debounce_tick) < 200) {
      return;
    }
    // Actualizamos la marca de tiempo de rebote
    button_debounce_tick = current_tick;

    // Aumentamos el contador de pulsaciones (para distinguir pulsación simple o doble)
    button_press_count++;

    // Guardamos el instante de esta pulsación para medir los 500 ms en la función button_handler_process
    last_button_press_time = current_tick;
  }
  // Si la interrupción es del botón B2, no hacemos nada aquí;
  // la lógica de B2 se maneja en button_handler_process_ring().
  else if (GPIO_Pin == B2_Pin) {
    // Se deja vacío intencionalmente.
  }
  // Si no es B1 ni B2, asumimos que es la interrupción de columna del keypad.
  else {
    // Chequeo de rebote para keypad: si no han pasado 200 ms desde la última vez,
    // se ignora para evitar lecturas duplicadas.
    if ((debounce_tick + 200) > current_tick) {
      return;
    }
    // Actualizamos la marca de tiempo para el keypad
    debounce_tick = current_tick;

    // Guardamos el tiempo y la columna que se detectó como pulsada
    key_pressed_tick = current_tick;
    column_pressed = GPIO_Pin;
  }
}

/*
 * Devuelve el valor de la columna pulsada (si existe) y luego la limpia
 * para que no se lea más de una vez la misma pulsación.
 */
uint16_t button_handler_get_column(void) {
  uint16_t col = column_pressed;
  // Ponemos column_pressed en 0 para indicar que ya se leyó
  column_pressed = 0;
  return col;
}

/*
 * Procesa las pulsaciones del botón B1 para abrir/cerrar la puerta.
 * - Una sola pulsación (single press) dentro de ~500 ms => 
 *     abre la puerta temporalmente si está cerrada, 
 *     o cierra la puerta si estaba permanentemente abierta.
 * - Dos (o más) pulsaciones consecutivas en menos de 500 ms => 
 *     abre la puerta de forma permanente.
 */
void button_handler_process(void) {
  // Obtenemos la marca de tiempo actual
  uint32_t current_tick = HAL_GetTick();

  // Verificamos si hubo al menos una pulsación y si ya pasaron 500 ms desde la última,
  // lo que indica que no esperamos más pulsaciones consecutivas (para distinguir simple/doble).
  if (button_press_count > 0 && (current_tick - last_button_press_time) >= 500) {
    // Caso de 1 pulsación
    if (button_press_count == 1) {
      // Si la puerta está abierta y en modo permanente, la cerramos
      if (door_is_open() && door_is_permanent()) {
        close_door();
        uart_send_string("\r\nButton: Door closed.\r\n");
      } else {
        // Si no está permanentemente abierta, abrimos temporalmente (5 s)
        open_door_temp();
        uart_send_string("\r\nButton: Door opened temporarily (5 sec).\r\n");
      }
    }
    // Caso de 2 (o más) pulsaciones => abrir puerta permanentemente
    else if (button_press_count >= 2) {
      open_door_perm();
      uart_send_string("\r\nButton: Door opened permanently.\r\n");
    }

    // Reseteamos contador y el tiempo de la última pulsación,
    // para que en la próxima vez se empiece a contar desde cero.
    button_press_count = 0;
    last_button_press_time = 0;
  }
}

/*
 * Procesa el botón B2 (Timbre) y actualiza la pantalla OLED en función
 * de si se encuentra pulsado o no. 
 * - Si está pulsado (activo en bajo), se enciende LD5 y se muestra el bitmap "ring".
 * - Al soltarlo, se apaga LD5 y se muestra "locked" o "unlocked" dependiendo del estado de la puerta.
 */
static uint8_t prev_b2_state = 0; // Variable estática para recordar el estado anterior de B2
void button_handler_process_ring(void) {
  // Leemos el estado actual de B2 (GPIO_PIN_RESET => pulsado)
  uint8_t current_b2_state = HAL_GPIO_ReadPin(B2_GPIO_Port, B2_Pin);

  // Si B2 está pulsado (active low)
  if (current_b2_state == GPIO_PIN_RESET) {
    // Si antes no estaba pulsado, es un nuevo evento de pulsación
    if (!prev_b2_state) {
      uart_send_string("\r\nRing pressed.\r\n");
    }
    // Encendemos el LED LD5
    HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_SET);

    // Limpiamos la pantalla y dibujamos el bitmap "ring"
    ssd1306_Fill(Black);
    ssd1306_DrawBitmap(0, 0, ring, 128, 64, White);
    ssd1306_UpdateScreen();

    // Actualizamos el estado anterior de B2
    prev_b2_state = 1;
  } else {
    // Si B2 no está pulsado, apagamos LD5
    HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_RESET);

    // Limpiamos la pantalla OLED
    ssd1306_Fill(Black);

    // Dependiendo del estado de la puerta, dibujamos "locked" o "unlocked"
    if (door_is_open())
      ssd1306_DrawBitmap(0, 0, unlocked, 128, 64, White);
    else
      ssd1306_DrawBitmap(0, 0, locked, 128, 64, White);

    ssd1306_UpdateScreen();
    // Marcamos que B2 ya no está pulsado
    prev_b2_state = 0;
  }
}
