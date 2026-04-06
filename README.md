⚡ CheCore v20.0 — ESP32 Wireless Security Engine

«Scan. Simulate. Analyze. Defend.
A real-time ESP32-based platform for wireless research, testing, and defensive security workflows.»

---

🧠 What is CheCore?

CheCore is a high-performance firmware designed to push the ESP32 to its limits for:

- 📡 Wireless environment visibility
- 📶 Protocol behavior simulation (lab use)
- 🔵 BLE signal experimentation
- 🪪 RFID interaction and logging
- 🌐 On-device control via web interface

Built with FreeRTOS + low-level WiFi control, optimized for stability, concurrency, and long runtime.

---

⚠️ Read Before Use

This tool is for:

- ✅ Authorized testing
- ✅ Security research
- ✅ Controlled lab environments

Never use on networks without permission.
You are responsible for how you use this tool.

---

🔥 Core Capabilities

📡 WiFi Recon Engine

- Fast network scanning with:
  - SSID / BSSID
  - RSSI
  - Channel
  - Encryption type
- Snapshot system for stable data handling
- Non-blocking scan architecture

---

📶 Beacon Simulation (Research Mode)

- Custom SSID broadcasting
- Randomized identity pool
- Controlled transmission rate
- Useful for:
  - Network behavior analysis
  - Client response testing

---

⚡ Frame Injection Engine (Lab Only)

- Structured 802.11 frame transmission
- Channel-aware execution
- Sequence handling for realism
- Designed for controlled experiments

---

🔵 BLE Signal Module

- Continuous advertising (no instability)
- Dynamic payload mutation
- Multiple signal profiles
- Safe update system (no stack crashes)

---

🪪 RFID System (MFRC522)

- UID detection + logging
- Card type identification
- Duplicate filtering
- Timestamped records

---

🌐 Web Control Interface

- Hosted directly on ESP32
- Real-time control panel
- JSON API endpoints
- Captive portal behavior

---

🛡️ Stability Engineering (What makes CheCore different)

This is NOT a basic script tool.

✔ FreeRTOS Task Architecture

- Dedicated tasks for each subsystem
- Non-blocking, concurrent execution

✔ Radio Ownership Layer

- Prevents WiFi/BLE conflicts
- Controlled access to low-level radio

✔ Fail-Safe System

- Heap monitoring
- Auto recovery from stuck states
- Scan timeout protection
- Mode reset system

✔ Watchdog Safe

- No freeze loops
- All tasks monitored

---

⚙️ Hardware

- ESP32 Dev Board
- MFRC522 RFID Module

---

🔌 Wiring (RC522 → ESP32)

RC522| ESP32
SDA| GPIO 5
SCK| GPIO 18
MOSI| GPIO 23
MISO| GPIO 19
RST| GPIO 4

---

🚀 Getting Started

ANDROID 
1. INSTALL ESP32 FLASHER APP FROM ANY APP STORE 
2. SELECT THE CORRECT OFFSET FILES
     |
     |
     ————–
           Bootloader-0x1000
      Partition Table-0x8000
         App Firmware-0x10000
3. flash the esp32
(in some esp32 we need to hold boot button till flashing complete)

---

🌐 Access Panel

- SSID: CheCore
- Password: checore1
- URL: http://192.168.4.1

---

🧩 System Design

Core Engine
├── WiFi Scanner
├── Beacon Module
├── Frame Engine
├── BLE Module
├── RFID System
└── Failsafe Controller

---

📊 Performance

- ⚡ Multi-tasking with FreeRTOS
- 🔋 Efficient CPU usage
- 🧠 Memory-safe design
- ⏱ Stable long runtime (days)

---

🧪 Use Cases

- Wireless lab experiments
- Embedded systems learning
- Security research (authorized)
- Protocol analysis

---

🚧 Roadmap

- OLED display integration
- Secure login system
- Advanced logging
- Mobile control app

---

👤 Author

Lesslie Newbigin
ESP32 | Embedded Systems | Wireless Research

---

⭐ Support

If this project helps you:

- ⭐ Star the repo
- 🔧 Contribute
- 📢 Share

---

🧠 Philosophy

«Precision over noise
Control over chaos
Stability over hacks»

---

⚡ CheCore

Built to explore. Designed to stay stable.# lesslie-newbigin.github.io
