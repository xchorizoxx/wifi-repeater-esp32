# 📶 Repetidor WiFi ESP32-S3

Un Repetidor WiFi de alto rendimiento con arquitectura OOP para el **ESP32-S3**. Este proyecto permite puentear una estación WiFi estándar (STA) a un Punto de Acceso local (AP) utilizando **NAPT (NAT)**, extendiendo efectivamente el rango de tu red WiFi con un portal de gestión web moderno.

![Licencia](https://img.shields.io/badge/Licencia-MIT-blue.svg)
![Plataforma](https://img.shields.io/badge/Plataforma-ESP--IDF%20v5.x-red.svg)
![Lenguaje](https://img.shields.io/badge/Lenguaje-C++-green.svg)

---

## 🚀 Características Principales

- **Repetidor Transparente:** Extiende el rango de WiFi existente usando NAT (NAPT).
- **Web UI Moderna:** Panel de control responsivo con estilo "Glassmorphism" para configuración.
- **Monitor de Conexión Inteligente:** Reconexión automática con backoff exponencial y watchdog de salud de red.
- **Actualizaciones OTA:** Actualización segura de firmware Over-The-Air directamente desde la interfaz web.
- **Feedback Visual:** LED RGB direccionable (WS2812) que indica el estado del sistema (Conectando, Activo, Error).
- **Modo Fail-safe:** Mantén presionado el botón BOOT (3s) para borrar credenciales y resetear el dispositivo.
- **Arquitectura Limpia:** Desarrollado en C++ siguiendo patrones OOP de nivel senior con integración de FreeRTOS.

---

## 🛠 Stack Tecnológico

- **Framework:** [ESP-IDF v5.x](https://github.com/espressif/esp-idf)
- **Lenguaje:** C++17 (Patrones OOP Senior, Singleton, RAII)
- **Componentes Core:**
  - `WifiManager`: Gestiona el ciclo de vida de STA y AP.
  - `NaptManager`: Gestiona las tablas NAT de LwIP.
  - `ConfigManager`: Almacenamiento persistente mediante NVS.
  - `WebServer`: Servidor HTTP moderno para la gestión.
  - `StatusLed`: Señalización visual mediante tareas de FreeRTOS.

---

## 🔌 Requisitos de Hardware

- **Microcontrolador:** ESP32-S3 (ej. Espressif DevKitC-1).
- **LED RGB:** Incorporado WS2812 conectado al **GPIO 48** (configurable en `main.cpp`).
- **Botón:** Botón BOOT integrado para reset de fábrica (**GPIO 0**).

---

## 📦 Inicio Rápido

### 1. Prerrequisitos
- Instalar **ESP-IDF** (v5.1 o posterior recomendado).
- Clonar este repositorio:
  ```bash
  git clone https://github.com/yourusername/wifi-repeater-esp32.git
  cd wifi-repeater-esp32
  ```

### 2. Configurar y Compilar
```bash
idf.py set-target esp32s3
idf.py build
```

### 3. Flashear
```bash
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 🌐 Uso

1. **Enciende** el ESP32-S3.
2. Conéctate al Punto de Acceso predeterminado: `ESP32-Repeater` (Sin contraseña por defecto).
3. Abre tu navegador y navega a `http://192.168.4.1`.
4. **Escanea** redes cercanas, selecciona tu router, introduce la contraseña y pulsa **Guardar**.
5. El dispositivo se reiniciará y comenzará a repetir la señal.

---

## 📁 Estructura del Proyecto

```text
.
├── main/
│   ├── include/       # Archivos de cabecera (.hpp)
│   ├── src/           # Archivos de implementación (.cpp)
│   └── main.cpp       # Punto de entrada
├── docs/              # Documentación extendida e imágenes
├── CMakeLists.txt     # Configuración de compilación
└── README.md          # Estás aquí
```

---

## 📜 Licencia

Este proyecto está bajo la Licencia MIT - mira el archivo [LICENSE](LICENSE) para más detalles.

---

## 🙌 Créditos

Desarrollado por **wallmonster**. Inspirado en equipos de red profesionales y principios de diseño web moderno.
