<!-- TOP DYNAMIC BANNER / BANNER SUPERIOR DINÁMICO -->
<div align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=0:0f2027,50:203a43,100:2c5364&height=220&section=header&text=Radio%20KALIMAN&fontSize=55&fontColor=00f2fe&animation=fadeIn&fontAlignY=38&desc=ATS25%20Max-Decoder%20DSP%20Firmware%20%7C%20ESP32%20%2B%20SI4735&descAlignY=60&descSize=18" alt="Radio KALIMAN Banner" width="100%"/>
</div>

<!-- BADGES / INSIGNIAS -->
<div align="center">

  ![Version](https://img.shields.io/badge/Version-1.9.0-00f2fe?style=for-the-badge&logo=github&logoColor=white)
  ![Hardware](https://img.shields.io/badge/Hardware-ATS25%20Max--Decoder-203a43?style=for-the-badge&logo=espressif&logoColor=white)
  ![DSP Chip](https://img.shields.io/badge/Chip-SI4735%20DSP-2c5364?style=for-the-badge&logo=radio&logoColor=white)
  ![Author](https://img.shields.io/badge/Author-KALIMAN%20%2F%20KO6EAI-ff6b6b?style=for-the-badge)
  ![License](https://img.shields.io/badge/License-MIT%20%2F%20Open%20Source-success?style=for-the-badge)

</div>

<!-- LANGUAGE SELECTOR / SELECTOR DE IDIOMA -->
<div align="center">
  <h3>🌐 <a href="#-español">Español</a> | <a href="#-english">English</a></h3>
</div>

---

# 🇪🇸 Español

<p align="center">
  <b>Firmware personalizado de alto rendimiento para receptores multibanda ATS25 max-Decoder.</b><br>
  <i>Interfaz táctil fluida, telemetría orbital, decodificación RDS, parche SSB y pronóstico de propagación HF en tiempo real.</i>
</p>

<p align="center">
  <a href="#-características-principales">Características</a> •
  <a href="#-estilos-de-hud-y-personalización">Estilos de HUD</a> •
  <a href="#-hardware-y-configuración-de-pines">Hardware y Pinout</a> •
  <a href="#-arquitectura-y-dependencias">Arquitectura</a> •
  <a href="#-navegación-y-controles">Controles</a> •
  <a href="#-instalación-y-compilación">Instalación</a>
</p>

---

## 📖 Descripción General

**Radio KALIMAN (v1.9.0)** es un firmware desarrollado desde cero por **KALIMAN / KO6EAI** para exprimir al máximo el potencial del microcontrolador **ESP32** y el receptor DSP **SI4735** dentro de la plataforma **ATS25 max-Decoder**.

Diseñado pensando tanto en el radioescucha (SWL) como en el radioaficionado activo, combina una estética moderna de telemetría con controles ergonómicos híbridos (pantalla táctil resistiva + encoder rotatorio con interrupciones).

---

## ✨ Características Principales

### 📡 Recepción Multibanda y Modulación
* **30 Bandas Preconfiguradas:** Cobertura completa desde Onda Larga hasta FM comercial:
  * **Radiodifusión:** FM, OM/MW, OC/SW, OL/LW y Balizas (Beacons).
  * **Radioaficionados:** Bandas completas desde `630M` hasta `10M`.
  * **Banda Ciudadana (CB):** Banda corrida de 11m y canalero dedicado de 40 canales (`26.965 MHz` – `27.405 MHz`).
* **Modos de Operación:** `FM` (Mono / Estéreo / Auto), `AM`, `LSB` y `USB`.
* **Sintonía de Precisión:** Control BFO (Beat Frequency Oscillator) para clarificación en SSB y sincronía de portadora (`SYNC`).

### 🎛️ Procesamiento de Señal y Audio (DSP)
* **Control de Ganancia:** AGC automático o atenuación manual escalonada (hasta `26 dB` en FM y `37 dB` en AM/SSB).
* **Filtrado Avanzado:** Ancho de banda AM/SSB automático o seleccionable manualmente, combinado con AVC AM (hasta `36 dB`) y control de silencio rápido (`MUTE`).
* **Búsqueda Inteligente (`Seek`):** Escaneo automático ascendente y descendente en bandas compatibles.
* **Decodificación RDS (FM):** Lectura y ensamble validado del nombre de la emisora (`PS`) y radiotexto (`RT`) con desplazamiento horizontal fluido.

### 🌐 Conectividad Wi-Fi y Propagación HF
* **Gestión de Red:** Escáner de redes Wi-Fi de 2.4 GHz integrado con teclado táctil en pantalla para ingreso de credenciales.
* **Telemetría Solar y HF (`HfActivity`):** Consulta en tiempo real de actividad de bandas HF e indicadores visuales de propagación Día/Noche (`D` / `N`).

### 💾 Memoria y Persistencia NVS
* **4 Memorias Rápidas:** Acceso directo en pantalla (pulsación corta para sintonizar, pulsación larga para guardar).
* **Almacenamiento No Volátil (`Preferences`):** Guarda automáticamente el último estado del equipo (banda, frecuencia, modo, paso de sintonía, volumen, ajustes individuales por banda, memorias, HUD, paleta de colores e idioma).

---

## 🎨 Estilos de HUD y Personalización

El motor gráfico utiliza *sprites* fuera de pantalla para ofrecer transiciones sin parpadeos (*flicker-free*), incluyendo una animación de arranque personalizada.

| Estilo de HUD | Descripción Visual |
| :--- | :--- |
| 🪐 **Órbitas** | Diseño futurista con planeta central, anillos orbitales dinámicos para RSSI/SNR y marcadores de pico en tiempo real. |
| 🖥️ **Consola** | Interfaz técnica de alta densidad de datos inspirada en equipos de laboratorio y telemetría. |
| 🔭 **Observatorio** | Vista panorámica enfocada en la lectura limpia de frecuencia y métricas de propagación. |
| 📈 **Trazo RSSI** | Graficador histórico en tiempo real de la intensidad de señal para monitoreo de desvanecimiento (*fading*). |

> **Personalización Total:** Incluye **15 paletas de color** integradas (como *Oscuro Rainbow*, *Azul Sonar*, *Ámbar Estudio*, *Órbita RX*, entre otras) y soporte multi-idioma mediante el motor `Localization`.

---

## 🔌 Hardware y Configuración de Pines

El firmware está calibrado específicamente para la arquitectura interna del **ATS25 max-Decoder** definida en `src/main.cpp`:

### Especificaciones de Pantalla y Táctil
* **Display:** TFT 320×240 px (controlado por `TFT_eSPI`, rotación `1`).
* **Touchscreen:** Controlador resistivo `XPT2046` con matriz de calibración `{387, 3530, 246, 3555, 7}`.

### Mapa de Pines (ESP32 Pinout)

| Componente / Función | Pin ESP32 | Protocolo / Notas Técnicas |
| :--- | :---: | :--- |
| **SI4735 SDA** | `GPIO 21` | Bus I2C (`Wire`) — Datos |
| **SI4735 SCL** | `GPIO 22` | Bus I2C (`Wire`) — Reloj |
| **SI4735 RESET** | `GPIO 12` | Control de reinicio por hardware |
| **TFT Backlight** | `GPIO 14` | Control de retroiluminación (`LIGHT`) |
| **Audio MUTE** | `GPIO 27` | Silenciamiento de etapa de salida |
| **Encoder A (`EA`)** | `GPIO 17` | Lectura por interrupciones de hardware |
| **Encoder B (`EB`)** | `GPIO 16` | Lectura por interrupciones de hardware |
| **Botón Encoder (`KEY`)** | `GPIO 33` | Entrada analógica (`analogRead < 500`, pulso largo `≥ 700 ms`) |
| **Auxiliar / Control** | `GPIO 32` | Inicializado como salida en estado `LOW` |

---

## 🧩 Arquitectura y Dependencias

El código fuente principal (`src/main.cpp`) está modularizado para facilitar su mantenimiento y escalabilidad:

### Módulos Internos
* `RadioNetwork.h` — Gestión de conexión Wi-Fi y peticiones de red.
* `HfActivity.h` — Procesamiento y renderizado de reportes de actividad HF y pronóstico de propagación.
* `FixedText.h` & `Localization.h` — Diccionarios de texto e internacionalización de la interfaz.
* `patch_init.h` — Carga del parche binario en la RAM del SI4735 para habilitar demodulación SSB.

### Bibliotecas Externas Requeridas
* [`TFT_eSPI`](https://github.com/Bodmer/TFT_eSPI) (Bodmer) — Motor gráfico de alta velocidad.
* [`SI4735`](https://github.com/pu2clr/SI4735) (PU2CLR) — Controlador principal del chip DSP.
* **Nativas ESP32:** `Preferences`, `Wire`, `esp_heap_caps.h`, `esp_system.h`.

---

## 🕹️ Navegación y Controles

La interfaz está diseñada para operarse con una sola mano usando el encoder o directamente mediante toques en pantalla:

### Pantalla Principal (`Radio`)
* **Encoder Rotatorio:** Ajusta dinámicamente **Sintonía**, **Volumen** o **Fino (BFO)** según el parámetro seleccionado.
* **Pulsación de Botón (`KEY`):** Alterna entre el ajuste directo de parámetros y el modo de navegación por botones en pantalla.
* **Barra de Acceso Rápido (Inferior):** `Ajustes` • `Buscar` • `Memoria` • `Info`.

### Menús del Sistema
1. **Bandas:** Selección directa entre las 30 bandas disponibles.
2. **Sintonía:** Configuración de pasos de frecuencia (*Step*) y modos de modulación.
3. **Recepción:** Ajustes finos de `AGC`, ancho de `Filtro`, `AVC` y detector `SYNC`.
4. **Estado:** Diagnóstico del sistema, memoria libre (*Heap*) y parámetros técnicos.
5. **Apariencia:** Selector de estilos de HUD, 15 paletas de colores e idioma.
6. **Wi-Fi:** Escáner de redes, teclado táctil y monitor de actividad solar/HF.

---

## 🚀 Instalación y Compilación

1. **Clonar el repositorio:**
   ```bash
   git clone https://github.com/tu-usuario/Radio-KALIMAN-ATS25.git
   cd Radio-KALIMAN-ATS25
   ```
2. **Configurar el entorno (PlatformIO / Arduino IDE):**
   * Selecciona la placa **ESP32 Dev Module**.
   * Asegúrate de configurar el archivo `User_Setup.h` de la librería `TFT_eSPI` acorde a los pines de tu placa ATS25.
3. **Compilar y subir:**
   * Conecta tu ATS25 max-Decoder vía USB y carga el firmware al ESP32.

---

## 📜 Créditos y Licencias

* **Desarrollo y Arquitectura:** **Radio KALIMAN (`v1.9.0`)** — Creado por **KALIMAN / KO6EAI**.
* **Reconocimientos y Terceros:**
  * **PU2CLR (Ricardo Lima Caratti):** Por la excelente biblioteca `SI4735` para Arduino/ESP32.
  * **Vadim Afonkin:** Por el desarrollo del parche SSB para la familia de chips Si473x.
  * **Bodmer:** Por las bibliotecas `TFT_eSPI` y soporte `XPT2046`.
  * **Nick O'Leary:** Por `PubSubClient` (Licencia MIT).

<br>

---

# 🇺🇸 English

<p align="center">
  <b>High-performance custom firmware for ATS25 max-Decoder multiband receivers.</b><br>
  <i>Smooth touch interface, orbital telemetry, RDS decoding, SSB patch support, and real-time HF propagation forecasting.</i>
</p>

<p align="center">
  <a href="#-key-features">Features</a> •
  <a href="#-hud-styles--customization">HUD Styles</a> •
  <a href="#-hardware--pinout-configuration">Hardware & Pinout</a> •
  <a href="#-architecture--dependencies">Architecture</a> •
  <a href="#-navigation--controls">Controls</a> •
  <a href="#-installation--building">Installation</a>
</p>

---

## 📖 Overview

**Radio KALIMAN (v1.9.0)** is a custom firmware built from the ground up by **KALIMAN / KO6EAI** to unlock the full potential of the **ESP32** microcontroller and **SI4735** DSP radio chip inside the **ATS25 max-Decoder** platform.

Designed for both Shortwave Listeners (SWL) and active Amateur Radio operators, it combines a modern telemetry aesthetic with ergonomic hybrid controls (resistive touchscreen + interrupt-driven rotary encoder).

---

## ✨ Key Features

### 📡 Multiband Reception & Modulation
* **30 Preconfigured Bands:** Full coverage from Longwave to commercial FM:
  * **Broadcast:** FM, MW (AM), SW, LW, and Beacons.
  * **Amateur Radio:** Full ham bands from `630M` to `10M`.
  * **Citizens Band (CB):** Continuous 11m band coverage plus a dedicated 40-channel CB mode (`26.965 MHz` – `27.405 MHz`).
* **Operating Modes:** `FM` (Mono / Stereo / Auto), `AM`, `LSB`, and `USB`.
* **Precision Tuning:** Fine BFO (Beat Frequency Oscillator) control for SSB clarification and synchronous carrier detection (`SYNC`).

### 🎛️ Signal & Audio Processing (DSP)
* **Gain Control:** Automatic Gain Control (AGC) or stepped manual attenuation (up to `26 dB` on FM and `37 dB` on AM/SSB).
* **Advanced Filtering:** Automatic or manually selectable AM/SSB bandwidth filters, paired with AM AVC (up to `36 dB`) and quick audio `MUTE`.
* **Smart Seek:** Automatic upward and downward station scanning on supported bands.
* **FM RDS Decoding:** Validated parsing and assembly of Program Service (`PS`) station names and smooth horizontally scrolling Radio Text (`RT`).

### 🌐 Wi-Fi Connectivity & HF Propagation
* **Network Management:** Built-in 2.4 GHz Wi-Fi scanner with an on-screen touch keyboard for password entry.
* **Solar & HF Telemetry (`HfActivity`):** Real-time HF band activity reports and Day/Night (`D` / `N`) propagation indicators.

### 💾 Memory & NVS Persistence
* **4 Quick Memories:** On-screen preset slots (short press to tune, long press to save).
* **Non-Volatile Storage (`Preferences`):** Automatically saves receiver state (band, frequency, mode, tuning step, volume, per-band settings, memory presets, HUD style, color palette, and language).

---

## 🎨 HUD Styles & Customization

The graphics engine utilizes off-screen *sprites* to deliver flicker-free transitions, including a custom boot splash animation.

| HUD Style | Visual Description |
| :--- | :--- |
| 🪐 **Orbits** | Futuristic layout featuring a central planet, dynamic orbital rings for RSSI/SNR, and real-time peak markers. |
| 🖥️ **Console** | High-density technical interface inspired by laboratory test equipment and telemetry monitors. |
| 🔭 **Observatory** | Panoramic view focused on clean frequency readouts and propagation metrics. |
| 📈 **RSSI Trace** | Real-time historical signal strength plotter for monitoring signal fading. |

> **Full Customization:** Includes **15 built-in color palettes** (such as *Dark Rainbow*, *Sonar Blue*, *Studio Amber*, *RX Orbit*, and more) along with multi-language support via the `Localization` engine.

---

## 🔌 Hardware & Pinout Configuration

The firmware is specifically calibrated for the internal hardware architecture of the **ATS25 max-Decoder** defined in `src/main.cpp`:

### Display & Touch Specifications
* **Display:** 320×240 px TFT (driven by `TFT_eSPI`, rotation `1`).
* **Touchscreen:** `XPT2046` resistive controller with calibration matrix `{387, 3530, 246, 3555, 7}`.

### Pinout Map (ESP32)

| Component / Function | ESP32 Pin | Protocol / Technical Notes |
| :--- | :---: | :--- |
| **SI4735 SDA** | `GPIO 21` | I2C Bus (`Wire`) — Data |
| **SI4735 SCL** | `GPIO 22` | I2C Bus (`Wire`) — Clock |
| **SI4735 RESET** | `GPIO 12` | Hardware reset control |
| **TFT Backlight** | `GPIO 14` | Backlight control (`LIGHT`) |
| **Audio MUTE** | `GPIO 27` | Output stage mute control |
| **Encoder A (`EA`)** | `GPIO 17` | Hardware interrupt-driven input |
| **Encoder B (`EB`)** | `GPIO 16` | Hardware interrupt-driven input |
| **Encoder Button (`KEY`)** | `GPIO 33` | Analog input (`analogRead < 500`, long press `≥ 700 ms`) |
| **Auxiliary / Control** | `GPIO 32` | Initialized as `OUTPUT` set to `LOW` |

---

## 🧩 Architecture & Dependencies

The main source file (`src/main.cpp`) is modularized for maintainability and scalability:

### Internal Modules
* `RadioNetwork.h` — Wi-Fi connection management and network requests.
* `HfActivity.h` — Parsing and rendering of HF activity reports and propagation forecasts.
* `FixedText.h` & `Localization.h` — Text dictionaries and UI internationalization.
* `patch_init.h` — Binary patch loader into SI4735 RAM to enable SSB demodulation.

### Required External Libraries
* [`TFT_eSPI`](https://github.com/Bodmer/TFT_eSPI) (Bodmer) — High-speed graphics rendering engine.
* [`SI4735`](https://github.com/pu2clr/SI4735) (PU2CLR) — Main DSP radio chip driver.
* **ESP32 Core Native:** `Preferences`, `Wire`, `esp_heap_caps.h`, `esp_system.h`.

---

## 🕹️ Navigation & Controls

The UI is engineered for one-handed operation via the rotary encoder or direct touch interaction:

### Main Screen (`Radio`)
* **Rotary Encoder:** Dynamically adjusts **Tuning**, **Volume**, or **Fine (BFO)** depending on the active parameter.
* **Button Press (`KEY`):** Toggles between direct parameter adjustment and on-screen button navigation mode.
* **Quick Access Bar (Bottom):** `Settings` • `Seek` • `Memory` • `Info`.

### System Menus
1. **Bands:** Direct selection across all 30 available bands.
2. **Tuning:** Frequency step size and modulation mode configuration.
3. **Reception:** Fine adjustments for `AGC`, `Filter` bandwidth, `AVC`, and `SYNC` detector.
4. **Status:** System diagnostics, free memory (*Heap*), and technical parameters.
5. **Appearance:** HUD style selector, 15 color palettes, and UI language.
6. **Wi-Fi:** Network scanner, on-screen touch keyboard, and solar/HF activity monitor.

---

## 🚀 Installation & Building

1. **Clone the repository:**
   ```bash
   git clone https://github.com/your-username/Radio-KALIMAN-ATS25.git
   cd Radio-KALIMAN-ATS25
   ```
2. **Configure your environment (PlatformIO / Arduino IDE):**
   * Select the **ESP32 Dev Module** board.
   * Ensure the `TFT_eSPI` library's `User_Setup.h` matches your ATS25 board's display and touch pinout.
3. **Compile and upload:**
   * Connect your ATS25 max-Decoder via USB and flash the firmware to the ESP32.

---

## 📜 Credits & Licenses

* **Development & Architecture:** **Radio KALIMAN (`v1.9.0`)** — Created by **KALIMAN / KO6EAI**.
* **Third-Party Acknowledgments:**
  * **PU2CLR (Ricardo Lima Caratti):** For the outstanding `SI4735` Arduino/ESP32 library.
  * **Vadim Afonkin:** For developing the SSB patch for the Si473x chip family.
  * **Bodmer:** For the `TFT_eSPI` and `XPT2046` touch libraries.
  * **Nick O'Leary:** For `PubSubClient` (MIT License).

---

<!-- FOOTER BANNER / BANNER INFERIOR -->
<div align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=0:0f2027,50:203a43,100:2c5364&height=120&section=footer&text=73%20de%20KO6EAI%20%2F%20KALIMAN&fontSize=24&fontColor=00f2fe&fontAlignY=65" alt="Footer Banner" width="100%"/>
</div>
