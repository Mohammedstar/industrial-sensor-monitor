# 🏭 Industrial Sensor Monitor (ESP32 + Python)

> **Career Stage:** Instrumentation Engineer  
> **Author:** Mohammed Satar | Electrical & Instrumentation Engineer  
> **Hardware:** ESP32 / Arduino | **Software:** Python 3, Flask, Matplotlib

---

## 📋 Overview

A real-time industrial parameter monitoring system that reads **temperature**, **pressure**, **flow rate**, and **level** sensors via ESP32, transmits data over MQTT/Serial, and displays it on a Python-based SCADA-like dashboard with alarm management.

This project simulates a real field instrumentation setup — the kind used in **oil & gas process units**, **refineries**, and **chemical plants**.

---

## 🎯 Features

- ✅ **Multi-sensor acquisition**: Temperature (PT100/DS18B20), Pressure (4-20mA), Flow (pulse counter), Level (ultrasonic)
- ✅ **Real-time dashboard**: Web-based HMI with live charts
- ✅ **Alarm management**: High/High-High and Low/Low-Low alarm limits per ISA-18.2
- ✅ **Data logging**: CSV-based historian with timestamps
- ✅ **Serial/MQTT communication**: Modbus-ready architecture
- ✅ **Email alerts**: SMTP notification on critical alarms

---

## 🔧 Hardware Requirements

| Component | Specification | Role |
|-----------|--------------|------|
| ESP32 | 240MHz, dual-core | Main controller |
| DS18B20 | -55°C to +125°C | Temperature sensor |
| MPX5700 | 0-700 kPa | Pressure sensor |
| HC-SR04 | 2-400cm | Level (ultrasonic) |
| YF-S201 | 1-30 L/min | Flow sensor |
| OLED 0.96" | 128x64, I2C | Local display |
| 4-20mA module | ADS1115 ADC | Analog input |

---

## 📁 Project Structure

```
industrial-sensor-monitor/
├── firmware/
│   ├── esp32_sensor_node.ino     # ESP32 Arduino firmware
│   ├── config.h                  # Pin definitions & calibration
│   └── modbus_rtu.h              # Modbus RTU slave implementation
├── dashboard/
│   ├── app.py                    # Flask web dashboard
│   ├── data_logger.py            # CSV historian
│   ├── alarm_manager.py          # ISA-18.2 alarm logic
│   └── templates/
│       └── index.html            # HMI interface
├── docs/
│   ├── wiring_diagram.png        # Hardware connections
│   └── P&ID_simulation.pdf      # Process flow diagram
├── requirements.txt
└── README.md
```

---

## 🚀 Quick Start

### 1. Flash ESP32 Firmware
```bash
# Open firmware/esp32_sensor_node.ino in Arduino IDE
# Select: Board > ESP32 Dev Module
# Flash to device
```

### 2. Install Python Dependencies
```bash
pip install -r requirements.txt
```

### 3. Run Dashboard
```bash
cd dashboard
python app.py
# Open browser: http://localhost:5000
```

---

## 📊 Dashboard Preview

```
╔══════════════════════════════════════════════════════╗
║  INDUSTRIAL SENSOR MONITOR v1.0                     ║
╠══════════════════════════════════════════════════════╣
║  🌡️  TEMP:    78.4°C   [●●●●●●●○○○] 78%  NORMAL    ║
║  📊  PRESS:   4.2 bar  [●●●●●○○○○○] 56%  NORMAL    ║
║  💧  FLOW:    12.3 L/m [●●●●●●●●○○] 82%  HIGH ⚠️   ║
║  📏  LEVEL:   45.2 cm  [●●●●○○○○○○] 45%  NORMAL    ║
╠══════════════════════════════════════════════════════╣
║  ALARMS: 1 Active | Last: FLOW HIGH @ 14:23:45      ║
╚══════════════════════════════════════════════════════╝
```

---

## 🔌 Communication Protocol

- **ESP32 → PC**: Serial (115200 baud) / MQTT (WiFi)
- **Data format**: JSON `{"temp": 78.4, "press": 4.2, "flow": 12.3, "level": 45.2}`
- **Alarm standard**: ISA-18.2 (4-state: Normal → Alert → Warning → Alarm)

---

## 📜 License

MIT License — Mohammed Satar, 2024  
🔗 [LinkedIn](https://www.linkedin.com/in/mohammed-satar-b13266214)
