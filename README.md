# EMA — IoT Air Quality Monitoring System

EMA (Environmental Monitoring Application) is a complete **end-to-end IoT system** designed to monitor and visualize air quality in real-time.  
It integrates low-power sensor nodes, long-range LoRa communication, a cloud backend (ThingsBoard + Node.js server), and a mobile application for visualization and alerts.

---

## 🌍 System Overview

EMA consists of four main components:

### 1️⃣ Sensor Node (Firmware)
- **MCU:** STM32F103C8T6 “Blue Pill”
- **Communication:** LoRa SX1278 @ 433 MHz (SPI)
- **Sensors:**
  - SDS011 – PM2.5 and PM10
  - DHT22 – Temperature and Humidity
  - MQ-7 – CO concentration
  - GUVA-S12SD – UV index
- **Features:**
  - Periodic measurement and data packaging with ACK
  - Power-saving sleep modes
  - Timeout protection and watchdog for sensor communication
  - LoRa packet header: `MSG_TYPE`, `DEVICE_ID`, `GATEWAY_ID`, `ACK`, etc.

---

### 2️⃣ Gateway (ESP32 LilyGo LoRa)
- Receives LoRa packets from sensor nodes
- Validates packet headers and forwards data via **Wi-Fi (MQTT)** to **ThingsBoard Cloud**
- Handles reconnection and local buffering when offline
- Runs under FreeRTOS with dedicated tasks for RX/TX and MQTT management
<img width="1577" height="711" alt="image" src="https://github.com/user-attachments/assets/a7362dcc-889f-46d4-8806-694eebf66e8c" />
<img width="1405" height="359" alt="image" src="https://github.com/user-attachments/assets/742931b0-618e-4b7a-80ce-bc3510bcc338" />


---

### 3️⃣ EMA Server (Node.js)
- Polls device telemetry from **ThingsBoard CE**
- Performs **data validation, reverse geocoding, and anomaly detection**
- Sends **Firebase Cloud Messaging (FCM)** push notifications for air-quality alerts
- Configuration through `.env` file:
  ```env
  FB_PROJECT_ID=...
  SA_PATH=service-account.json
  TB_URL=https://demo.thingsboard.io
  TB_JWT=...
  TB_POLL_SEC=5
  TB_PAGE_SIZE=100
  ```

---

### 4️⃣ Mobile App (React Native)
- Displays real-time and historical environmental data per province
- Visualizes metrics (Temperature, Humidity, CO, UV, PM2.5, PM10) with line and bar charts
- Interactive **map view** using `react-native-maps`
- Receives **push alerts** from EMA server through FCM
- Implements date-range filters (1, 3, 7, 15, 30 days) and province selector

  <img width="1375" height="43" alt="image" src="https://github.com/user-attachments/assets/5bb82d1f-7959-49e3-ae02-755d83ef1e5f" />


---

## 🗂️ Project Structure

```
EMA/
├─ firmware/     # STM32F103C8T6 firmware for LoRa sensor nodes
├─ gateway/      # ESP32 LilyGo LoRa gateway firmware
├─ server/       # Node.js backend for alerting and Firebase push
├─ mobile/       # React Native mobile app for visualization
└─ README.md     # This documentation file
```

---

## ⚙️ Development Requirements

| Component | Toolchain / Environment |
|------------|-------------------------|
| Firmware |  ARM-GCC, st-flash, make|
| Gateway | ESP-IDF |
| Server | Node.js 18+, Firebase Admin SDK |
| Mobile | React Native CLI, Android Studio |

---

## 🚀 Quick Start

### Firmware
1. Open `firmware/` 
2. Adjust LoRa frequency, SF/BW, and sensor pins  
3. Build and flash to Blue Pill boards

### Gateway
1. Configure Wi-Fi and MQTT credentials in source  
2. Build with ESP-IDF 
3. Flash to LilyGo LoRa32 board

### Server
```bash
cd server
npm install
node server.js
```
The server will start polling ThingsBoard and sending notifications.

### Mobile
```bash
cd mobile
npm install
npm run android   
```
Run the app and allow notification permissions.

---

## 🔄 Data Flow

```
[STM32 Sensor Node] --LoRa--> [ESP32 Gateway] --Wi-Fi/MQTT--> [ThingsBoard]
     ↑                                                       ↓
   Sensors (CO, UV, PM, Temp, Humidity)        [EMA Server] --FCM--> [Mobile App]
```

---

## 📈 Planned Improvements
- Add AES-based LoRa payload encryption  
- Calibration database for CO and UV sensors  
- Implement OTA update support for nodes  
- Add caching and offline mode to mobile app  
- Extend ThingsBoard dashboard visualization

---

## 🧑‍💻 Authors
- **Nguyễn Minh Hiếu**  

---

## 📄 License
MIT License — free to use and modify for educational or research purposes.

---
