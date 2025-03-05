Control Access System
Autores:
Juan Jeronimo Castaño y Daniel Mauricio Mejia Hoyos

Este firmware se ha desarrollado para gestionar el control de acceso de una puerta mediante un microcontrolador de STMicroelectronics, integrando múltiples interfaces de entrada y salida. La aplicación se basa en una arquitectura modular que combina comunicación UART, manejo de teclado matricial, visualización en pantalla OLED y control de indicadores LED, junto con funciones de ahorro de energía y gestión de errores.

1. Arquitectura General
El código se estructura en diferentes módulos, cada uno encargado de gestionar una funcionalidad específica:

Comunicación UART
Se utilizan dos interfaces UART (UART2 y UART3) para la comunicación bidireccional.
Cada UART opera en modo de interrupción, lo que permite recibir datos de forma asíncrona.
Los datos recibidos se almacenan en ring buffers independientes, facilitando la segregación y el posterior procesamiento de comandos.
Teclado Matricial
El teclado se conecta al microcontrolador mediante pines configurados con interrupciones.
Los eventos del teclado se almacenan en un ring buffer específico, integrando así la entrada manual con los comandos provenientes de las UART.
Pantalla OLED y LEDs
La pantalla OLED se utiliza para mostrar imágenes bitmap que representan el estado de la puerta (por ejemplo, “bloqueada” o “desbloqueada”) y mensajes de notificación.
Se utilizan varios LEDs:
LD4: Indica el estado de la puerta (encendido para puerta abierta y apagado para cerrada).
LD2 (Heartbeat): Parpadea periódicamente para mostrar que el sistema está activo.
LD5 (Ring): Se activa en respuesta a un botón específico (B2) para indicar eventos de "ring" o llamada.
2. Procesamiento de Comandos y Máquina de Estados
El núcleo del sistema se basa en una máquina de estados simple para el procesamiento de comandos, implementada con dos estados definidos:

STATE_WAIT_FOR_START:
El sistema espera el comando de inicio #*#, que actúa como "llave" para habilitar el reconocimiento del siguiente comando.

STATE_WAIT_FOR_COMMAND:
Una vez recibido el comando de inicio, se procesan comandos específicos para controlar el estado de la puerta:

#0#: Abre la puerta temporalmente durante 5 segundos.
#C#: Cierra la puerta.
#1#: Solicita el estado actual de la puerta.
#8#: Reinicia el sistema, restableciendo la señalización de la puerta y las indicaciones visuales.
Cada interfaz (UART2, UART3 y el teclado) mantiene su propio buffer y un buffer temporal de 3 bytes (el tamaño del comando) para acumular los caracteres entrantes. A medida que se reciben los bytes, se actualiza el buffer y se evalúa el estado actual mediante la función process_commands_buffer(), lo que permite que la aplicación reaccione de forma inmediata y consistente a los comandos provenientes de distintas fuentes.

3. Gestión de Eventos y Sincronización
El firmware implementa varios mecanismos para garantizar la fiabilidad y coherencia del sistema:

Interrupciones y Debounce:

Se configuran interrupciones para los botones y las columnas del teclado.
Cada interrupción actualiza una marca de tiempo (HAL_GetTick()) e implementa lógica de debounce (retardo de 200 ms) para evitar lecturas erróneas debido a rebotes mecánicos.
Sincronización de Actividad:

Se registra el último tick de actividad, ya sea por recepción UART o por interacción con el teclado/pulsadores.
Esto permite detectar inactividad prolongada y activar modos de ahorro de energía.
Contador de Errores:

Se utiliza un contador de errores ("mistakes") que se incrementa ante la recepción de comandos no reconocidos.
Al superar cinco intentos, el sistema entra en un modo de suspensión especial (mistake sleep) durante 10 segundos, reiniciando el contador una vez finalizado este periodo.
4. Funciones de Ahorro de Energía
El sistema incorpora dos modos de suspensión para optimizar el consumo energético:

Modo de Inactividad:

Si no se detecta actividad (ningún comando o pulsación) durante 30 segundos, se invoca sleep_mode_inactivity().
Esta función suspende el tick del sistema y entra en modo de sueño (SLEEPMode) hasta que una interrupción reactiva el sistema, momento en el que se reanuda el tick.
Modo por Comandos Inválidos:

