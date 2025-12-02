# MeowBiu Firmware - Project Structure

## 📋 Overview

MeowBiu is an ESP32-based firmware project for a desk gadget with an animated display. The project follows a service-oriented architecture built on ESP-IDF (Espressif IoT Development Framework) with LVGL for graphics rendering.

## 🏗️ Architecture

```
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
│ UI SERVICE   │ CLOCK SERVICE│ INPUT        │ FACE ENGINE    │
│              │              │ SERVICE      │                │
│ - Screen mgmt│ - Time mgmt  │ - Sensor det.│ - Animations   │
│ - Mode FSM   │ - Display    │ - Debouncing │ - Emotions     │
│ - Rendering  │              │ - Callbacks  │ - Rendering    │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              ▼
┌──────────────┬──────────────┬──────────────┬────────────────┐
│ DISPLAY HAL  │ LVGL         │ FACE ENGINE  │ ESP LCD GC9A01 │
│ (Component)  │ (Component)  │ (Component)  │ (Managed)      │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   HARDWARE ABSTRACTION LAYER                │
│                        (ESP-IDF HAL)                        │
│  - I2C, SPI, GPIO, WiFi, Timers, Display Drivers            │
└─────────────────────────────────────────────────────────────┘
```

## 📁 Directory Structure

```
MeowBiuFirmware/
│
├── 📄 CMakeLists.txt              # Root build configuration
├── 📄 sdkconfig                   # ESP-IDF project configuration
├── 📄 dependencies.lock           # Component manager lock file
├── 📄 README.md                   # Project documentation
│
├── 📂 main/                       # Application entry point
│   ├── CMakeLists.txt            # Main component build config
│   ├── main.c                    # Application startup and orchestration
│   └── idf_component.yml         # Main component dependencies
│
├── 📂 components/                 # Custom components (services & drivers)
│   │
│   ├── 📂 service_manager/       # Core infrastructure layer
│   │   ├── CMakeLists.txt
│   │   ├── service_manager.c     # Service lifecycle management
│   │   ├── event_bus.c           # Inter-service communication
│   │   └── include/              # Public headers
│   │       ├── service_manager.h
│   │       ├── event_bus.h
│   │       └── service_base.h
│   │
│   ├── 📂 ui_service/            # UI management service
│   │   ├── CMakeLists.txt
│   │   ├── ui_service.c          # UI state machine & rendering coordination
│   │   └── include/
│   │       └── ui_service.h
│   │
│   ├── 📂 input_service/         # Input handling service
│   │   ├── CMakeLists.txt
│   │   ├── input_service.c       # Sensor detection & debouncing
│   │   └── include/
│   │       └── input_service.h
│   │
│   ├── 📂 clock/                 # Time management service (placeholder)
│   │   ├── CMakeLists.txt
│   │   ├── clock.c
│   │   └── include/
│   │       └── clock.h
│   │
│   ├── 📂 face/                  # Face/emotion animation engine
│   │   ├── CMakeLists.txt
│   │   ├── face.c                # Animation controller & emotion states
│   │   ├── include/
│   │   │   └── face.h
│   │   └── assets/               # Animation assets (376 files)
│   │       ├── all_animations.h  # Master animation header
│   │       ├── lvgl_img_conv.py  # Image conversion tool
│   │       ├── script.py         # Asset generation script
│   │       │
│   │       ├── 📂 DGAF_loop/     # "Don't Give A F***" emotion loop
│   │       ├── 📂 DGAF_start/    # DGAF emotion start transition
│   │       ├── 📂 angry_loop/    # Angry emotion loop
│   │       ├── 📂 angry_start/   # Angry emotion start transition
│   │       ├── 📂 excited_loop/  # Excited emotion loop
│   │       ├── 📂 excited_start/ # Excited emotion start transition
│   │       ├── 📂 sad_loop/      # Sad emotion loop
│   │       ├── 📂 sad_start/     # Sad emotion start transition
│   │       ├── 📂 scared_loop/   # Scared emotion loop
│   │       ├── 📂 scared_start/  # Scared emotion start transition
│   │       ├── 📂 idle_blink/    # Idle blinking animation
│   │       ├── 📂 idle_look_left_loop/  # Look left loop
│   │       ├── 📂 idle_look_left_start/ # Look left start
│   │       ├── 📂 idle_look_right_loop/ # Look right loop
│   │       └── 📂 idle_look_right_start/# Look right start
│   │
│   ├── 📂 display/               # Display hardware abstraction
│   │   ├── CMakeLists.txt
│   │   ├── display.c             # SPI display driver & LVGL setup
│   │   └── include/
│   │       └── display.h
│   │
│   └── 📂 lvgl/                  # LVGL graphics library (2244 files)
│       └── [LVGL source files]
│
├── 📂 managed_components/         # ESP Component Manager dependencies
│   ├── espressif__cmake_utilities/
│   └── espressif__esp_lcd_gc9a01/ # GC9A01 LCD driver (round display)
│
└── 📂 build/                      # Build artifacts (generated)
```

## 🔧 Components Overview

### Core Infrastructure

#### `service_manager`
- **Purpose**: Orchestrates service lifecycle and inter-service communication
- **Key Features**:
  - Service registration and initialization
  - Dependency injection
  - Service health monitoring
  - Event bus for decoupled communication
- **Files**:
  - `service_manager.c` - Service lifecycle management
  - `event_bus.c` - Publish/subscribe event system

