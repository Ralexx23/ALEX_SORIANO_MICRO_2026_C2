# Control RC — ESP32-S3 (ESP-IDF)

Firmware desarrollado para un control remoto (RC) basado en **ESP32-S3** utilizando **ESP-IDF**. El sistema integra **dos joysticks analógicos**, **ocho botones digitales**, **dos botones de calibración**, un sensor **MPU-6050 (IMU)**, una pantalla **OLED mediante I2C**, comunicación por **radiofrecuencia (RF)** y transmisión de telemetría mediante **MQTT**.

---

## 1. Configuración de pines

Antes de compilar el proyecto, se recomienda verificar el archivo `main/pin_config.h` y confirmar que la asignación de los GPIO coincida con el esquemático del hardware utilizado.

La arquitectura del firmware contempla:

- Cuatro canales ADC para la lectura de los joysticks.
- Un bus I2C compartido entre la pantalla OLED y el sensor MPU-6050.
- Los GPIO 17 y 18 destinados a la comunicación RF.

Toda la configuración relacionada con los pines se centraliza en `pin_config.h`, por lo que normalmente no es necesario modificar otros archivos del firmware.

---

## 2. Configuración de WiFi y MQTT

Los parámetros de conexión se encuentran en `main/app_config.h`.

Configurar los siguientes valores:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_BROKER_URI` (puede utilizarse un broker local como **Mosquitto** o uno público de pruebas como `mqtt://broker.hivemq.com:1883`).

La información de telemetría se publica en el tópico:

```text
rc_controller/1/telemetry
```

Los datos pueden visualizarse utilizando cualquier cliente MQTT, como **MQTT Explorer** en PC o aplicaciones móviles compatibles.

---

## 3. Compilación y carga del firmware

```bash
# Primera configuración del target
idf.py set-target esp32s3

# Compilar el proyecto
idf.py build

# Flashear y abrir el monitor serie
idf.py -p COM5 flash monitor         # Windows
idf.py -p /dev/ttyUSB0 flash monitor # Linux
```

También es posible realizar estas operaciones desde **Visual Studio Code** utilizando la extensión oficial de **ESP-IDF**, seleccionando el objetivo `esp32s3` y empleando las opciones **Build**, **Flash** y **Monitor**.

---

## 4. Funcionalidades implementadas

### Joysticks analógicos

El módulo `joystick.c` realiza la lectura de cuatro canales ADC y convierte cada eje a un rango comprendido entre **-100 y +100** respecto al centro calibrado. Además, incorpora una zona muerta del **4 %** para minimizar el ruido cuando los joysticks permanecen en reposo.

### Botones

El sistema dispone de:

- **Cuatro botones tipo gatillo**, definidos mediante `PIN_BTN_TRIG_L1..R2`.
- **Cuatro botones frontales**, definidos mediante `PIN_BTN_FRONT_A/B/X/Y`.

Todos implementan eliminación de rebotes (*debouncing*) mediante sondeo periódico en `buttons.c`.

### Calibración

Al mantener presionados simultáneamente los dos botones superiores durante aproximadamente **3 segundos**, se ejecuta el proceso de calibración del centro de los joysticks y del punto cero del MPU-6050.

Los valores obtenidos se almacenan en la memoria **NVS**, por lo que permanecen disponibles incluso después de reiniciar el dispositivo.

### Pantalla OLED

El módulo `display.c` genera una interfaz gráfica que incluye:

- Indicadores de posición para ambos joysticks.
- Visualización de la inclinación del control.
- Valores del acelerómetro.
- Iconos de estado para WiFi, MQTT y calibración.

La interfaz combina elementos gráficos y numéricos priorizando un funcionamiento estable y ligero.

### Comunicación MQTT

El módulo `wifi_mqtt.c` establece la conexión WiFi en modo estación (STA) y publica la información del control en formato JSON mediante el tópico:

```text
<TOPIC_BASE>/telemetry
```

Las publicaciones se realizan aproximadamente a **10 Hz**, frecuencia controlada desde `control_task.c` para evitar una carga innecesaria sobre el broker MQTT.

### Control mediante IMU

Además de los joysticks físicos, la inclinación obtenida desde el MPU-6050 puede utilizarse como una entrada adicional.

El sistema combina las entradas de los joysticks y los datos de inclinación mediante una suma saturada, permitiendo controlar el vehículo tanto con los joysticks como inclinando el control.

### Datos del acelerómetro

Las mediciones del acelerómetro se encuentran disponibles en la estructura `imu_data_t`, se muestran en la pantalla OLED y también forman parte de la telemetría enviada mediante MQTT.

---

## 5. Comunicación RF

El módulo `rf_link.c` implementa una base para la comunicación mediante radiofrecuencia utilizando los GPIO 17 y 18.

### Transmisión (GPIO17)

Se implementa un protocolo genérico basado en **OOK/PWM**, compatible con numerosos módulos ASK de **315 MHz** y **433 MHz**.

Cada trama transmitida incluye:

- Estado de los joysticks.
- Datos del control por inclinación.
- Estado de los botones.
- Checksum de verificación.

### Recepción (GPIO18)

La recepción utiliza el periférico **RMT** del ESP32-S3 para capturar los pulsos provenientes del receptor RF mediante la función:

```cpp
rf_link_get_last_rx_symbol_count()
```

Esta implementación sirve como base para desarrollar un decodificador específico según el protocolo utilizado por el hardware de destino.

---

## 6. Estructura del proyecto

```text
main/
│
├── pin_config.h       Configuración de GPIO
├── app_config.h       Configuración de WiFi y MQTT
├── joystick.c/h       Lectura ADC, calibración y mapeo
├── buttons.c/h        Gestión de botones y debounce
├── mpu6050.c/h        Control del MPU-6050 y cálculo de inclinación
├── i2c_bus.c/h        Bus I2C compartido (OLED + MPU6050)
├── display.c/h        Interfaz gráfica para la pantalla OLED
├── rf_link.c/h        Comunicación RF mediante RMT
├── wifi_mqtt.c/h      Conexión WiFi y cliente MQTT
├── control_task.c/h   Lógica principal del sistema
└── app_main.c         Punto de entrada de la aplicación
```
