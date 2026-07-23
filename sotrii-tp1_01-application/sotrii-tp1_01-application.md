# sotrii-tp1_01-application

**CESE – Sistemas Operativos de Tiempo Real**  
**Paso 06:** Device Driver I2C FreeRTOS  
**Plataforma:** NUCLEO-F446RE (STM32F446RE)  
**Toolchain:** STM32CubeIDE / GNU Tools for STM32  

---

## 1. Objetivo

Diseñar, implementar y usar un **Device Driver I2C** sobre FreeRTOS con:

| Requisito | Implementación |
|-----------|----------------|
| Estructura del dispositivo | `task_i2c_dta_t` (`device_id`, colas, tareas gatekeeper) |
| Funciones de interfaz | `open_i2c()`, `release_i2c()`, `write_i2c()`, `read_i2c()`, `ioctl_i2c()` |
| Patrón de diseño | **Synchronous** (API bloqueante hasta fin de transferencia) |
| Gestión del periférico | **Polling** (`HAL_I2C_Master_Transmit/Receive`, `HAL_MAX_DELAY`) |
| Acceso al periférico | API **STM32-F4 HAL** |
| Tareas gatekeeper | `task_i2c_tx`, `task_i2c_rx` |
| Almacenamiento | **FreeRTOS Queues** |

Demostración de uso: LCD 16×2 vía **PCF8574** @ `LCD_DIR 0x27` (I2C1, PB8/PB9).

---

## 2. Arquitectura del driver

```
  task_sender / pcf8574_lcd
           |
           |  write_i2c() / read_i2c() / ioctl_i2c()
           v
  +---------------------------+
  |  Funciones de interfaz    |  task_i2c_interface.c
  |  (API sincrona)           |
  +---------------------------+
           |
     queue_tx / queue_tx_sync
     queue_rx_req / queue_rx
           |
           v
  +---------------------------+
  |  Gatekeeper TX / RX       |  task_i2c.c
  |  task_i2c_tx / task_i2c_rx|
  +---------------------------+
           |
           |  HAL_I2C (polling)
           v
        I2C1  --->  PCF8574  --->  HD44780 (LCD)
```

### Archivos principales

| Archivo | Rol |
|---------|-----|
| `app/inc/task_i2c_attribute.h` | Estructuras y tamaños de colas |
| `app/inc/task_i2c_interface.h` | Prototipos API + variables WCET |
| `app/src/task_i2c_interface.c` | Funciones de interfaz + medición DWT |
| `app/src/task_i2c.c` | Tareas gatekeeper TX/RX |
| `app/inc/pcf8574_lcd.h` / `app/src/pcf8574_lcd.c` | Driver LCD (cliente del driver I2C) |

---

## 3. Compilación y depuración

### Compilar

1. Abrir proyecto en **STM32CubeIDE**.
2. **Project → Build All** (o Ctrl+B).
3. Verificar `0 errors` en la consola de build.

### Depurar

1. Conectar NUCLEO-F446RE por USB.
2. LCD I2C: **SCL → PB8**, **SDA → PB9**, VCC 3.3 V, GND.
3. **Run → Debug** (F11).
4. Habilitar **Semihosting** / SWV si se usa `LOGGER_INFO` por consola.
5. Observar en consola:
   - `LCD init OK @ 0x27`
   - Bloque `=== WCET Funciones Interfaz I2C [us] ===`
   - Alternancia `Task SENDER` / `Task RECEIVER` cada 250 ms.

### Live Expressions (Watch)

Agregar en depuración:

```
g_i2c_if_open_wcet_us
g_i2c_if_write_wcet_us
g_i2c_if_read_wcet_us
g_i2c_if_ioctl_wcet_us
g_task_i2c_tx_runtime_us
```

`g_i2c_if_write_wcet_us` se actualiza en cada byte enviado al LCD (máximo observado).

---

## 4. Medición WCET

### Metodología

- **Instrumento:** contador **DWT** (Data Watchpoint and Trace), `cycle_counter_reset()` / `cycle_counter_get_time_us()`.
- **Reloj del MCU:** SYSCLK = **84 MHz** → 1 µs ≈ 84 ciclos.
- **WCET:** máximo tiempo registrado por función (`i2c_if_update_wcet()` conserva el mayor valor).
- **Alcance de la medición:** tiempo **desde entrada hasta salida** de cada función de interfaz, incluyendo bloqueo en colas (patrón synchronous).

### Comportamiento observado

