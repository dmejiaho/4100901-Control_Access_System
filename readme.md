# Control Access System

## Autores:
**Juan Jeronimo Castaño y Daniel Mauricio Mejia Hoyos**

Este firmware se ha desarrollado para gestionar el control de acceso de una puerta mediante un microcontrolador de STMicroelectronics, integrando múltiples interfaces de entrada y salida. La aplicación se basa en una arquitectura modular que combina comunicación UART, manejo de teclado matricial, visualización en pantalla OLED y control de indicadores LED, junto con funciones de ahorro de energía y gestión de errores.

---

## 1. Arquitectura General
El código se estructura en diferentes módulos, cada uno encargado de gestionar una funcionalidad específica:

### Comunicación UART
- Se utilizan dos interfaces UART (**UART2 y UART3**) para la comunicación bidireccional.
- Cada UART opera en modo de interrupción, permitiendo recibir datos de forma asíncrona.
- Los datos recibidos se almacenan en *ring buffers* independientes, facilitando la segregación y el posterior procesamiento de comandos.

### Teclado Matricial
- El teclado se conecta al microcontrolador mediante pines configurados con interrupciones.
- Los eventos del teclado se almacenan en un *ring buffer* específico, integrando así la entrada manual con los comandos provenientes de las UART.

### Pantalla OLED y LEDs
- La pantalla **OLED** muestra imágenes *bitmap* representando el estado de la puerta (*bloqueada* o *desbloqueada*) y mensajes de notificación.
- Se utilizan varios **LEDs**:
  - **LD4:** Indica el estado de la puerta (encendido para abierta, apagado para cerrada).
  - **LD2 (Heartbeat):** Parpadea periódicamente para indicar que el sistema está activo.
  - **LD5 (Ring):** Se activa en respuesta a un botón específico (**B2**) para indicar eventos de "ring" o llamada.

---

## 2. Procesamiento de Comandos y Máquina de Estados
El núcleo del sistema se basa en una máquina de estados simple para el procesamiento de comandos, implementada con dos estados definidos:

- **STATE_WAIT_FOR_START:** El sistema espera el comando de inicio `#*#`, que actúa como "llave" para habilitar el reconocimiento del siguiente comando.
- **STATE_WAIT_FOR_COMMAND:** Una vez recibido el comando de inicio, se procesan comandos específicos para controlar el estado de la puerta:
  - `#0#`: Abre la puerta temporalmente durante **5 segundos**.
  - `#C#`: Cierra la puerta.
  - `#1#`: Solicita el estado actual de la puerta.
  - `#8#`: Reinicia el sistema, restableciendo la señalización de la puerta y las indicaciones visuales.

Cada interfaz (**UART2, UART3 y el teclado**) mantiene su propio *buffer* y un *buffer* temporal de **3 bytes** (el tamaño del comando) para acumular los caracteres entrantes. La función `process_commands_buffer()` evalúa el estado actual, permitiendo que la aplicación reaccione de forma inmediata y consistente a los comandos provenientes de distintas fuentes.

---

## 3. Gestión de Eventos y Sincronización
El firmware implementa varios mecanismos para garantizar la fiabilidad y coherencia del sistema:

### Interrupciones y *Debounce*
- Se configuran **interrupciones** para los botones y las columnas del teclado.
- Cada interrupción actualiza una marca de tiempo (*HAL_GetTick()*) e implementa lógica de *debounce* (retardo de **200 ms**) para evitar lecturas erróneas debido a rebotes mecánicos.

### Sincronización de Actividad
- Se registra el último *tick* de actividad, ya sea por recepción UART o por interacción con el teclado/pulsadores.
- Esto permite detectar inactividad prolongada y activar modos de ahorro de energía.

### Contador de Errores
- Se utiliza un contador de errores (*mistakes*) que se incrementa ante la recepción de comandos no reconocidos.
- Al superar **cinco intentos**, el sistema entra en un modo de suspensión especial (*mistake sleep*) durante **10 segundos**.

---

## 4. Funciones de Ahorro de Energía
El sistema incorpora dos modos de suspensión:

- **Modo de Inactividad:**
  - Si no se detecta actividad en **30 segundos**, se invoca `sleep_mode_inactivity()`.
  - Esta función suspende el *tick* del sistema y entra en modo de sueño (*SLEEPMode*).

- **Modo por Comandos Inválidos:**
  - Tras cinco intentos consecutivos de introducir comandos inválidos, se activa `sleep_mode_mistake()`.
  - El sistema se duerme durante al menos **10 segundos**.

---

## 5. Control de la Puerta
El firmware maneja dos modos de operación:

- **Apertura Temporal:**
  - Al recibir el comando `#0#` o mediante la pulsación de un botón, se abre la puerta temporalmente.
  - Pasados **5 segundos**, el sistema la cierra automáticamente.

- **Apertura Permanente:**
  - Mediante un **doble pulsado** (o comandos específicos), se mantiene la puerta abierta hasta que se envíe el comando de cierre (`#C#`).

---

## 6. Interacción con la Pantalla OLED y LEDs
La pantalla **OLED** proporciona retroalimentación visual en tiempo real:

- **"Locked"** (cerrado): Cuando el **LED LD4** está apagado.
- **"Unlocked"** (abierto): Cuando el **LED LD4** está encendido.
- Al presionar el botón **B2**, se muestra una imagen especial del "ring" y se enciende el **LED LD5**.
- El **LED LD2 (heartbeat)** parpadea cada **500 ms** para indicar que el sistema está activo.

---

## 7. Comunicación y Retroalimentación UART
- Los datos recibidos por **UART2** se envían inmediatamente a **UART3** y viceversa.
- Se envían mensajes informativos al usuario cada vez que se ejecuta un comando.

---

## 8. Inicialización y Configuración del Sistema
### Inicialización del Hardware
- Configuración del **reloj del sistema**, **pines GPIO** y **interfaces de comunicación** (**UART, I2C**).
- La **I2C** se usa para la comunicación con la pantalla **OLED**.

### Pantalla de Bienvenida
- Al iniciar, se muestra la versión del firmware en la salida **UART**.

---


