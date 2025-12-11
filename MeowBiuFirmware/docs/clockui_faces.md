## Clock UI Faces — quick pattern

Goal: add/switch faces without touching unrelated code.

### Layout (fits your current `components/clock_ui`)
- Keep `clock_ui.c` as the controller.
- Add faces under `components/clock_ui/clock_faces/`:
  - `face_old_fashioned/face_old_fashioned.c/.h`
  - `face_digital/face_digital.c/.h`
  - `face_<new>/...`
  - `face_registry.c/.h`

### Where code lives (relative to what you have)
- Interface typedef: `components/clock_ui/clock_faces/face_registry.h`.
- Registry table + helpers: `components/clock_ui/clock_faces/face_registry.c`.
- Each face’s code: `components/clock_ui/clock_faces/face_<id>/face_<id>.c/.h`.
- Assets (fonts/images): alongside that face in `face_<id>/`.
- `clock_ui.c`: stays the controller; it includes `face_registry.h`, calls `clock_face_count()`, `clock_face_at()`, and `switch_to()` to display.

### Mapping to your current code (`clock_ui.c`)
- Your current enum `clock_face_t` (text vs old_fashioned) can move to `face_registry.h` as the face descriptor struct.
- The existing layout code in `clock_ui.c` can be split:
  - Move the text layout into `face_digital/face_digital.c` (implement `init/show/hide/destroy` there).
  - Move the analog layout into `face_old_fashioned/face_old_fashioned.c`.
  - Keep the shared state/time update helpers (e.g., `clock_ui_set_time/date`, locking, container) in `clock_ui.c` or a small shared helper file if both faces need it.
- `clock_ui.c` then just:
  1) Initializes LVGL container.
  2) Uses the registry to select a face (by id or index).
  3) Calls `switch_to(face)` to hide current and show the next.

### Common interface (per face)
```c
typedef struct {
  const char *id;          // "old_fashioned", "digital", ...
  const char *name;        // shown in menu
  void (*init)(void);      // build LVGL objects (hidden)
  void (*show)(void);      // make visible, start timers
  void (*hide)(void);      // hide, stop timers
  void (*destroy)(void);   // free if needed
} clock_face_t;

const clock_face_t *clock_face_get_<id>(void);
```

### Lifecycle Functions: init / show / hide / destroy

These four function pointers manage the complete lifecycle of a clock face:

#### `init()` — One-time setup (hidden)
**When called:** Once when the face is first registered/loaded, before it's shown  
**Purpose:** Create all LVGL UI objects (labels, arcs, images, etc.) but keep them hidden  
**Why hidden:** Allows pre-building all faces at startup for instant switching later  
**Example:**
```c
static lv_obj_t *hour_hand, *minute_hand, *face_bg;

static void init_old_fashioned(void) {
  // Create LVGL objects but don't show them yet
  face_bg = lv_obj_create(parent_container);
  lv_obj_add_flag(face_bg, LV_OBJ_FLAG_HIDDEN);
  
  hour_hand = lv_arc_create(face_bg);
  minute_hand = lv_arc_create(face_bg);
  // ... create all UI elements
}
```

#### `show()` — Make visible and start updates
**When called:** Every time this face becomes active/visible  
**Purpose:** 
- Make LVGL objects visible (`lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN)`)
- Start any timers for animations/updates
- Refresh the display with current time
**Why separate from init:** Allows switching between faces without recreating objects  
**Example:**
```c
static void show_old_fashioned(void) {
  // Make visible
  lv_obj_clear_flag(face_bg, LV_OBJ_FLAG_HIDDEN);
  
  // Start update timer (e.g., every second)
  lv_timer_create(update_analog_clock, 1000, NULL);
  
  // Update immediately with current time
  update_clock_display();
}
```

#### `hide()` — Hide and stop updates
**When called:** Every time this face becomes inactive (user switches away)  
**Purpose:**
- Hide LVGL objects (`lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)`)
- Stop/delete timers to save CPU and battery
- Pause animations
**Why important:** Prevents background faces from consuming resources  
**Example:**
```c
static lv_timer_t *update_timer = NULL;

static void hide_old_fashioned(void) {
  // Hide UI
  lv_obj_add_flag(face_bg, LV_OBJ_FLAG_HIDDEN);
  
  // Stop timer
  if (update_timer) {
    lv_timer_del(update_timer);
    update_timer = NULL;
  }
}
```