Tras cinco intentos consecutivos de introducir comandos inválidos, se activa sleep_mode_mistake().
En este caso, el sistema se duerme durante al menos 10 segundos, actuando como medida de seguridad ante errores o posibles intentos de acceso no autorizados.
5. Control de la Puerta
El firmware maneja dos modos de operación para la puerta:

Apertura Temporal:

Al recibir el comando #0# o mediante la pulsación de un botón (cuando la puerta no está en estado permanente), se abre la puerta temporalmente y se inicia un contador.
Pasados 5 segundos, el sistema cierra automáticamente la puerta, a menos que se haya configurado una apertura permanente.
Apertura Permanente:

Mediante un doble pulsado (o comandos específicos), se activa la apertura permanente, manteniendo la puerta abierta hasta que se envíe el comando de cierre (#C#) o se realice una acción de reinicio.
6. Interacción con la Pantalla OLED y LEDs
La pantalla OLED proporciona retroalimentación visual en tiempo real:

Durante el estado normal se muestran imágenes que indican el estado de la puerta:
"Locked" (cerrado): Cuando el LED LD4 está apagado.
"Unlocked" (abierto): Cuando el LED LD4 está encendido.
Al presionar el botón B2 (configurado con interrupciones), se muestra una imagen especial del “ring” y se enciende el LED LD5, generando un mensaje en la salida UART que indica la detección de la pulsación.
El LED LD2 (heartbeat) parpadea a intervalos regulares (cada 500 ms) para indicar que el sistema está activo.
7. Comunicación y Retroalimentación UART
La comunicación UART se gestiona de forma bidireccional:

Los datos recibidos por UART2 se envían inmediatamente a UART3 y viceversa, asegurando redundancia y sincronización entre ambas interfaces.
Se envían mensajes informativos al usuario cada vez que se ejecuta un comando, se detecta un error o se produce un cambio de estado (por ejemplo, apertura o cierre de la puerta, entrada y salida de modos de suspensión).
8. Inicialización y Configuración del Sistema
Al inicio del programa (en main()), se ejecutan una serie de inicializaciones fundamentales:

Inicialización del Hardware:

Se configuran el reloj del sistema, los pines GPIO y las interfaces de comunicación (UART e I2C). La I2C se utiliza para la comunicación con la pantalla OLED.
Inicialización de Periféricos:

Se inician los módulos de la pantalla OLED, el teclado y los ring buffers, dejando el sistema preparado para recibir comandos y procesar entradas de forma simultánea.
Pantalla de Bienvenida:

Al iniciar, se muestra la versión del firmware tanto en la salida UART como en la pantalla OLED, informando al usuario sobre la versión del sistema en ejecución.
Diagramas
Diagrama 1: Arquitectura General del Código
                 ┌───────────────────────────────────────┐
                 │   main.c (inicialización, bucle      │
                 │   principal, etc.)                   │
                 └─────────────────┬──────────────────────┘
                                   │
                                   │ (Interrupciones)
                                   │  HAL_GPIO_EXTI_Callback
                                   │  HAL_UART_RxCpltCallback
                                   ▼
   ┌──────────────────────────────────────────────────────────────┐
   │                  button_handler.c                            │
   │   (Gestión de EXTI: B1/B2, debouncing, pulsación simple/doble, │
   │    control del anillo OLED, etc.)                              │
   └───────────────┬──────────────────────────────────────────────┘
                   │ (Eventos de botón)
                   ▼
 ┌─────────────────────────────────────────────────────────────────┐
 │         ring_buffer.c (UART2, UART3, Keypad)                     │
 │  (Buffers circulares donde se almacenan los bytes recibidos por    │
 │   interrupción. Se leen en el bucle principal.)                   │
 └───────────────┬────────────────────────────────────────────────┘
                 │ (Lectura y escritura de datos)
                 ▼
   ┌──────────────────────────────────────────────────────────────┐
   │                command_handler.c                             │
   │ (Máquina de estados para comandos: #*#, #0#, #C#, #1#, #8#,    │
   │  etc. Lógica de STATE_WAIT_FOR_START / STATE_WAIT_FOR_COMMAND,  │
   │  control de errores y sleep, envío de strings.)                │
   └───────────────┬──────────────────────────────────────────────┘
                   │ (Comandos reconocidos)
                   ▼
   ┌──────────────────────────────────────────────────────────────┐
   │                  door_controller.c                           │
   │ (Funciones para abrir/cerrar la puerta: LD4, temporizada,      │
   │  permanente, auto-cierre tras 5 s, etc.)                       │
   └──────────────────────────────────────────────────────────────┘
Notas:

main.c inicializa los periféricos (GPIO, UART, I2C) mediante el código autogenerado por CubeMX y ejecuta el bucle principal.
Las interrupciones (pulsaciones de botón y recepción de bytes UART) llaman a funciones en button_handler.c y/o escriben bytes en los ring buffers de ring_buffer.c.
En el bucle principal de main.c, se revisan los buffers y se llama a la máquina de estados de command_handler.c, que interpreta comandos de 3 caracteres (por ejemplo, #0#, #C#).
Según el comando, se invoca la función correspondiente en door_controller.c para abrir o cerrar la puerta (control de LD4).
button_handler.c se encarga de la lógica de pulsaciones (simple/doble) para B1 y de la actualización de la pantalla OLED al pulsar B2 (timbre).
Diagrama 2: Flujo Detallado de Comandos
┌─────────────────────────────────────────────────────────┐
│   Interrupción UART2/UART3 o Keypad (HAL_UART_RxCplt,     │
│   EXTI para keypad). Se recibe un byte.                  │
└─────────────────────────────────────────────────────────┘
               │
               │ (Se almacena el byte en el buffer circular)
               ▼
┌───────────────────────────────────────────────────────────────┐
│                        ring_buffer.c                          │
│   - Se gestionan tres buffers: UART2, UART3 y Keypad.         │
│   - El byte se encola en la rutina de interrupción.           │
└───────────────────────────────────────────────────────────────┘
               │
               │ (main.c lee estos buffers en el bucle principal)
               ▼
┌─────────────────────────────────────────────────────────────────┐
│                     command_handler.c                           │
│  - Máquina de estados:                                          │
│      1) STATE_WAIT_FOR_START (espera "#*#")                     │
│      2) STATE_WAIT_FOR_COMMAND (interpreta "#0#", "#C#", etc.)   │
│  - Si se detecta un comando válido, se llama a                   │
│    `door_controller.c` para abrir o cerrar la puerta.           │
│  - Si es inválido, se incrementa el contador de errores y,       │
│    si se excede el umbral, se activa `sleep_mode_mistake()`.      │
└─────────────────────────────────────────────────────────────────┘
               │ (Comando válido: #0#, #C#, #1#, #8#, ...)
               ▼
┌───────────────────────────────────────────────────────────────┐
│                      door_controller.c                         │
│  - `open_door_temp()`: activa **LD4** y arranca un temporizador │
│    de 5 s.                                                      │
│  - `open_door_perm()`: activa **LD4** sin temporizador.         │
│  - `close_door()`: apaga **LD4**.                                 │
│  - `door_controller_auto_close_check()`: verifica si han         │
│    transcurrido 5 s y cierra la puerta si se trata de una          │
│    apertura temporal.                                             │
└───────────────────────────────────────────────────────────────┘
Explicación del flujo:

Cuando llega un byte por UART2, UART3 o el teclado (keypad), la rutina de interrupción lo escribe en el ring buffer correspondiente de ring_buffer.c.
En el bucle principal de main.c, se llama periódicamente a command_handler.c para leer los bytes de cada buffer y trasladarlos a un arreglo de 3 caracteres (current_cmd).
La máquina de estados en command_handler.c primero busca la secuencia de inicio (#*#). Una vez detectada, pasa a esperar un comando (por ejemplo, #0# para apertura temporal, #C# para cierre, etc.).
Al reconocer un comando válido, se invoca la función correspondiente en door_controller.c (por ejemplo, open_door_temp()). Si el comando es inválido, se incrementa el contador de errores; tras 5 errores consecutivos, se activa el modo de suspensión (sleep_mode_mistake()).
En door_controller.c, se enciende o apaga la salida digital (LD4) y, en caso de apertura temporal, se guarda la marca de tiempo para que, transcurridos 5 segundos, la puerta se cierre automáticamente.