### Services

#### `ui_service`
- **Purpose**: Manages UI state machine and screen rendering
- **Responsibilities**:
  - Screen mode management (state machine)
  - UI transitions
  - LVGL integration and rendering coordination
- **Dependencies**: `display`, `face`, `lvgl`

#### `input_service`
- **Purpose**: Handles sensor input and user interactions
- **Responsibilities**:
  - Sensor event detection
  - Input debouncing
  - Event callbacks to other services
- **Dependencies**: `service_manager`

#### `clock`
- **Purpose**: Time management (placeholder/future development)
- **Status**: Basic structure in place

### Hardware Abstraction & Drivers

#### `display`
- **Purpose**: Low-level display driver and LVGL hardware abstraction
- **Responsibilities**:
  - SPI configuration and communication
  - Display initialization (GC9A01)
  - LVGL driver setup and configuration
  - Framebuffer management
- **Size**: ~11.4 KB

#### `face`
- **Purpose**: Character animation engine with emotion system
- **Features**:
  - Multiple emotion states (DGAF, angry, excited, sad, scared, idle)
  - Smooth transitions between states
  - Frame-based animation playback
  - LVGL image rendering
- **Assets**: 376 animation files organized by emotion and phase (start/loop)
- **Size**: ~10.5 KB + assets

#### `lvgl` (v9.3.0)
- **Purpose**: Graphics library for embedded systems
- **Type**: Vendored third-party library
- **Size**: 2244 files

### Managed Components

External dependencies managed by ESP-IDF Component Manager:

- **`espressif/esp_lcd_gc9a01`**: Hardware-specific LCD driver for GC9A01 round display
- **`espressif/cmake_utilities`**: Build system utilities

## 🎯 Application Flow

### Startup Sequence (`main.c`)

1. **Hardware Initialization**
   ```c
   SPI_Setup()    // Initialize SPI bus for display
   LVGL_Setup()   // Initialize LVGL graphics library
   ```

2. **Infrastructure Initialization**
   ```c
   event_bus_init()       // Set up event bus
   service_manager_init() // Initialize service manager
   ```

3. **Service Registration & Startup**
   ```c
   service_manager_register(&ui_service)
   service_manager_register(&input_service)
   service_manager_start_all()
   ```

4. **Background Tasks**
   - Memory monitoring task (reports LVGL memory every 5s)

## 🔌 Key Interfaces

### Service Interface
All services implement the base service interface:
```c
typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
} service_t;
```

### Event Bus
Decoupled communication between services:
- Services publish events
- Other services subscribe to relevant events
- Asynchronous message passing

## 🛠️ Build System

- **Build Tool**: CMake (via ESP-IDF)
- **Framework**: ESP-IDF (v4.1.0+)
- **Toolchain**: Xtensa GCC (ESP32)
- **Package Manager**: ESP Component Manager

### Key Build Files
- `/CMakeLists.txt` - Root project configuration
- `/main/CMakeLists.txt` - Main component build
- `/components/*/CMakeLists.txt` - Individual component builds
- `sdkconfig` - ESP-IDF configuration (77KB of settings)

## 📦 Dependencies

### External (Managed)
- ESP-IDF framework (>=4.1.0)
- LVGL graphics library
- ESP LCD GC9A01 driver

### Internal
- `service_manager` ← All services depend on this
- `ui_service` ← Depends on `display`, `face`, `lvgl`
- `input_service` ← Depends on `service_manager`
- `face` ← Depends on `lvgl`
- `display` ← Depends on `lvgl`, `esp_lcd_gc9a01`

## 🎨 Animation System

The face engine supports multiple emotions with smooth transitions:

| Emotion  | Start Frames | Loop Frames | Size      |
|----------|-------------|-------------|-----------|
| DGAF     | 31          | 51          | ~500 KB   |
| Angry    | 21          | 37          | ~328 KB   |
| Excited  | 17          | 27          | ~185 KB   |
| Sad      | 36          | 36          | ~173 KB   |
| Scared   | 31          | 33          | ~682 KB   |
| Idle     | N/A         | 21 (blink)  | ~68 KB    |

Each emotion has:
- **Start animation**: Transition into the emotion
- **Loop animation**: Continuous animation for the emotion state
- Individual frames as LVGL-compatible C arrays

## 🔍 Development Tools

Located in `components/face/assets/`:
- `lvgl_img_conv.py` - Converts images to LVGL format
- `script.py` - Batch asset processing and generation
- `script_2.py` - Additional asset utilities

## 📝 Configuration Files

- `.editorconfig` - Editor formatting rules
- `.clangd` - C/C++ language server configuration
- `.vscode/` - VS Code workspace settings
- `.devcontainer/` - Development container configuration
- `sdkconfig` - Complete ESP32 hardware/software configuration

## 🚀 Getting Started

1. **Prerequisites**: ESP-IDF v4.1.0+
2. **Build**: `idf.py build`
3. **Flash**: `idf.py flash`
4. **Monitor**: `idf.py monitor`

## 📊 Project Statistics

- **Total Components**: 7 custom components
- **Animation Assets**: 376 files (~2MB)
- **LVGL Files**: 2244 files
- **Configuration Options**: ~77KB in sdkconfig
- **Architecture**: Service-oriented with event-driven communication

---

*Last Updated: 2025-11-25*