#### `destroy()` — Cleanup and free memory
**When called:** When the face is permanently removed (rare, usually on shutdown)  
**Purpose:** Free all allocated memory, delete LVGL objects, release resources  
**Why separate:** Most systems keep faces alive and just hide/show them; destroy is for cleanup  
**Example:**
```c
static void destroy_old_fashioned(void) {
  // Delete timers
  if (update_timer) {
    lv_timer_del(update_timer);
  }
  
  // Delete LVGL objects
  if (hour_hand) lv_obj_del(hour_hand);
  if (minute_hand) lv_obj_del(minute_hand);
  if (face_bg) lv_obj_del(face_bg);
  
  // Free any other allocated resources
}
```

#### Typical Lifecycle Flow:
```
1. System startup:
   → init() called for all faces (objects created, hidden)

2. User selects "Analog" face:
   → show() called for analog face (becomes visible, timer starts)

3. User switches to "Digital" face:
   → hide() called for analog face (hidden, timer stopped)
   → show() called for digital face (becomes visible, timer starts)

4. User switches back to "Analog":
   → hide() called for digital face
   → show() called for analog face (no init needed, already created!)

5. System shutdown:
   → destroy() called for all faces (cleanup)
```

#### Why This Pattern?
- **Fast switching:** Objects pre-created, just hide/show (no allocation delays)
- **Resource efficient:** Only active face runs timers/updates
- **Clean separation:** Each face manages its own lifecycle
- **Flexible:** Can implement lazy init (init on first show) or eager init (all at startup)

### Connecting Time/Date Callbacks to Faces

The `clock_ui_set_date` and `clock_ui_set_time` callbacks need to forward updates to the currently active face. Here's how to wire them together:

#### Step 1: Extend the Interface with Time/Date Update Callbacks

Add update callbacks to `clock_face_t` in `face_registry.h`:

```c
typedef struct {
  const char *id;
  const char *name;
  void (*init)(void);
  void (*show)(void);
  void (*hide)(void);
  void (*destroy)(void);
  
  // Time/date update callbacks
  void (*set_time)(int hour, int minute, int second);  // Called when time changes
  void (*set_date)(int year, int month, int day);      // Called when date changes
} clock_face_t;
```

#### Step 2: Track the Active Face in `clock_ui.c`

```c
#include "clock_faces/face_registry.h"

static const clock_face_t *current_face = NULL;

void clock_ui_init(void) {
  // Initialize LVGL container
  // ...
  
  // Initialize all faces (they stay hidden)
  for (size_t i = 0; i < clock_face_count(); i++) {
    const clock_face_t *face = clock_face_at(i);
    if (face && face->init) {
      face->init();
    }
  }
  
  // Show default face
  current_face = clock_face_at(0);
  if (current_face && current_face->show) {
    current_face->show();
  }
}

void clock_ui_switch_to_face(size_t index) {
  const clock_face_t *next_face = clock_face_at(index);
  if (!next_face) return;
  
  // Hide current face
  if (current_face && current_face->hide) {
    current_face->hide();
  }
  
  // Show new face
  current_face = next_face;
  if (current_face && current_face->show) {
    current_face->show();
  }
}
```

#### Step 3: Forward Time/Date Updates to Active Face

In `clock_ui.c`, forward the callbacks to the active face:

```c
// These are your existing callbacks - now they forward to the active face
void clock_ui_set_time(int hour, int minute, int second) {
  // Forward to currently active face
  if (current_face && current_face->set_time) {
    current_face->set_time(hour, minute, second);
  }
}

void clock_ui_set_date(int year, int month, int day) {
  // Forward to currently active face
  if (current_face && current_face->set_date) {
    current_face->set_date(year, month, day);
  }
}
```

#### Step 4: Implement Time/Date Updates in Each Face

Each face implements these callbacks to update its own display:

**Example: `face_digital.c`**
```c
static lv_obj_t *time_label, *date_label;
static int current_hour, current_minute, current_second;
static int current_year, current_month, current_day;

static void set_time_digital(int hour, int minute, int second) {
  current_hour = hour;
  current_minute = minute;
  current_second = second;
  
  // Update the label if face is visible
  if (time_label) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
    lv_label_set_text(time_label, buf);
  }
}

static void set_date_digital(int year, int month, int day) {
  current_year = year;
  current_month = month;
  current_day = day;
  
  // Update the label if face is visible
  if (date_label) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    lv_label_set_text(date_label, buf);
  }
}

static const clock_face_t face_digital = {
  .id = "digital",
  .name = "Digital",
  .init = init_digital,
  .show = show_digital,
  .hide = hide_digital,
  .destroy = destroy_digital,
  .set_time = set_time_digital,  // Connect the callback
  .set_date = set_date_digital,  // Connect the callback
};
```

