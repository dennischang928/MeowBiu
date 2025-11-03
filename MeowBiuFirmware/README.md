# Outline
┌─────────────────────────────────────────────────────────────┐
│                        APPLICATION LAYER                    │
│                          (main.c)                           │
│  - Startup/shutdown orchestration                           │
│  - Service registration and lifecycle management            │
│  - High-level business logic only                           │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     SERVICE MANAGER LAYER                   │
│                    (service_manager)                        │
│  - Dependency injection                                     │
│  - Inter-service communication (event bus)                  │
│  - Service health monitoring                                │
└─────────────────────────────────────────────────────────────┘
                              ▼
┌──────────────┬──────────────┬──────────────┬────────────────┐
│ UI SERVICE   │ TIME SERVICE │ SENSOR       │ NETWORK        │
│              │              │ SERVICE      │ SERVICE        │
│ - Screen mgmt│ - RTC mgmt   │ - Shock det. │ - WiFi/BLE     │
│ - Mode FSM   │ - Timezone   │ - Debouncing │ - API calls    │
│ - Rendering  │ - Alarms     │ - Callbacks  │ - OTA          │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              ▼
┌──────────────┬──────────────┬──────────────┬────────────────┐
│ FACE ENGINE  │ LVGL HAL     │ DS3231       │ HTTP CLIENT    │
│ (Component)  │ (Component)  │ DRIVER       │ (Component)    │
│              │              │ (Component)  │                │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   HARDWARE ABSTRACTION LAYER                │
│                        (ESP-IDF HAL)                        │
│  - I2C, SPI, GPIO, WiFi, Timers                             │
└─────────────────────────────────────────────────────────────┘



## 📁 Complete Directory Structure
```
desk-gadget/
├── CMakeLists.txt
├── sdkconfig
├── partitions.csv
│
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                          # Minimal startup only
│   └── Kconfig.projbuild
│
├── components/
│   │
│   ├── service_manager/                # Core infrastructure
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── service_manager.h
│   │   │   ├── event_bus.h
│   │   │   └── service_base.h
│   │   ├── service_manager.c
│   │   ├── event_bus.c
│   │   └── README.md
│   │
│   ├── ui_service/                     # UI as a service
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── ui_service.h
│   │   ├── ui_service.c
│   │   ├── ui_state_machine.c
│   │   ├── ui_screens.c
│   │   └── ui_transitions.c
│   │
│   ├── time_service/                   # Time management service
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── time_service.h
│   │   ├── time_service.c
│   │   └── timezone.c
│   │
│   ├── sensor_service/                 # Sensor input service
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── sensor_service.h
│   │   └── sensor_service.c
│   │
│   ├── network_service/                # WiFi/Network service
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── network_service.h
│   │   ├── network_service.c
│   │   ├── wifi_manager.c
│   │   └── api_client.c
│   │
│   ├── face/                          # Face engine (your existing)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── face.h
│   │   ├── face.c
│   │   └── assets/
│   │
│   ├── display_hal/                   # Display hardware abstraction
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── display_hal.h
│   │   ├── display_hal.c
│   │   └── gc9a01_driver.c
│   │
│   └── rtc_hal/                       # RTC hardware abstraction
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── rtc_hal.h
│       └── ds3231_driver.c
│
├── tools/
│   ├── generate_emotion.py
│   └── deploy.sh
│
└── docs/
    ├── architecture.md
    └── service_api.md



#