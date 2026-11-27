# ⚡ Smart Electricity Monitoring System

An IoT-based real-time electricity monitoring system using **ESP32**, **PZEM-004T v3.0**, **0.96" I²C OLED Display**, and **MQTT** for live telemetry, automated threshold alerts, and cloud/local dashboards. Includes a complete **Node-RED dashboard**, a **Python CSV logger** with desktop notifications, and a full **Wokwi hardware simulation**.

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Sensor: PZEM-004T](https://img.shields.io/badge/Sensor-PZEM--004T%20v3.0-orange.svg)](https://innovatorsguru.com/pzem-004t-v3/)
[![Protocol: MQTT](https://img.shields.io/badge/Protocol-MQTT-purple.svg)](https://mqtt.org/)
[![Simulation: Wokwi](https://img.shields.io/badge/Simulation-Wokwi-brightgreen.svg)](https://wokwi.com/projects/481764224060237825)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 📑 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [System Architecture](#-system-architecture)
- [Hardware Components & Bill of Materials](#-hardware-components--bill-of-materials)
- [Wiring & Pinout](#-wiring--pinout)
- [Repository Structure](#-repository-structure)
- [Getting Started](#-getting-started)
  - [1. Arduino Firmware Setup](#1-arduino-firmware-setup)
  - [2. MQTT Broker Setup](#2-mqtt-broker-setup)
  - [3. Node-RED Dashboard](#3-node-red-dashboard)
  - [4. Python Telemetry Logger](#4-python-telemetry-logger)
- [Wokwi Online Simulation](#-wokwi-online-simulation)
- [MQTT Topics & Payload Schema](#-mqtt-topics--payload-schema)
- [OLED Multi-Page Interface](#-oled-multi-page-interface)
- [Safety Guidelines](#-safety-guidelines)
- [Contributing](#-contributing)
- [License](#-license)

---

## 🔍 Overview

The **Smart Electricity Monitor** accurately captures mains line parameters (voltage, current, active power, total energy consumption, frequency, and power factor) from an electrical panel or AC appliance using the industrial-grade PZEM-004T v3.0 sensor.

The ESP32 processes sensor data, renders rotating metric screens on an SSD1306 OLED display, persists energy usage to Non-Volatile Storage (NVS) to survive blackouts, and streams JSON telemetry over Wi-Fi via MQTT.

---

## ✨ Key Features

- **High-Precision Multi-Metric Measurement**:
  - Voltage (80–260 V AC)
  - Current (0–100 A)
  - Active Power (0–23 kW)
  - Accumulated Energy (0–9999 kWh)
  - Line Frequency (45–65 Hz)
  - Power Factor (0.00–1.00)
- **OLED UI (3 Rotating Pages)**: Cycles between Voltage/Current, Power/Energy & Cost, and Line Quality/Network status.
- **Power Failure Immunity**: Cumulative energy (kWh) is regularly saved to ESP32 Flash NVS (`Preferences.h`), preventing data loss during power outages.
- **Bi-Directional MQTT Control**:
  - Publishes telemetry every 5 seconds to `home/energy/data`.
  - Publishes instantaneous over-power and energy breach warnings to `home/energy/alert`.
  - Remote counter clearing via `home/energy/reset`.
- **Node-RED UI**: Pre-built dashboard flow (`node_red_flow.json`) with interactive radial gauges, consumption trend charts, and cost indicators.
- **Python Data Logger**: Desktop background daemon (`mqtt_logger.py`) that logs readings to daily CSV files and triggers OS desktop notifications upon threshold violations.
- **Full Wokwi Simulation**: Test the OLED pages, potentiometer load variation, alert LEDs, and reset buttons directly in your browser without physical hardware.

---

## 🏗️ System Architecture

```
                  ┌────────────────────────────────────────┐
                  │          230V AC Mains / Load          │
                  └───────────────────┬────────────────────┘
                                      │ Current Transformer (CT)
                                      ▼
                             ┌─────────────────┐
                             │  PZEM-004T v3.0 │
                             └────────┬────────┘
                                      │ UART (5V TTL)
                          ┌───────────▼───────────┐
                          │ Logic Level Converter │ (5V ↔ 3.3V)
                          └───────────┬───────────┘
                                      │ Serial (GPIO16 / GPIO17)
                                      ▼
                             ┌─────────────────┐
                             │   ESP32 DevKit  │
                             └────┬───────┬────┘
               I²C (GPIO21/22)    │       │   Wi-Fi (MQTT)
         ┌────────────────────────┘       └────────────────────────┐
         ▼                                                         ▼
┌──────────────────┐                                      ┌──────────────────┐
│ 0.96" SSD1306    │                                      │   MQTT Broker    │
│ OLED Display     │                                      │ (Mosquitto/EMQX) │
└──────────────────┘                                      └────────┬─────────┘
                                                                   │
                                                ┌──────────────────┴──────────────────┐
                                                ▼                                     ▼
                                      ┌───────────────────┐                 ┌───────────────────┐
                                      │ Node-RED UI       │                 │ Python CSV Logger │
                                      │ (Gauges & Trends) │                 │ + Desktop Alerts  │
                                      └───────────────────┘                 └───────────────────┘
```

---

## 🧰 Hardware Components & Bill of Materials

| Component | Quantity | Specification / Description |
|-----------|----------|-----------------------------|
| **ESP32 DevKit V1** | 1 | 30-pin or 36-pin dual-core MCU with Wi-Fi & Bluetooth |
| **PZEM-004T v3.0** | 1 | AC digital multimeter module with split-core or solid CT coil |
| **SSD1306 OLED Display** | 1 | 0.96 inch, 128×64 resolution, I²C interface |
| **Logic Level Converter** | 1 | 4-channel bidirectional 5V ↔ 3.3V shifter (for UART lines) |
| **Status & Alert LEDs** | 2 | Green (Heartbeat / Wi-Fi) & Red (Over-power alert) |
| **Current Limiting Resistors** | 2 | 220 Ω (for external LEDs) |
| **External Push Buttons** | 2 | Momentary tactile buttons (Manual page toggle & Energy reset) |
| **Power Supply** | 1 | 5V 2A micro-USB or AC-DC stepdown module (e.g., HLK-PM01) |

---

## 🔌 Wiring & Pinout

### 1. PZEM-004T v3.0 to ESP32 (via Level Shifter)
> **Note**: The PZEM-004T UART logic operates at **5V**. Connecting PZEM TX directly to ESP32 RX without level conversion can degrade or damage 3.3V ESP32 GPIOs over time.

| PZEM-004T Pin | Level Converter | ESP32 GPIO | Description |
|---|:---:|:---:|---|
| **5V (VCC)** | HV | — | 5V Power from supply |
| **GND** | GND | GND | Common ground |
| **TX** | HV1 ➔ LV1 | **GPIO 16 (RX2)** | PZEM Serial Output |
| **RX** | HV2 ⬅ LV2 | **GPIO 17 (TX2)** | PZEM Serial Input |

### 2. 0.96" I²C OLED Display (SSD1306)
| OLED Pin | ESP32 GPIO | Notes |
|---|:---:|---|
| **VCC** | **3.3V** | Power rail |
| **GND** | **GND** | Ground reference |
| **SDA** | **GPIO 21** | I²C Serial Data |
| **SCL** | **GPIO 22** | I²C Serial Clock |

---

## 📂 Repository Structure

```
smart-electricity-monitor/
├── src/
│   └── smart_electricity_monitor.ino   # Complete ESP32 production firmware
├── dashboard/
│   ├── node_red_flow.json              # Node-RED dashboard workflow import
│   └── mqtt_logger.py                  # Python MQTT subscriber, CSV logger & notifications
├── docs/
│   └── hardware-prototype.html         # Interactive schematic & wiring browser widget
├── wokwi/
│   ├── diagram.json                    # Wokwi simulation schematic and circuit layout
│   ├── sketch.ino                      # Wokwi simulation firmware (potentiometer load)
│   ├── libraries.txt                   # Arduino dependencies for simulation
│   └── wokwi.toml                      # Wokwi workspace configuration
├── README.md                           # Project documentation
└── WIKI.md                             # Hardware guide and simulation notes
```

---

## 🚀 Getting Started

### 1. Arduino Firmware Setup

1. Open the [Arduino IDE](https://www.arduino.cc/en/software) (version 2.0+ recommended).
2. Install the **ESP32 Board Package**:
   - Go to **File → Preferences**
   - Add to *Additional Board Manager URLs*:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - In **Tools → Board → Boards Manager**, search for `esp32` and click **Install**.
3. Install required libraries via **Sketch → Include Library → Manage Libraries**:
   - `PZEM004Tv30` by Oleh Siryk
   - `Adafruit SSD1306` by Adafruit
   - `Adafruit GFX Library` by Adafruit
   - `PubSubClient` by Nick O'Leary
   - `ArduinoJson` by Benoît Blanchon (v6 or v7)
4. Open [`src/smart_electricity_monitor.ino`](src/smart_electricity_monitor.ino).
5. Edit your Wi-Fi credentials and MQTT broker IP:
   ```cpp
   #define WIFI_SSID       "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
   #define MQTT_BROKER     "192.168.1.100"      // Your MQTT broker IP
   #define MQTT_PORT       1883
   ```
6. Connect the ESP32 via USB, select **ESP32 Dev Module**, choose the correct COM port, and click **Upload**.

---

### 2. MQTT Broker Setup

You can run an MQTT broker locally using Eclipse Mosquitto:

```bash
# Ubuntu / Debian
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable --now mosquitto

# Windows / macOS
# Download installer from https://mosquitto.org/download/ or install via Homebrew:
brew install mosquitto
mosquitto -v
```

---

### 3. Node-RED Dashboard

1. Start Node-RED:
   ```bash
   node-red
   ```
2. Open your browser at `http://localhost:1880`.
3. Click the top-right menu ➔ **Import** ➔ select [`dashboard/node_red_flow.json`](dashboard/node_red_flow.json).
4. Double-click the MQTT configuration node to confirm the broker IP (`localhost:1883`).
5. Click **Deploy**.
6. Access your dashboard at: **`http://localhost:1880/ui`**

---

### 4. Python Telemetry Logger

The Python daemon saves energy logs to CSV and triggers desktop alerts:

```bash
# 1. Install dependencies
pip install paho-mqtt plyer

# 2. Run logger
python dashboard/mqtt_logger.py
```

The script automatically creates daily timestamped log files under `dashboard/logs/energy_YYYYMMDD.csv` with fields:
`timestamp, voltage, current, power, energy_kwh, frequency, power_factor, est_cost_inr`.

---

## 🌐 Wokwi Online Simulation

You can test and modify the project directly in your browser without hardware:

[![Try on Wokwi](https://img.shields.io/badge/▶_Run_Live_Simulation-Wokwi-25c2a0?style=for-the-badge&logo=wokwi)](https://wokwi.com/projects/481764224060237825)

👉 **[Launch Smart Electricity Monitor on Wokwi](https://wokwi.com/projects/481764224060237825)**

### How to use the simulation:
2. Copy the contents of [`wokwi/sketch.ino`](wokwi/sketch.ino) into `sketch.ino`.
3. Copy [`wokwi/diagram.json`](wokwi/diagram.json) into `diagram.json`.
4. In the Library Manager tab, add the libraries from [`wokwi/libraries.txt`](wokwi/libraries.txt).
5. Click **Start Simulation**:
   - Move the **Slide Potentiometer** to simulate varying appliance power loads (0–5000 W).
   - Press the **Green Button** to manually switch between OLED display pages.
   - Press the **Blue Button** to reset accumulated energy.
   - The **Red LED** lights up when power exceeds 2000 W.

---

## 📡 MQTT Topics & Payload Schema

### Published Topics

#### 1. `home/energy/data` (Telemetry payload, published every 5s)
```json
{
  "voltage": 232.4,
  "current": 3.821,
  "power": 888.0,
  "energy_kwh": 4.1205,
  "frequency": 50.0,
  "power_factor": 0.98,
  "timestamp": 1284,
  "est_cost_inr": 32.96
}
```

#### 2. `home/energy/alert` (Fired upon threshold violations)
```json
{
  "type": "power",
  "msg": "Power exceeded 2000 W",
  "value": 2450.3
}
```

#### 3. `home/energy/status` (Retained LWT - Last Will & Testament)
```json
{
  "online": true
}
```

### Subscribed Command Topics
- **`home/energy/reset`**: Send payload `RESET` to clear the PZEM cumulative energy registers and zero out NVS storage.

---

## 📺 OLED Multi-Page Interface

The onboard 0.96" SSD1306 OLED automatically cycles through three diagnostic pages every 3 seconds:

| Page 1: Primary Metrics | Page 2: Power & Billing | Page 3: Line Quality & Network |
|:---:|:---:|:---:|
| **Voltage & Current**<br>`232.1V`<br>`3.840A` | **Active Power & Energy**<br>`891.2W`<br>Energy: `4.1205 kWh`<br>Cost: `Rs 32.96` | **Frequency, PF & IP**<br>Freq: `50.0 Hz` \| PF: `0.98`<br>WiFi: `OK`<br>IP: `192.168.1.105` |

---

## ⚠️ Safety Guidelines

> **DANGER: HIGH VOLTAGE RISK**
> - The PZEM-004T high-voltage terminals connect directly to **230V AC mains electricity**.
> - Never touch exposed terminals or sensor circuit boards while connected to AC power.
> - Always house the PZEM-004T and power supply inside an **insulated, flame-retardant project box**.
> - Always test new firmware changes using the [Wokwi simulation](wokwi/) or low-voltage DC before deploying on live mains wiring.
> - If you are unfamiliar with domestic AC wiring regulations, consult a certified electrician.

---

## 🤝 Contributing

Contributions are welcomed! Feel free to fork the repository and submit a pull request:
1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/NewFeature`)
3. Commit your Changes (`git commit -m 'Add NewFeature'`)
4. Push to the Branch (`git push origin feature/NewFeature`)
5. Open a Pull Request

---

## 📄 License

This project is licensed under the **MIT License**. Feel free to use, modify, and distribute it for personal and educational projects.
