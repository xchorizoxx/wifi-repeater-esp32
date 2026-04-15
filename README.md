# 📶 ESP32-S3 WiFi Repeater

A high-performance, OOP-architected WiFi Repeater for the **ESP32-S3**. This project allows bridging a standard WiFi station (STA) to a local Access Point (AP) using **Network Address Port Translation (NAPT)**, effectively extending your WiFi range with a modern web-based management portal.

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/Platform-ESP--IDF%20v5.x-red.svg)
![Language](https://img.shields.io/badge/Language-C++-green.svg)

---

## 🚀 Key Features

- **Transparent Repeater:** Extends existing WiFi ranges using NAT (NAPT).
- **Modern Web UI:** Responsive, dark-themed "Glassmorphism" dashboard for configuration.
- **Smart Connection Monitor:** Automatic reconnection with exponential backoff and connection health watchdog.
- **OTA Updates:** Secure Over-The-Air firmware updates directly through the web interface.
- **Visual Feedback:** Addressable RGB LED (WS2812) indicating system status (Connecting, Active, Error).
- **Fail-safe Mode:** Long-press the BOOT button (3s) to wipe credentials and reset the device.
- **Clean Architecture:** Developed in C++ using a senior-level OOP approach with FreeRTOS integration.

---

## 🛠 Tech Stack

- **Framework:** [ESP-IDF v5.x](https://github.com/espressif/esp-idf)
- **Language:** C++17 (Senior OOP patterns, Singleton, RAII)
- **Core Components:**
  - `WifiManager`: Handles STA and AP lifecycle.
  - `NaptManager`: Manages LwIP NAT tables.
  - `ConfigManager`: Persistent storage using NVS.
  - `WebServer`: Modern HTTP server for management.
  - `StatusLed`: Visual signalling via FreeRTOS task.

---

## 🔌 Hardware Requirements

- **Microcontroller:** ESP32-S3 (e.g., Espressif DevKitC-1).
- **RGB LED:** On-board WS2812 connected to **GPIO 48** (configurable in `main.cpp`).
- **Input:** On-board BOOT button for factory reset (**GPIO 0**).

---

## 📦 Quick Start

### 1. Prerequisites
- Install **ESP-IDF** (v5.1 or later recommended).
- Clone this repository:
  ```bash
  git clone https://github.com/yourusername/wifi-repeater-esp32.git
  cd wifi-repeater-esp32
  ```

### 2. Configure & Build
```bash
idf.py set-target esp32s3
idf.py build
```

### 3. Flash
```bash
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 🌐 Usage

1. **Power on** the ESP32-S3.
2. Connect to the default Access Point: `ESP32-Repeater` (No password by default).
3. Open your browser and navigate to `http://192.168.4.1`.
4. **Scan** for nearby networks, select your home router, enter the password, and click **Save**.
5. The device will reboot and begin repeating the signal.

---

## 📁 Project Structure

```text
.
├── main/
│   ├── include/       # Header files (.hpp)
│   ├── src/           # Implementation files (.cpp)
│   └── main.cpp       # Entry point
├── docs/              # Extended documentation & images
├── CMakeLists.txt     # Build configuration
└── README.md          # You are here
```

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙌 Credits

Developed by **wallmonster**. Inspired by professional networking equipment and modern web design principles.
