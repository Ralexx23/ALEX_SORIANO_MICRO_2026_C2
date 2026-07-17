# Control RC — ESP32-S3 (ESP-IDF)

Firmware para el control RC basado en tu esquemático de KiCad: 2 joysticks
analógicos, 8 botones digitales + 2 de calibración, MPU-6050 (IMU), OLED
I2C, enlace RF (RX demodulador / TX), y telemetría por MQTT.

## 1. Antes de compilar: verifica `main/pin_config.h`

El PDF que enviaste es un "Print to PDF" de KiCad sin texto seleccionable
(son trazos vectoriales), así que lo inspeccioné rasterizándolo en alta
resolución. Pude confirmar la arquitectura general (4 canales ADC para
joysticks, bus I2C compartido para OLED+MPU6050, GPIO17/18 para el RF),
pero **no puedo garantizar al 100% el número exacto de cada GPIO
individual de los botones** a partir de la imagen. Abre `pin_config.h`
y compara cada `#define` contra tu esquemático/netlist real antes de
flashear. Es el único archivo que deberías necesitar tocar por temas de
pines — el resto del firmware solo usa esas macros.

## 2. Configura WiFi/MQTT

Edita `main/app_config.h`:
- `WIFI_SSID` / `WIFI_PASSWORD`
- `MQTT_BROKER_URI` (un broker local tipo Mosquitto en tu PC, o uno
  público de pruebas como `mqtt://broker.hivemq.com:1883`)

Para verlo en el celular, cualquier app cliente MQTT (ej. "IoT MQTT
Panel", "MQTT Explorer" en PC) suscrita a `rc_controller/1/telemetry`
te mostrará el JSON con joystick, IMU y botones en tiempo real.

## 3. Compilar y flashear

```bash
# primera vez: configurar target
idf.py set-target esp32s3

# compilar
idf.py build

# flashear y ver logs (ajusta el puerto)
idf.py -p COM5 flash monitor        # Windows
idf.py -p /dev/ttyUSB0 flash monitor # Linux
```

Desde VS Code: usa la extensión oficial "ESP-IDF" — abre esta carpeta
como proyecto, selecciona el target `esp32s3` en la barra inferior, y
usa los botones Build / Flash / Monitor.

## 4. Cómo se cumple cada requerimiento

1. **Joysticks -100..+100**: `joystick.c` lee ADC1 (4 canales) y mapea
   cada eje a -100..+100 respecto al centro calibrado, con zona muerta
   del 4% para evitar ruido en reposo.
2. **4 botones gatillo**: `PIN_BTN_TRIG_L1..R2` en `pin_config.h`,
   debounceados por polling en `buttons.c`.
3. **4 botones frontales**: `PIN_BTN_FRONT_A/B/X/Y`, mismo mecanismo.
4. **Calibración (2 botones superiores, 3s)**: `buttons.c` detecta el
   combo sostenido y `control_task.c` dispara
   `joystick_calibrate_center()` + `mpu6050_calibrate_zero()`, ambos
   persistidos en NVS (sobreviven a reinicios).
5. **Pantalla gráfica**: `display.c` dibuja, sin depender de una fuente
   de texto completa (evité reinventar una a mano): crosshair de cada
   joystick, un círculo con línea de inclinación para el giroscopio,
   valores del acelerómetro con dígitos estilo "7 segmentos", e iconos
   de estado WiFi/MQTT/calibración. Es la combinación gráfico+numérico
   que pediste, priorizando robustez sobre estética por ahora.
6. **MQTT**: `wifi_mqtt.c` conecta WiFi STA y publica JSON en
   `<TOPIC_BASE>/telemetry` cada ~10Hz (`control_task.c` limita la
   frecuencia de publicación para no saturar el broker).
7. **Giroscopio como joystick adicional**: `control_task.c` fusiona
   `joy.rx/ry` (joystick) con `imu.tilt_x/tilt_y` (inclinación del
   MPU6050 mapeada también a -100..+100) por suma saturada — el
   vehículo responde tanto al joystick como a inclinar el control.
8. **Datos del acelerómetro**: expuestos en `imu_data_t` (g's crudos) y
   mostrados en pantalla; también van en el JSON de MQTT.

## 5. Sobre el enlace RF (GPIO17/18) — léelo con atención

No especificaste el chip/protocolo exacto del demodulador ni del
receptor destino, así que `rf_link.c` implementa:

- **TX (GPIO17)**: un framing OOK/PWM genérico tipo "rc-switch",
  compatible con la mayoría de módulos ASK 315/433MHz económicos.
  Empaqueta joystick + comando fusionado + botones + checksum en una
  trama de pocos bytes.
- **RX (GPIO18)**: captura cruda de pulsos vía el periférico RMT
  (`rf_link_get_last_rx_symbol_count()`), pensada para que analices el
  protocolo real de tu receptor con un osciloscopio o logic analyzer
  antes de escribir el decodificador definitivo.

Si me confirmas el chip del demodulador (o el protocolo que espera tu
receptor), puedo escribir el decodificador/encodificador exacto en vez
del framing genérico.

## 6. Estructura

```
main/
  pin_config.h    <- verificar contra el esquemático
  app_config.h    <- WiFi / MQTT
  joystick.c/h    <- ADC + calibración + mapeo -100..100
  buttons.c/h     <- debounce + combo de calibración
  mpu6050.c/h     <- IMU: accel/gyro + filtro complementario
  i2c_bus.c/h     <- bus I2C compartido (OLED + MPU6050)
  display.c/h     <- UI gráfica en el OLED
  rf_link.c/h     <- TX/RX por RMT
  wifi_mqtt.c/h   <- WiFi STA + MQTT
  control_task.c/h<- ciclo principal (50Hz) que conecta todo
  app_main.c
```
