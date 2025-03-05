# Control Access System

### By: Juan Jeronimo Castaño y Daniel Mauricio Mejia Hoyos

Este firmware está desarrollado para gestionar el control de acceso de una puerta mediante un microcontrolador de STMicroelectronics, integrando diversas interfaces de entrada y salida. La aplicación sigue una arquitectura modular que combina:

- **Comunicación UART**
- **Manejo de teclado matricial**
- **Visualización en pantalla OLED**
- **Control de indicadores LED**
- **Funciones de ahorro de energía**
- **Gestón de errores**

---

## 1. Arquitectura General
El código está estructurado en módulos independientes, cada uno con una función específica:

### **Comunicación UART**
- Se utilizan dos interfaces UART (**UART2** y **UART3**) en modo interrupción para recepción asíncrona de datos.
- Los datos recibidos se almacenan en **ring buffers** independientes para su procesamiento.

### **Teclado Matricial**
- Se conecta mediante pines configurados con interrupciones.
- Los eventos del teclado se almacenan en un buffer específico, permitiendo integrar entrada manual y comandos UART.

### **Pantalla OLED y LEDs**
- La pantalla OLED muestra el estado de la puerta (**bloqueado/desbloqueado**) y mensajes de notificación.
- LEDs indicativos:
  - **LD4:** Estado de la puerta (encendido: abierta, apagado: cerrada).
  - **LD2 (Heartbeat):** Parpadeo periódico indicando actividad del sistema.
  - **LD5 (Ring):** Se activa con el botón **B2** para indicar un evento de "ring" o llamada.

---

## 2. Procesamiento de Comandos y Máquina de Estados
El sistema funciona con una **máquina de estados** simple para interpretar comandos:

- **STATE_WAIT_FOR_START**: Espera el comando de inicio (`#*#`).
- **STATE_WAIT_FOR_COMMAND**: Tras recibir `#*#`, procesa comandos:
  - `#0#`: Abre la puerta por 5 segundos.
  - `#C#`: Cierra la puerta.
  - `#1#`: Consulta el estado de la puerta.
  - `#8#`: Reinicia el sistema.

Cada interfaz (UART2, UART3 y teclado) gestiona su propio **buffer** de 3 bytes para procesar comandos en tiempo real.

---

## 3. Gestión de Eventos y Sincronización
El firmware implementa mecanismos para asegurar confiabilidad y coherencia:

- **Interrupciones y Debounce:**
  - Se configuran interrupciones para botones y teclado.
  - Lógica de debounce con retardo de **200 ms** para evitar lecturas erróneas.

- **Sincronización de Actividad:**
  - Se registra el último tick de actividad para detectar inactividad prolongada.

- **Contador de Errores:**
  - Tras 5 intentos fallidos, el sistema entra en "**mistake sleep**" por **10 segundos**.

---

## 4. Funciones de Ahorro de Energía
- **Modo de Inactividad:** Si no hay actividad en **30 segundos**, entra en modo **sleep** hasta recibir una interrupción.
- **Modo por Comandos Inválidos:** Tras 5 intentos fallidos, entra en **sleep** por **10 segundos**.

---

## 5. Control de la Puerta
- **Apertura Temporal:**
  - Se activa con `#0#` o un botón.
  - Se cierra automáticamente tras **5 segundos**.

- **Apertura Permanente:**
  - Se activa con doble pulsación o comando especial.
  - Se mantiene abierta hasta recibir `#C#` o un reinicio.

---

## 6. Interacción con Pantalla OLED y LEDs
- **OLED:**
  - Muestra "**Locked**" (cerrado) y "**Unlocked**" (abierto).
  - Muestra "**Ring**" al presionar **B2**.
- **LEDs:**
  - **LD2 (Heartbeat):** Parpadea cada **500 ms**.

---

## 7. Comunicación y Retroalimentación UART
- UART2 y UART3 se sincronizan bidireccionalmente.
- Se envían mensajes informativos al usuario en cada evento relevante.

---

## 8. Inicialización y Configuración del Sistema
- **main.c:**
  - Inicializa **GPIO, UART, I2C** (para OLED).
  - Muestra versión de firmware al iniciar.

---

## 9. Diagramas de Arquitectura

### **Diagrama 1: Estructura del Código**
```plaintext
┌───────────────────────────────────────────┐
│         main.c (inicialización)           │
└───────────────┬───────────────────────────┘
                │  (Interrupciones)
                ▼
┌───────────────────────────────────────────┐
│   button_handler.c (Eventos de botón)     │
└───────────────┬───────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────┐
│   ring_buffer.c (Manejo de UART y Keypad) │
└───────────────┬───────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────┐
│   command_handler.c (Manejo de comandos)  │
└───────────────┬───────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────┐
│   door_controller.c (Control de puerta)   │
└───────────────────────────────────────────┘
```

### **Diagrama 2: Flujo de Comandos**
```plaintext
┌───────────────────────────────────────────┐
│ Interrupción UART2/UART3 o Keypad         │
│ (HAL_UART_RxCplt, EXTI para keypad)       │
└───────────────────────────────────────────┘
        ▼
┌───────────────────────────────────────────┐
│ Se recibe un byte y se almacena en buffer │
└───────────────────────────────────────────┘
        ▼
┌───────────────────────────────────────────┐
│ Se evalúa el comando en process_commands  │
└───────────────────────────────────────────┘
        ▼
┌───────────────────────────────────────────┐
│ Se ejecuta la acción correspondiente      │
└───────────────────────────────────────────┘
```

---


