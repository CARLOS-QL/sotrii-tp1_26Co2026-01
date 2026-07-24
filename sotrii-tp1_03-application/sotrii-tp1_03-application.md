# TP1 – Actividad 03 – Device Driver ADC de FreeRTOS

**CESE – Sistemas Operativos de Tiempo Real II**  
**Trabajo Práctico N° 1 – Device Driver**  
**Cohorte-Grupo:** 26Co2026-01  
**Responsable:** QUISPE LOPEZ, CARLOS (SIU e2614)  
**Plataforma:** NUCLEO-F446RE (STM32F446RE) @ **84 MHz**  
**Periférico:** **ADC1** — canal **0** (PA0 / conector **A0**, potenciómetro) + **DMA2 Stream0**  
**Toolchain:** STM32CubeIDE / GNU Tools for STM32  

---

## Paso 01: Generar el proyecto STM32

El proyecto **sotrii-tp1_03-application** fue generado con STM32CubeMX, importado en STM32CubeIDE y compila sin errores (**Project → Build All**).

---

## Paso 02: Crear el archivo de entrega

Se creó el archivo de entrega **`sotrii-tp1_03-application.md`** (este documento) en la raíz del proyecto.

---

## Paso 03: Análisis del código fuente base

| Archivo | Función principal |
|---------|-------------------|
| `app/src/app.c` | Inicialización (`app_init`), DWT, `open_adc(&hadc1)` y arranque del scheduler FreeRTOS |
| `app/src/app_it.c` | Callback `HAL_ADC_ConvCpltCallback` e instrumentación WCET de ISR |
| `app/src/task_receiver.c` | Tarea periódica; lectura del potenciómetro cada 250 ms; reporte WCET cada ~5 s |
| `app/src/task_adc.c` | Tarea gatekeeper **ADC** con `HAL_ADC_Start_DMA` |
| `app/src/task_adc_interface.c` | API del device driver: `open_adc`, `write_adc`, `read_adc`, `ioctl_adc`, `release_adc` |
| `app/inc/task_adc_attribute.h` | Estructura `task_adc_dta_t`, colas estáticas y enums |
| `app/inc/task_adc_interface.h` | Prototipos de la API y variables globales WCET |
| `app/inc/dwt.h` | Contador de ciclos DWT para medición en microsegundos |
| `Core/Src/main.c` | Init HAL: ADC1 (1 conversión, PA0), DMA, TIM2, USART2 |

El sistema es **Event-Triggered (ETS)**: las tareas se despiertan por temporizador (`vTaskDelay`), por comandos en colas spooler y por semáforos liberados desde ISR DMA.

---

## Paso 04: Depurar el nuevo proyecto STM32

Se confirmó mediante depuración en la NUCLEO-F446RE:

- Arranque correcto de `app_init`, `open_adc()` y tareas **Receiver** y **ADC** (gatekeeper)
- Logs por consola (semihosting) con el prefijo `[info]`
- Mensaje de inicialización del receptor con referencia al potenciómetro en PA0
- Lecturas ADC periódicas cada **250 ms** con valor raw (0–4095) y porcentaje aproximado
- Reporte WCET cada **~5 s** (20 ciclos de 250 ms)
- Verificación en **Live Expressions** con `task_adc_dta.dma_buffer[0]` reflejando el valor del potenciómetro en tiempo real

---

## Paso 05: *(Reservado / actividades intermedias del enunciado)*

---

## Paso 06: Device Driver ADC FreeRTOS — Implementación, prueba y WCET

### 6.1 Aplicación realizada

Se diseñó e implementó un **Device Driver ADC** sobre FreeRTOS que cumple los requisitos del enunciado:

