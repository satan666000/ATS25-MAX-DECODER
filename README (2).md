# Radio KALIMAN — ATS25 Max Decoder Firmware

Firmware personalizado para **ATS25 max-Decoder** basado en ESP32 y chip de radio **SI4735**, desarrollado por **KALIMAN/KO6EAI** (versión `1.9.0`). Incluye interfaz gráfica optimizada para pantalla táctil y encoder rotatorio, múltiples estilos de HUD, decodificación RDS, soporte SSB con parche, gestión Wi-Fi y consulta de propagación / actividad HF.

---

## Características principales

* **Recepción multibanda (30 bandas):** FM, OM/MW, OC/SW, OL/LW, Balizas, bandas de aficionados (`630M` a `10M`), banda corrida de 11m y canalero CB de 40 canales (`26.965 MHz` – `27.405 MHz`).
* **Modos de operación:** FM (Mono / Estéreo / Auto), AM, LSB y USB, con sintonía fina (BFO) en SSB y sincronía de portadora (`SYNC`).
* **HUDs y personalización visual:**
  * **Estilos de HUD:** *Órbitas* (planeta central con anillos orbitales de RSSI y SNR y marcadores de pico), *Consola*, *Observatorio* y *Trazo RSSI*.
  * **15 paletas de color** (incluyendo *Oscuro Rainbow*, *Azul sonar*, *Ámbar estudio*, *Orbita RX*, entre otras) y selector de idioma.
  * Animación de bienvenida en *sprite* fuera de pantalla antes de cargar la interfaz principal.
* **Control de señal y audio:**
  * AGC automático o atenuación manual (hasta 26 en FM / 37 en otros modos).
  * Filtro AM/SSB automático o fijo, AVC AM (hasta 36 dB) y control de silencio/audio.
  * Búsqueda automática de emisoras (`Seek`) hacia arriba o hacia abajo en modos compatibles.
* **RDS en FM:** lectura y ensamble validado de nombre de emisora (`PS`) y radiotexto (`RT`) con desplazamiento horizontal automático.
* **Memorias:** 4 espacios de memoria persistentes (pulsar para sintonizar, mantener presionado para guardar).
* **Red Wi-Fi, propagación y actividad HF:** escaneo de redes 2.4 GHz, teclado en pantalla para clave, indicadores de propagación día/noche (`D`/`N`) y visor de actividad HF (`HfActivity`).
* **Persistencia en NVS (`Preferences`):** guarda banda, frecuencia, modo, paso, volumen, ajustes de cada banda, memorias, HUD, paleta e idioma.

---

## Hardware y pines configurados en `main.cpp`

* **Microcontrolador / Pantalla:** ESP32 con `TFT_eSPI` (320×240, rotación `1`) y calibración táctil `XPT2046` (`{387, 3530, 246, 3555, 7}`).
* **Bus I2C (`Wire`):** `SDA = 21`, `SCL = 22` (receptor SI4735).
* **Pines de control:**
  * `RESET = 12`
  * `LIGHT = 14` (retroiluminación)
  * `MUTE = 27`
  * `EA = 17` y `EB = 16` (encoder rotatorio por interrupciones)
  * `KEY = 33` (botón analógico con lectura `analogRead(KEY) < 500`, pulsación corta y larga ≥ 700 ms)
  * Pin `32` configurado como salida en `LOW` al inicio

---

## Dependencias y módulos del proyecto

El archivo `src/main.cpp` se integra con los siguientes encabezados y bibliotecas:

* `RadioNetwork.h` y `HfActivity.h` (gestión Wi-Fi, pronóstico HF y reportes de actividad)
* `FixedText.h` y `Localization.h` (texto e internacionalización)
* `patch_init.h` (parche SSB)
* Bibliotecas Arduino / ESP32: `TFT_eSPI`, `SI4735` (PU2CLR), `Preferences`, `Wire`, `esp_heap_caps.h`, `esp_system.h`

---

## Navegación y controles

* **Pantalla principal (`Radio`):**
  * Girar el encoder ajusta **Sintonía**, **Volumen** o **Fino (BFO)** según el control activo (o navega entre controles/botones cuando está activo el modo navegación con el botón).
  * Barra inferior rápida: **Ajustes**, **Buscar**, **Memoria** y **Info**.
* **Submenús principales:** **Bandas**, **Sintonía**, **Recepción** (AGC, Filtro y audio, SYNC), **Estado**, **Apariencia** y **Wi-Fi** (escaneo, teclado y actividad HF).

---

## Créditos y licencias

* **Firmware / Aplicación:** Radio KALIMAN (`1.9.0`) — **KALIMAN/KO6EAI**.
* **Bibliotecas y componentes de terceros:** PU2CLR (`SI4735`), `TFT_eSPI`, `XPT2046`, `PubSubClient` (Nick O'Leary / MIT) y parche SSB (Vadim Afonkin), conservando sus respectivos avisos y licencias.
