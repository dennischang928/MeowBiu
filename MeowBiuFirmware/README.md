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



## 🔒 Thread Safety and LVGL

### What is Thread Safety?

**Thread safety** means that code can be safely used by multiple threads simultaneously without causing race conditions, data corruption, or unexpected behavior. When multiple threads access shared resources (like memory, variables, or hardware) at the same time, conflicts can occur if proper synchronization isn't used.

### Why LVGL Requires Locking

**LVGL (Light and Versatile Graphics Library) is NOT thread-safe by default.** This means:

- Only **one thread at a time** should access LVGL functions and objects
- Without proper locking, multiple threads can corrupt LVGL's internal state
- Concurrent access can cause infinite loops, crashes, or watchdog timeouts

In this firmware, we have multiple FreeRTOS tasks that need to interact with LVGL:
- **`lvgl_task`**: Main LVGL task that handles rendering and timer events
- **`clock_ui_task`**: Updates clock display elements
- **`behavior_task`**: Controls face animations

### The Problem Without Locking

```c
// ❌ WRONG - This causes crashes and watchdog timeouts!
void clock_ui_task(void *pvParameters) {
    while (1) {
        clock_ui_set_time(2024, 1, 1, 1);  // Directly calling LVGL functions
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**What happens:**
1. `clock_ui_task` calls `lv_label_set_text()` to update the clock
2. At the same time, `lvgl_task` is calling `lv_timer_handler()` to render the screen
3. Both tasks corrupt LVGL's internal data structures
4. LVGL gets stuck in an infinite loop in `lv_inv_area()`
5. The IDLE task can't run, triggering the **watchdog timer**
6. System crashes with: `E (40805) task_wdt: Task watchdog got triggered`

### The Solution: LVGL Locking

ESP-IDF's LVGL port provides `lv_lock()` and `lv_unlock()` functions that use a **mutex** (mutual exclusion) to ensure only one task accesses LVGL at a time.

```c
// ✅ CORRECT - Safe multi-threaded LVGL access
void clock_ui_task(void *pvParameters) {
    // Initialize with lock
    lv_lock();
    clock_ui_layout_init();
    lv_unlock();
    
    while (1) {
        // Lock before any LVGL call
        lv_lock();
        clock_ui_set_time(2024, 1, 1, 1);
        lv_unlock();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### How Locking Works

1. **`lv_lock()`**: Acquires the LVGL mutex
   - If another task already holds the lock, this task **waits** (blocks)
   - When the lock is available, this task acquires it and proceeds
   
2. **Protected Section**: Code between `lv_lock()` and `lv_unlock()`
   - Only this task can access LVGL
   - Other tasks must wait
   
3. **`lv_unlock()`**: Releases the LVGL mutex
   - Allows other waiting tasks to acquire the lock
   - Always unlock as soon as possible to avoid blocking other tasks

### Best Practices

#### ✅ DO:
- **Always** use `lv_lock()` / `lv_unlock()` when calling LVGL functions from non-LVGL tasks
- Keep locked sections **as short as possible** to minimize blocking
- Lock around the entire sequence of related LVGL calls
- Verify all LVGL object creation and updates are protected

```c
lv_lock();
lv_obj_t *label = lv_label_create(parent);
lv_label_set_text(label, "Hello");
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
lv_unlock();
```

#### ❌ DON'T:
- Call LVGL functions without locking from non-LVGL tasks
- Hold locks for long periods (e.g., during delays or blocking operations)
- Lock/unlock inside tight loops unnecessarily
- Forget to unlock (causes deadlock)

```c
// ❌ BAD - Lock held during delay
lv_lock();
lv_label_set_text(label, "Update");
vTaskDelay(pdMS_TO_TICKS(1000));  // Other tasks blocked for 1 second!
lv_unlock();

// ✅ GOOD - Unlock before delay
lv_lock();
lv_label_set_text(label, "Update");
lv_unlock();
vTaskDelay(pdMS_TO_TICKS(1000));  // Other tasks can access LVGL
```

### Common Pitfalls

1. **Forgetting to lock in utility functions**
   ```c
   void clock_ui_set_time(int year, int month, int day, int weekday) {
       // These LVGL calls need locking if called from non-LVGL tasks!
       lv_label_set_text(date_label, date_str);
       lv_label_set_text(day_label, days[weekday]);
   }
   ```
   
   **Solution**: Either lock inside the function OR document that callers must lock

2. **Nested locking** (usually safe in ESP-IDF, but avoid if possible)
   ```c
   lv_lock();
   my_function();  // Also calls lv_lock() internally
   lv_unlock();
   ```

3. **Deadlock from missing unlock**
   ```c
   lv_lock();
   if (error) {
       return;  // ❌ Forgot to unlock!
   }
   lv_unlock();
   ```

### Debugging Thread Issues

If you see:
- **Watchdog timeouts**: Usually caused by infinite loops from race conditions
- **Task hangs**: Check if you forgot to `lv_unlock()`
- **Display corruption**: Missing locks around LVGL calls
- **Crashes in `lv_inv_area()`**: Multiple tasks accessing LVGL simultaneously

**Enable backtrace for better debugging:**
```
idf.py menuconfig
→ Component config → ESP System Settings
→ Enable CONFIG_ESP_SYSTEM_USE_FRAME_POINTER
```

### References

- [LVGL Thread Safety Documentation](https://docs.lvgl.io/master/porting/os.html)
- [FreeRTOS Mutex Documentation](https://www.freertos.org/a00113.html)
- [ESP-IDF Thread Safety Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/thread-local-storage.html)


#