| Requisito | Implementación |
|-----------|----------------|
| Estructura del dispositivo | `task_adc_dta_t` (`device_id`, `adc_channel`, colas spooler estáticas, semáforo DMA, buffer DMA, tarea gatekeeper) |
| Funciones de interfaz | `open_adc()`, `release_adc()`, `write_adc()`, `read_adc()`, `read_adc_wait()`, `ioctl_adc()` |
| Patrón de diseño | **Latest Input Only (LIO)** — output spooler profundidad 1 + `xQueueOverwrite` |
| Gestión del periférico | **DMA** — `HAL_ADC_Start_DMA` (DMA2 Stream0, periférico → memoria) |
| Acceso al hardware | API **STM32-F4 HAL** (`hadc1` / ADC1, canal 0 / PA0) |
| Tarea gatekeeper | `task_adc` (creada en `open_adc()`, recibe `task_adc_dta_t` con canal ADC) |
| Almacenamiento | **Colas Input/Output Spooler** + **memoria estática** (`xQueueCreateStatic`, `xSemaphoreCreateBinaryStatic`) |
| Medición WCET | Contador **DWT** + variables globales + `adc_if_wcet_report()` |

**Hardware — un solo potenciómetro:**

| Conexión | Pin NUCLEO |
|----------|------------|
| Extremo 1 del potenciómetro | **3.3 V** |
| Extremo 2 del potenciómetro | **GND** |
| Cursor (wiper) | **PA0** (conector Arduino **A0**) |

**Flujo de muestreo (LIO + DMA):**

1. `write_adc()` encola `ADC_IN_CMD_SAMPLE` en el **input spooler** (`queue_in`). Retorna de inmediato.
2. `task_adc` (gatekeeper) ejecuta `HAL_ADC_Start_DMA` sobre buffer estático de 1 muestra.
3. Al completar, la ISR `HAL_ADC_ConvCpltCallback` libera el semáforo DMA.
4. El gatekeeper publica la muestra en el **output spooler** con `xQueueOverwrite` (LIO: la muestra anterior se descarta si no fue leída).
5. `read_adc()` / `read_adc_wait()` leen la última muestra disponible.

**Demostración funcional (`task_receiver`):**

| Acción | Período |
|--------|---------|
| `ioctl_adc(ADC_IOCTL_FLUSH)` + `write_adc()` + `read_adc_wait()` | Cada **250 ms** |
| Log `ADC raw=XXXX (~YY%)` | Cada **250 ms** |
| `adc_if_wcet_report()` | Cada **~5 s** |

---

### 6.2 Funciones utilizadas

#### API del Device Driver ADC (`task_adc_interface.c`)

| Función | Descripción |
|---------|-------------|
| `open_adc(h_adc)` | Crea colas estáticas, semáforo DMA y crea tarea gatekeeper |
| `release_adc(h_adc)` | Detiene DMA, elimina tarea, colas y semáforo |
| `write_adc(h)` | Encola solicitud de conversión en input spooler (**async**) |
| `read_adc(h, &value)` | Lee última muestra del output spooler LIO (**non-blocking**) |
| `read_adc_wait(h, &value, timeout)` | Lee muestra con timeout |
| `ioctl_adc(h, cmd)` | `FLUSH`, `START_SAMPLING`, `STOP_SAMPLING` |
| `adc_if_wcet_report()` | Imprime WCET de interfaz y gatekeeper |

#### Tarea gatekeeper (`task_adc.c`)

| Tarea | Descripción |
|-------|-------------|
| `task_adc` | Atiende input spooler, ejecuta `HAL_ADC_Start_DMA`, publica en output spooler con LIO |

#### Callback HAL (`app_it.c`)

| Callback | Acción |
|----------|--------|
| `HAL_ADC_ConvCpltCallback` | Notifica fin de conversión DMA al gatekeeper (`adc_dma_cplt_notify_from_isr`) |

---

### 6.3 Configuración ADC (CubeMX / `main.c`)

| Parámetro | Valor |
|-----------|-------|
| Periférico | ADC1 |
| Resolución | 12 bits (0–4095) |
| Canal | `ADC_CHANNEL_0` (PA0) |
| Conversiones | **1** (un potenciómetro) |
| Sampling time | 480 ciclos |
| Trigger | Software |
| DMA | DMA2 Stream0, halfword, normal mode |

---

### 6.4 Resultados de prueba — Log de consola