**Example: `face_old_fashioned.c` (Analog)**
```c
static lv_obj_t *hour_hand, *minute_hand, *second_hand;

static void set_time_old_fashioned(int hour, int minute, int second) {
  // Update analog hands
  // Hour hand: (hour % 12) * 30 + minute * 0.5 degrees
  int hour_angle = ((hour % 12) * 30) + (minute / 2);
  lv_arc_set_value(hour_hand, hour_angle);
  
  // Minute hand: minute * 6 degrees
  lv_arc_set_value(minute_hand, minute * 6);
  
  // Second hand: second * 6 degrees
  lv_arc_set_value(second_hand, second * 6);
}

static void set_date_old_fashioned(int year, int month, int day) {
  // Analog faces might not show date, or show it differently
  // Could update a small date display if present
}

static const clock_face_t face_old_fashioned = {
  .id = "old_fashioned",
  .name = "Analog",
  .init = init_old_fashioned,
  .show = show_old_fashioned,
  .hide = hide_old_fashioned,
  .destroy = destroy_old_fashioned,
  .set_time = set_time_old_fashioned,
  .set_date = set_date_old_fashioned,  // Can be NULL if not used
};
```

#### Step 5: Handle Updates When Face Becomes Active

When a face is shown, it should immediately update with the current time/date:

```c
static void show_digital(void) {
  // Make visible
  lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
  
  // Request current time/date from clock_ui and update display
  // Option 1: If clock_ui exposes getters:
  int h, m, s, y, mo, d;
  clock_ui_get_current_time(&h, &m, &s);
  clock_ui_get_current_date(&y, &mo, &d);
  set_time_digital(h, m, s);
  set_date_digital(y, mo, d);
  
  // Option 2: Or trigger a refresh callback
  // clock_ui_refresh_current_time();
}
```

#### Complete Flow Diagram:

```
External time source (RTC, NTP, etc.)
    ↓
clock_ui_set_time(h, m, s)  ← Your existing callback
    ↓
clock_ui.c checks: current_face->set_time
    ↓
current_face->set_time(h, m, s)  ← Forwards to active face
    ↓
face_digital.c: set_time_digital() updates LVGL labels
OR
face_old_fashioned.c: set_time_old_fashioned() updates analog hands
```

#### Key Points:
- **`clock_ui.c` acts as a router:** It receives time/date callbacks and forwards them to the active face
- **Each face handles its own display:** Different faces update differently (digital vs analog)
- **Only active face receives updates:** Inactive faces don't waste CPU updating hidden displays
- **Callbacks can be NULL:** If a face doesn't need date updates, set `set_date` to `NULL` and check before calling
- **Update on show:** When switching faces, the new face should immediately refresh with current time/date

### Registry (one place to list faces)
```c
extern const clock_face_t *clock_face_get_old_fashioned(void);
extern const clock_face_t *clock_face_get_digital(void);

static const clock_face_t *faces[] = {
  clock_face_get_old_fashioned(),
  clock_face_get_digital(),
  // add new face here
};

size_t clock_face_count(void) { return ARRAY_SIZE(faces); }
const clock_face_t *clock_face_at(size_t i) { return faces[i]; }
```

### Switching
```c
static const clock_face_t *current;
void switch_to(const clock_face_t *next) {
  if (current && current->hide) current->hide();
  current = next;
  if (current && current->show) current->show();
}
```

### Per-face rules
- Keep LVGL objects/timers local to the face; clean them in `hide/destroy`.
- Store assets next to the face code.
- Use stable `id` (no spaces); use `name` for UI text.

### Add a new face (quick checklist)
1) Create `components/clock_ui/clock_faces/face_<new>/face_<new>.c/.h`.
2) Implement `clock_face_get_<new>()` returning your `clock_face_t` (in that `.c`).
3) Include its header in `components/clock_ui/clock_faces/face_registry.c` and add it to `faces[]`.
4) In `clock_ui.c`, include `face_registry.h`, call `clock_face_count()/clock_face_at()` to list, and `switch_to()` to change.
5) Build/test switching; ensure hide/show stop/start timers; verify time/date hooks still work.

