este entorno configura la placa ESP32 amarilla 4 pulgadas de librería TFT
contiene varios proyectos separados por nel prefijo main_

## Red privada para receptores ESP32

El ESP32 emisor se conecta al router de casa para recibir Internet y, al mismo
tiempo, crea una red Wi-Fi privada para los receptores. Cambia `WIFI_AP_SSID` y
`WIFI_AP_PASSWORD` en `include/wifi_credentials.h`; la clave debe tener al
menos 8 caracteres. La red privada usa la direccion `192.168.50.1/24`.

Mientras reproduce una emisora, el emisor envia los bytes MP3 mediante UDP
multicast a `239.255.42.99:12345` por la interfaz de su AP privado. Los ESP32
receptores deben conectarse a la red privada y unirse al grupo multicast en el
puerto `12345`. Cada datagrama comienza con los cuatro bytes `AMR1`, seguidos
por cuatro bytes de numero de secuencia big-endian; el resto del datagrama son
bytes MP3. Cada paquete contiene hasta 1200 bytes de audio. UDP no garantiza
entrega ni orden; el numero de secuencia permite detectar perdidas.

La red privada distribuye el audio, pero no comparte Internet con los
receptores. En modo punto de acceso y cliente, el ESP32 usa el mismo canal Wi-Fi
que el router al que esta conectado.

### Control desde un telefono

Conecta el telefono a la red `WIFI_AP_SSID` del transmisor y abre
`http://192.168.50.1/` en el navegador. La pagina permite reproducir o pausar
la emisora seleccionada en la pantalla y subir o bajar el volumen en pasos de
10%. Para iniciar una emisora desde el telefono, primero seleccionala en la
pantalla del transmisor. La red privada no necesita acceso a Internet para
usar estos controles.

### Proyecto receptor

El firmware receptor esta separado en `receptor_esp32`. Abre esa carpeta como
proyecto PlatformIO y carga su firmware en cada ESP32 receptor. Ajusta
`receptor_esp32/include/wifi_credentials.h` para que SSID y clave coincidan con
los valores `WIFI_AP_SSID` y `WIFI_AP_PASSWORD` configurados en el transmisor.
El receptor usa el DAC interno y el mismo pin de habilitacion del parlante
(GPIO 4), por lo que requiere el mismo circuito de salida de audio.