#### Reporte WCET medido (ciclo cnt=20, ~5 s de ejecución)

```
=== WCET Funciones Interfaz ADC [us] ===
  open_adc()    : 120
  release_adc() : 0
  write_adc()   : 63
  read_adc()    : 0
  ioctl_adc()   : 6
=== WCET Gatekeeper ADC [us] ===
  task_adc      : 22
  DMA ISR cnt   : 19
```

#### Lecturas del potenciómetro (potenciómetro cerca del máximo)

```
  ADC raw=4095  (~100)  cnt=20
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=21
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=22
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=23
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4094  (~99)   cnt=24
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=25
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4094  (~99)   cnt=26
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=27
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4095  (~100)  cnt=28
   ==> Task RECEIVER - Wait:   250mS
  ADC raw=4091  (~99)   cnt=29
   ==> Task RECEIVER - Wait:   250mS
```

#### Video de demostración

Demostración en hardware (potenciómetro en PA0 / A0, lecturas ADC y depuración):

**[Ver video adc.mp4 en Google Drive](https://drive.google.com/file/d/1TkHStvvY1bHbnLmzCB7HiIS0XX9FDQvs/view?usp=sharing)**

---

### 6.5 Explicación del reporte WCET

El **WCET (Worst Case Execution Time)** es el peor tiempo de ejecución observado de cada función, medido con el contador **DWT** (Data Watchpoint and Trace) del Cortex-M4. Cada función de interfaz ejecuta `cycle_counter_reset()` al inicio y `cycle_counter_get_time_us()` al final; se guarda el máximo histórico en una variable global.

#### Funciones de interfaz

| Función | WCET [µs] | Explicación |
|---------|-----------|-------------|
| `open_adc()` | **120** | Peor caso al inicializar el driver: creación de colas estáticas (`xQueueCreateStatic`), semáforo binario estático, registro en el registry de FreeRTOS y creación de la tarea gatekeeper `task_adc`. Es la función más costosa de la interfaz porque configura todo el subsistema. |
| `release_adc()` | **0** | No fue invocada durante la prueba (solo se llama al cerrar el driver). El valor 0 indica que no hubo ejecución medida. |
| `write_adc()` | **63** | Peor caso al encolar una solicitud de conversión (`ADC_IN_CMD_SAMPLE`) en el input spooler. Incluye validación del handle, armado del comando y `xQueueSend` con timeout. Es **asíncrona**: retorna sin esperar la conversión DMA. |
| `read_adc()` | **0** | No se usa directamente en la demo (`task_receiver` usa `read_adc_wait()`). El WCET de `read_adc()` permanece en 0 porque esa función no fue ejecutada en el camino medido. |
| `ioctl_adc()` | **6** | Peor caso al vaciar el output spooler con `ADC_IOCTL_FLUSH` antes de cada muestreo. Operación liviana: drenar la cola LIO de profundidad 1. |

#### Gatekeeper e ISR

| Elemento | WCET / Valor | Explicación |
|----------|--------------|-------------|
| `task_adc` | **22 µs** | Peor caso de una iteración del gatekeeper en modo one-shot: recibir comando del input spooler, iniciar `HAL_ADC_Start_DMA`, esperar semáforo DMA y publicar muestra con `xQueueOverwrite`. |
| `DMA ISR cnt` | **19** | Número acumulado de interrupciones por conversión DMA completada (`HAL_ADC_ConvCpltCallback`). Coherente con ~19 conversiones ejecutadas antes del reporte en cnt=20. |

#### Relación WCET interfaz vs. gatekeeper

- Las funciones de interfaz (`write_adc` = 63 µs, `ioctl_adc` = 6 µs) son **rápidas** porque solo encolan comandos o vacían colas; no acceden al hardware ADC directamente.
- El trabajo pesado (DMA + espera de conversión) lo realiza **`task_adc`** (22 µs medidos por iteración de gatekeeper), respetando el patrón **Gatekeeper**: una sola tarea es dueña del periférico.
- `open_adc()` (120 µs) concentra el costo de setup inicial; ocurre una sola vez al arrancar.

---

### 6.6 Explicación de las lecturas ADC

| Observación | Interpretación |
|-------------|----------------|
| Valores **4091–4095** (~99–100 %) | El potenciómetro estaba girado cerca del **máximo** (cursor hacia 3.3 V en PA0). |
| Variación ±1–4 LSB (4091 vs 4095) | **Ruido normal** de un ADC de 12 bits con potenciómetro; no indica fallo. Típico en entradas analógicas sin filtrado adicional. |
| Período **250 ms** entre lecturas | Lo impone `vTaskDelay(TASK_RECEIVER_DEL_MAX)` en `task_receiver`. |
| `cnt` incrementa de 20 a 29 | Confirma ejecución periódica estable de la tarea receptora. |
| `DMA ISR cnt = 19` al reportar en cnt=20 | Cada `write_adc()` dispara una conversión DMA; el contador ISR confirma que el hardware responde correctamente. |

**Live Expressions verificadas en depuración:**

| Expresión | Valor observado |
|-----------|-----------------|
| `task_adc_dta.dma_buffer[0]` | 4095 (coincide con log `ADC raw=4095`) |
| `g_adc_dma_isr_cnt` | 3 (en instante de captura; sigue incrementando en runtime) |
| `g_task_receiver_cnt` | 3 (en instante de captura) |
| `g_task_adc_runtime_us` | 22 |
| `g_adc_if_write_wcet_us` | 63 |
| `g_adc_if_read_wcet_us` | 0 |
| `hal_xxxx_callback_cnt` | 4 |

La variable **`task_adc_dta.dma_buffer[0]`** es la más directa para observar el valor ADC en Live Expressions; refleja la última muestra transferida por DMA antes de publicarse en el output spooler LIO.

---

### 6.7 Observaciones generales

1. **Patrón LIO:** si el consumidor no lee a tiempo, la muestra más reciente reemplaza a la anterior mediante `xQueueOverwrite`; no se acumulan lecturas obsoletas.
2. **Gatekeeper:** solo `task_adc` invoca `HAL_ADC_Start_DMA`, serializando el acceso al periférico.
3. **Colas estáticas:** toda la memoria del spooler se reserva en tiempo de compilación/enlace (`queue_in_storage`, `queue_out_storage`, `sem_dma_done_struct`).
4. **Driver funcional:** el potenciómetro en PA0 responde correctamente; lecturas estables cerca del máximo con ruido de pocos LSB.
5. **`release_adc()`** no se invocó en la prueba; su WCET permanece en 0.

---

### 6.8 Variables globales WCET

| Variable | Función / elemento medido |
|----------|---------------------------|
| `g_adc_if_open_wcet_us` | `open_adc()` |
| `g_adc_if_release_wcet_us` | `release_adc()` |
| `g_adc_if_write_wcet_us` | `write_adc()` |
| `g_adc_if_read_wcet_us` | `read_adc()` |
| `g_adc_if_ioctl_wcet_us` | `ioctl_adc()` |
| `g_task_adc_runtime_us` | Iteración gatekeeper `task_adc` |
| `g_adc_dma_isr_cnt` | Contador de interrupciones DMA completadas |
| `task_adc_dta.dma_buffer[0]` | Última muestra ADC (Live Expression) |

---

## Referencias

- Patrón gatekeeper + spooler: TP1 Actividad 01 (I2C) y Actividad 02 (UART)
- Documentación HAL: `stm32f4xx_hal_adc.c`, `stm32f4xx_hal_dma.c`
- FreeRTOS: `xQueueCreateStatic`, `xQueueOverwrite`, `xSemaphoreCreateBinaryStatic`
- Video de demostración: [adc.mp4](https://drive.google.com/file/d/1TkHStvvY1bHbnLmzCB7HiIS0XX9FDQvs/view?usp=sharing)

---

*Documento de entrega — Paso 06: Device Driver ADC FreeRTOS (potenciómetro en PA0 / A0).*
