# FSM de 2 estados + LED + botón + WiFi + MQTT

Ejemplo de una **máquina de estados finitos (FSM)** de dos estados implementada en un **ESP32-S3** utilizando **ESP-IDF**. El sistema permite controlar un LED mediante un botón físico o a través de comandos enviados por **MQTT**, manteniendo el estado sincronizado entre el dispositivo y cualquier cliente conectado.

---

## 1. Hardware

El ejemplo utiliza la siguiente configuración de hardware:

- **LED** conectado al **GPIO2**, acompañado de una resistencia limitadora en serie (220 Ω a 330 Ω).
- **Botón** conectado al **GPIO0**. En la mayoría de las tarjetas de desarrollo este corresponde al botón **BOOT**, ya conectado internamente a GND. Si se utiliza un botón externo, basta con conectar un terminal al GPIO0 y el otro a GND, ya que el firmware habilita la resistencia **pull-up interna**.

---

## 2. Configuración de WiFi y MQTT

Los parámetros de conexión se encuentran en el archivo `main/app_config.h`.

```c
#define WIFI_SSID       "TU_WIFI"
#define WIFI_PASSWORD   "TU_PASSWORD"
#define MQTT_BROKER_URI "mqtt://192.168.1.100:1883"
```

El broker MQTT puede ser:

- **Local**, utilizando un servidor como **Mosquitto** instalado en un equipo de la red.
- **Público de pruebas**, por ejemplo:

```text
mqtt://broker.hivemq.com:1883
```

Los brokers públicos son útiles para realizar pruebas, aunque no se recomienda utilizarlos en aplicaciones reales debido a que cualquier usuario puede acceder a los tópicos publicados.

---

## 3. Compilación y carga del firmware

```bash
# Configurar el objetivo de compilación
idf.py set-target esp32s3

# Compilar el proyecto
idf.py build

# Flashear y abrir el monitor serie
idf.py -p COM5 flash monitor         # Windows
idf.py -p /dev/ttyUSB0 flash monitor # Linux
```

También es posible realizar estas operaciones desde **Visual Studio Code** mediante la extensión oficial de **ESP-IDF**.

---

## 4. Funcionamiento de la máquina de estados

El módulo `fsm_led.c` implementa una máquina de estados de dos estados claramente definidos:

```text
FSM_STATE_OFF --(ON o TOGGLE)--> FSM_STATE_ON  -> LED encendido
FSM_STATE_ON  --(OFF o TOGGLE)--> FSM_STATE_OFF -> LED apagado
```

Los comandos (`FSM_CMD_ON`, `FSM_CMD_OFF` y `FSM_CMD_TOGGLE`) pueden generarse desde dos fuentes diferentes:

- **Botón físico**, gestionado por `button_task`, que detecta la pulsación y solicita un cambio de estado.
- **Cliente MQTT**, mediante `wifi_mqtt.c`, que interpreta los mensajes recibidos en el tópico de control.

Ambos orígenes envían sus solicitudes a la función `fsm_led_request()`, la cual utiliza una cola segura para hilos (*thread-safe*).

Una única tarea, `fsm_task`, es responsable de administrar el estado de la máquina, actualizar el LED y ejecutar `on_state_changed()`, función encargada de publicar el nuevo estado mediante MQTT.

Esta arquitectura evita lógica duplicada y garantiza que el botón físico y el control remoto permanezcan completamente sincronizados.

---

## 5. Topics MQTT

| Topic | Dirección | Payload |
|--------|-----------|---------|
| `fsm_led/1/state` | ESP32 → Cliente MQTT | `"ON"` / `"OFF"` *(retained)* |
| `fsm_led/1/set` | Cliente MQTT → ESP32 | `"ON"` / `"OFF"` / `"TOGGLE"` |

El tópico `fsm_led/1/state` utiliza el atributo **retained**, permitiendo que cualquier cliente que se conecte posteriormente reciba inmediatamente el último estado publicado sin necesidad de esperar un nuevo cambio.

---

## 6. Control desde un cliente MQTT

El proyecto es compatible con cualquier cliente MQTT para PC o dispositivos móviles.

La configuración básica consiste en:

1. Conectarse al broker MQTT configurado.
2. Suscribirse al tópico:

```text
fsm_led/1/state
```

para visualizar el estado actual del LED.

3. Publicar alguno de los siguientes comandos en el tópico:

```text
fsm_led/1/set
```

Comandos disponibles:

- `ON`
- `OFF`
- `TOGGLE`

De esta forma es posible controlar el LED remotamente mientras el estado permanece sincronizado con el botón físico conectado al ESP32.

---

## 7. Estructura del proyecto

```text
main/
│
├── pin_config.h      Configuración del LED y botón
├── app_config.h      Configuración de WiFi, MQTT y topics
├── fsm_led.c/h       Implementación de la máquina de estados
├── wifi_mqtt.c/h     Conexión WiFi, cliente MQTT y recepción de comandos
└── app_main.c        Integración entre la FSM y el sistema MQTT
```