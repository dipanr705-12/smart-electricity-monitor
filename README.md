# ⚡ Smart Electricity Monitoring System

An IoT-based real-time electricity monitoring system using **ESP32**, **PZEM-004T**, **OLED display**, and **MQTT protocol** for live dashboard and alerts.

## Features
- 📊 Measures **Voltage, Current, Power, Energy (kWh), Frequency, Power Factor**
- 📺 Live readings on a **0.96" OLED display** (3 rotating pages)
- 📡 Publishes data via **MQTT** to Node-RED dashboard
- 🔔 **Alerts** when power or energy exceeds set thresholds
- 💾 Energy counter **persisted in NVS** (survives power cuts)
- 🏠 Compatible with **Home Assistant** via MQTT sensors
- 📝 Python logger saves data to **CSV** with desktop notifications

## Hardware
| Component | Spec |
|-----------|------|
| ESP32 DevKit | 240 MHz, Wi-Fi |
| PZEM-004T v3.0 | AC 80–260 V, 0–100 A |
| OLED Display | 0.96" SSD1306 I²C |
| Logic Level Shifter | 5V ↔ 3.3V |

## Wiring
```
PZEM TX → [Level Shifter] → ESP32 GPIO16 (RX2)
PZEM RX → [Level Shifter] → ESP32 GPIO17 (TX2)
OLED SDA → ESP32 GPIO21
OLED SCL → ESP32 GPIO22
```

## Project Structure
```
smart-electricity-monitor/
├── src/
│   └── smart_electricity_monitor.ino   # ESP32 firmware (Arduino)
├── dashboard/
│   ├── node_red_flow.json              # Node-RED import flow
│   └── mqtt_logger.py                  # Python CSV logger + alerts
└── README.md
```

## Quick Start

### 1. Flash ESP32
- Install libraries: `PZEM004Tv30`, `Adafruit SSD1306`, `PubSubClient`, `ArduinoJson`
- Edit WiFi & MQTT settings in `smart_electricity_monitor.ino`
- Flash via Arduino IDE (Board: ESP32 Dev Module)

### 2. Run MQTT Broker
```bash
mosquitto -v
```

### 3. Import Node-RED Dashboard
```bash
node-red
# Open http://localhost:1880 → Import → node_red_flow.json → Deploy
# Dashboard: http://localhost:1880/ui
```

### 4. Start Python Logger
```bash
pip install paho-mqtt plyer
python dashboard/mqtt_logger.py
```

## MQTT Topics
| Topic | Description |
|-------|-------------|
| `home/energy/data` | Live JSON readings |
| `home/energy/alert` | Threshold alerts |
| `home/energy/status` | Device online/offline (retained) |
| `home/energy/reset` | Publish `RESET` to clear energy counter |

## Alert Thresholds (configurable)
- **Power** > 2000 W → MQTT alert fired
- **Energy** > 10 kWh → MQTT alert fired

> ⚠️ **Safety**: The PZEM-004T connects to 230V AC mains. Have a licensed electrician make the AC wiring connections.

## License
MIT