---

## Step-by-Step Implementation Guide

### Step 1: Create the Directory Structure
```
components/clock_ui/clock_faces/
├── face_registry.h
├── face_registry.c
├── face_old_fashioned/
│   ├── face_old_fashioned.h
│   └── face_old_fashioned.c
└── face_digital/
    ├── face_digital.h
    └── face_digital.c
```

### Step 2: Define the Interface (`face_registry.h`)
Create the header with the `clock_face_t` typedef:

```c
#ifndef FACE_REGISTRY_H
#define FACE_REGISTRY_H

#include <stddef.h>

typedef struct {
  const char *id;          // "old_fashioned", "digital", ...
  const char *name;        // shown in menu
  void (*init)(void);      // build LVGL objects (hidden)
  void (*show)(void);      // make visible, start timers
  void (*hide)(void);      // hide, stop timers
  void (*destroy)(void);   // free if needed
} clock_face_t;

// Registry functions
size_t clock_face_count(void);
const clock_face_t *clock_face_at(size_t i); 

#endif
```

### Step 3: Implement Individual Faces
For each face (e.g., `face_old_fashioned.c`), implement the getter:

```c
#include "face_registry.h"

static void init_old_fashioned(void) { /* ... */ }
static void show_old_fashioned(void) { /* ... */ }
static void hide_old_fashioned(void) { /* ... */ }
static void destroy_old_fashioned(void) { /* ... */ }

static const clock_face_t face_old_fashioned = {
  .id = "old_fashioned",
  .name = "Analog",
  .init = init_old_fashioned,
  .show = show_old_fashioned,
  .hide = hide_old_fashioned,
  .destroy = destroy_old_fashioned,
};

const clock_face_t *clock_face_get_old_fashioned(void) {
  return &face_old_fashioned;
}
```

### Step 4: Create the Registry (`face_registry.c`)
This is where the registry array (lines 50-51) lives:

```c
#include "face_registry.h"
#include "face_old_fashioned/face_old_fashioned.h"
#include "face_digital/face_digital.h"

// Forward declarations (or include headers)
extern const clock_face_t *clock_face_get_old_fashioned(void);
extern const clock_face_t *clock_face_get_digital(void);

// THE REGISTRY ARRAY (lines 50-51)
static const clock_face_t *faces[] = {
  clock_face_get_old_fashioned(),
  clock_face_get_digital(),
  // add new face here
};

// Helper macro for array size (if not already defined)
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

size_t clock_face_count(void) {
  return ARRAY_SIZE(faces);
}

const clock_face_t *clock_face_at(size_t i) {
  if (i >= clock_face_count()) return NULL;
  return faces[i];
}
```

**Key points about the registry array (lines 50-51):**
- `faces[]` is `static` — only accessible within `face_registry.c`
- Each entry calls a getter function that returns a pointer to a `clock_face_t`
- The getters are declared `extern` so the compiler knows they exist
- Order matters — the first entry is index 0, second is index 1, etc.
- To add a face: implement the getter, include its header, add it to the array

### Step 5: Implementation Order
1. **Start with `face_registry.h`** — define the interface first
2. **Implement one face first** (e.g., `face_old_fashioned`) to validate the pattern
3. **Create `face_registry.c`** with the `faces[]` array and helper functions
4. **Test with one face** before adding more
5. **Add the second face** (`face_digital`)
6. **Wire into `clock_ui.c`** to use the registry

### Step 6: Integration with `clock_ui.c`
```c
#include "clock_faces/face_registry.h"

// To switch faces:
void clock_ui_switch_to_face(size_t index) {
  const clock_face_t *face = clock_face_at(index);
  if (face) {
    // Hide current, show new
    switch_to(face);
  }
}

// To list all faces:
for (size_t i = 0; i < clock_face_count(); i++) {
  const clock_face_t *face = clock_face_at(i);
  printf("Face %zu: %s (%s)\n", i, face->name, face->id);
}
```

### Step 7: Common Pitfalls to Avoid
1. **Missing extern declarations** — declare getters before using them
2. **Circular includes** — keep headers minimal; forward declare when needed
3. **Uninitialized function pointers** — ensure all callbacks are set (can be NULL if not needed)
4. **Array size macro** — define `ARRAY_SIZE` if your platform doesn't have it
5. **Static array initialization** — make sure getter functions are callable at compile/init time