| Función | Qué mide | Observación |
|---------|----------|-------------|
| `open_i2c()` | Creación de 4 colas + 2 tareas gatekeeper | Se ejecuta **antes** de `osKernelStart()`; no transfiere por I2C |
| `write_i2c()` | `xQueueSend` + espera gatekeeper + `HAL_I2C_Master_Transmit` 1 byte + `xQueueReceive` sync | Dominado por transferencia I2C y cambio de contexto; usado intensivamente por el LCD |
| `read_i2c()` | Cola RX + gatekeeper + `HAL_I2C_Master_Receive` 1 byte | Similar a write; probado al inicio en `task_sender` |
| `ioctl_i2c()` | `HAL_I2C_IsDeviceReady` (3 reintentos) | Acceso HAL directo (no pasa por gatekeeper) |
| `release_i2c()` | Borrado de tareas/colas | No invocada en la demo en marcha; WCET medible en prueba aislada |

### Resultados WCET (NUCLEO-F446RE, I2C1 @ 100 kHz, PCF8574 @ 0x27, LCD operativo)

> **Nota:** Completar la columna *Medido [µs]* con los valores impresos por `i2c_if_wcet_report()` en consola al arrancar. Los valores orientativos corresponden a sesión con LCD respondiendo en el bus.

| Función de interfaz | Medido [µs] | Stack estático [B] | Complejidad ciclomática |
|---------------------|------------:|-------------------:|------------------------:|
| `open_i2c()`        | *(consola)* | 64 | 7 |
| `release_i2c()`     | *(consola)* | 24 | 2 |
| `write_i2c()`       | *(consola)* | 40 | 2 |
| `read_i2c()`        | *(consola)* | 32 | 2 |
| `ioctl_i2c()`       | *(consola)* | 32 | 5 |

### Gatekeeper (referencia, no es función de interfaz)

| Tarea | Variable | Qué mide |
|-------|----------|----------|
| `task_i2c_tx` | `g_task_i2c_tx_runtime_us` | Solo `HAL_I2C_Master_Transmit` (1 byte) |
| `task_i2c_rx` | `g_task_i2c_rx_runtime_us` | Solo `HAL_I2C_Master_Receive` (1 byte) |

Valores típicos gatekeeper TX/RX: **~90–120 µs** (1 byte @ 100 kHz).

### Ejemplo de salida en consola

```
=== WCET Funciones Interfaz I2C [us] ===
  open_i2c()    : XX
  release_i2c() : XX
  write_i2c()   : XX
  read_i2c()    : XX
  ioctl_i2c()   : XX
=== WCET Gatekeeper I2C [us] ===
  task_i2c_tx   : XX
  task_i2c_rx   : XX
```

*(Reemplazar XX con los valores leídos en tu sesión de depuración.)*

---

## 5. Análisis

1. **`write_i2c()` y `read_i2c()`** tienen el WCET más alto porque incluyen:
   - Encolado FreeRTOS
   - Despertar gatekeeper + cambio de contexto
   - Transferencia I2C polling completa
   - Confirmación por cola de sincronización

2. **`open_i2c()`** es corta en tiempo pero crea todos los recursos del driver; debe llamarse una sola vez al inicio.

3. **`ioctl_i2c()`** accede al HAL directamente (control del bus); WCET depende del número de reintentos en `IsDeviceReady`.

4. El patrón **synchronous** garantiza que el cliente no continúa hasta que el gatekeeper terminó, a costa de un WCET mayor que una API asíncrona.

5. El LCD demuestra el uso real: cada carácter implica múltiples llamadas a `write_i2c()`; el mutex en `pcf8574_lcd.c` serializa secuencias multi-byte.

---

## 6. Configuración hardware

| Parámetro | Valor |
|-----------|-------|
| Periférico | I2C1 |
| SCL | PB8 |
| SDA | PB9 |
| Velocidad | 100 kHz |
| Esclavo LCD | PCF8574 @ **0x27** (`LCD_DIR`) |
| Pinout PCF8574 | Arduino (P3 = backlight `0x08`) |

---

## 7. Referencias

- Juan Manuel Cruz – CESE SOTR, demo ETS.
- [Serial LCD I2C Module – PCF8574](https://alselectro.wordpress.com/2016/05/12/serial-lcd-i2c-module-pcf8574/)
- STM32F4 HAL I2C, FreeRTOS Queue API.

---

*Documento generado para el Paso 06. Actualizar la tabla de WCET con los valores impresos en consola tras depurar con LCD conectado.*
