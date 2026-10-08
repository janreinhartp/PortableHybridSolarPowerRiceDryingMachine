# Master Plan — Hybrid Solar Power Rice Dryer

This is the implementation roadmap for the 10 kg/batch Hybrid Solar Power Rice Dryer.

The numbered design documents remain the requirements source. This file tells the team how to build firmware and integrate hardware from the current repository state.

| Document | Role |
|---|---|
| [01-product-requirements.md](01-product-requirements.md) | What the machine must do |
| [02-technical-requirements.md](02-technical-requirements.md) | Technical constraints |
| [03-system-architecture.md](03-system-architecture.md) | System structure |
| [04-hardware-design.md](04-hardware-design.md) | Electrical and mechanical hardware |
| [05-software-design.md](05-software-design.md) | Firmware architecture |
| [06-implementation-plan.md](06-implementation-plan.md) | Detailed phase checklist |
| **MASTERPLAN.md** | **Execution order, demo reuse, locked decisions** |

---

# 1. Current Repository State

```text
Requirements              COMPLETE
Technical Requirements    COMPLETE
System Architecture       COMPLETE
Hardware Design           COMPLETE
Software Design           COMPLETE
Implementation Plan       COMPLETE
Firmware Development      NOT STARTED
Hardware Integration      NOT STARTED
Dry Run                   NOT STARTED
Rice Validation           NOT STARTED
```

What exists today:

- Design docs under `docs/01` through `docs/06`, summarized in [README.md](../README.md)
- Waveshare board demo at [ESP32-S3-Touch-LCD-7B-Demo/15_LVGL_SLIDER](ESP32-S3-Touch-LCD-7B-Demo/15_LVGL_SLIDER)

What does not exist yet:

- `firmware/` project
- `hardware/` drawings and wiring maps
- `tests/` and `data/test-results/`

---

# 2. Waveshare Demo Review

The only executable code is the Waveshare ESP-IDF demo `15_LVGL_SLIDER`.

| Item | Value |
|---|---|
| Framework | ESP-IDF `>=5.1` |
| Target | ESP32-S3 |
| Panel | ST7262 RGB, 1024 × 600 |
| Touch | GT911 |
| UI library | LVGL 8 (`>8.3.9,<9`) |
| Demo purpose | Brightness slider + battery-voltage label |

The demo README still mentions a 4.3-inch board. The code matches the 7-inch panel used by this project.

Keep the demo untouched as a board reference. Do not turn the demo into the product firmware.

---

# 3. What to Reuse from the Demo

Create a new `firmware/` ESP-IDF project. Copy only board bring-up components.

| Component | Path | Reuse |
|---|---|---|
| RGB LCD | `components/rgb_lcd_port/` | Yes — 1024 × 600 panel and GPIO map |
| Touch | `components/touch/` | Yes — GT911 |
| I2C | `components/i2c/` | Yes — GPIO8 SDA, GPIO9 SCL, 400 kHz |
| IO expander | `components/io_extension/` | Yes — address `0x24`, backlight, resets, SD select |
| LVGL port | `components/lvgl_port/` | Yes — task, PSRAM buffers, direct-mode double buffer |
| SD | `components/sd/` | Yes — SDMMC 1-bit, CLK 12, CMD 11, D0 13, mount `/sdcard` |
| Slider UI | `main/lvgl_demo.c` | No — product HMI replaces it |
| Battery ADC loop | `loop_bat()` | No — not the moisture path |

Startup pattern to keep from `main.c`:

```text
touch_gt911_init()
  -> waveshare_esp32_s3_rgb_lcd_init()
  -> backlight on
  -> lvgl_port_init()
  -> create UI under lvgl_port_lock()
```

Every LVGL update must hold `lvgl_port_lock()`. The dryer HMI must refresh from a shared sensor snapshot on a fixed timer. Do not create a new LVGL timer on every sample the way the demo battery loop does.

---

# 4. Hardware Constraints

## 4.1 Busy GPIOs

The RGB panel already uses:

```text
GPIO 0, 1, 2, 3, 5, 7, 10, 14, 17, 18, 21, 38, 39, 40, 41, 42, 45, 46, 47, 48
```

Also reserved:

```text
I2C: GPIO8 (SDA), GPIO9 (SCL)
SD:  GPIO11 (CMD), GPIO12 (CLK), GPIO13 (D0)
Demo LED: GPIO6
```

RS485 UART, RS485 direction control, door limit inputs, and emergency-stop input must use only remaining free pins.

**First firmware document before Modbus or ADS1115 work:** `hardware/wiring/esp32-gpio-map.md`

## 4.2 Shared I2C Bus

One I2C bus serves:

| Device | Typical address | Role |
|---|---|---|
| GT911 | board-defined | Touch |
| IO expander | `0x24` | Backlight, resets, SD select |
| TinyRTC | `0x68` | Date and time |
| ADS1115 | `0x48` default | Moisture and EC ADC |

Do not start a second I2C controller. Confirm that every address is unique before enabling the ADS1115 driver.

## 4.3 Moisture and EC Path

The moisture/EC sensor is 0–5 V.

```text
Moisture / EC Sensor
        |
        | 0-5 V
        v
 Protection / Filter / Scaling if needed
        |
        v
     ADS1115
        |
        | I2C
        v
     ESP32-S3
```

Rules:

- The 0–5 V signal never connects to an ESP32 GPIO.
- Moisture and EC use separate ADS1115 inputs when the sensor provides both.
- Acceptable ADS1115 supplies:
  - 5 V supply, PGA ±6.144 V, I2C level-shifted to 3.3 V, or
  - 3.3 V supply with a divider and protection so the pin stays inside the selected range
- Do not use the IO-expander battery ADC from the Waveshare demo for moisture.

## 4.4 Out of Firmware Scope

The hybrid inverter manages solar charging, MPPT, battery charging, and AC/solar source selection. The ESP32 controls only the dryer.

---

# 5. Locked Baseline Decisions

These resolve contradictions between the design docs so implementation does not fork.

| Topic | Decision |
|---|---|
| SD mount | `/sdcard` |
| Log folders | `/sdcard/batches`, `/sdcard/summaries`, `/sdcard/logs`, `/sdcard/faults` |
| Batch ID | `BATCH-0001` |
| Process CSV name | Matches batch ID, for example `BATCH-0001.csv` |
| Analog input | ADS1115 over I2C |
| Heater control temperature | Chamber sensor |
| Cooling completion temperature | Chamber sensor |
| Over-temperature safety input | Hot-air sensor |
| Heating permission | Both chamber and hot-air readings must be valid |
| Elevator continuous mode | No software runtime timeout for intentional continuous drying circulation |
| Elevator interval mode | Uses configured run time; stuck-on relay is a separate fault |
| Over-temperature fault | Heater OFF, fan ON |
| Other critical faults | Heater OFF, elevator OFF |
| Door on fault | Keep last safe commanded state unless the fault is a door fault |
| Web API v1 | Read-only monitoring only |
| Web control | None in v1 |
| Internet | Not required |

Web endpoints for v1:

```text
GET /api/status
GET /api/sensors
GET /api/actuators
GET /api/batch
GET /api/settings
GET /api/history
GET /api/faults
GET /api/export/{file}
```

No actuator endpoints.

Default process settings:

| Parameter | Default |
|---|---:|
| Target moisture | 14% |
| Drying temperature | 50°C |
| Temperature hysteresis | 2°C |
| Cooling temperature | 35°C |
| Circulation mode | Interval |
| Circulation interval | 2 min |
| Circulation run time | 10 s |
| Discharge time | 60 s |

---

# 6. Target Firmware Layout

```text
firmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── components/                  # Waveshare board drivers + new drivers
│   ├── rgb_lcd_port/
│   ├── touch/
│   ├── i2c/
│   ├── io_extension/
│   ├── lvgl_port/
│   ├── sd/
│   ├── rs485/
│   ├── modbus/
│   ├── ads1115/
│   ├── rtc/
│   └── relay/
└── main/
    ├── main.cpp
    ├── app/
    │   ├── dryer_controller/
    │   ├── process_controller/
    │   ├── safety_manager/
    │   └── batch_manager/
    ├── services/
    │   ├── sensor_manager/
    │   ├── circulation_manager/
    │   ├── actuator_manager/
    │   ├── settings_manager/
    │   ├── logger/
    │   ├── diagnostics/
    │   ├── web_server/
    │   └── rtc_manager/
    └── ui/
        ├── ui_manager/
        ├── screens/
        └── components/
```

Language choice:

- C for board and low-level drivers where the Waveshare code is already C
- C++ for application and service layers

---

# 7. Runtime Architecture

```text
Sensors
   |
   v
Sensor Manager -----> Safety Manager -----> Actuator Manager -----> Modbus Relay
   |                        ^
   |                        |
   +----> Process Controller
   |              |
   |              +----> Batch Manager ----> Logger Queue ----> SD Card
   |
   +----> LVGL HMI
   |
   +----> Read-only Web API
```

FreeRTOS tasks:

| Task | Role | Target period |
|---|---|---|
| Safety | Critical limits and safe outputs | ≤ 100 ms |
| Control | State machine and process requests | 100–500 ms |
| Sensor / Modbus | RS485 devices and ADS1115 | 500–1000 ms |
| HMI | Existing LVGL port task | 10–500 ms |
| Logger | SD writes from a queue | 1 s active logging |
| Web | Local HTTP | event-driven |
| RTC | Timekeeping | 1 s |

Rules:

- Process and UI never write relays directly.
- Safety can override process commands.
- Control never blocks on SD or HTTP.
- No long `delay()` for circulation, discharge, or drying timers.

Process state machine:

```text
IDLE -> PRECHECK -> DRYING -> COOLING -> DISCHARGING -> COMPLETE
                              |
                              +---- FAULT from any active state
```

---

# 8. Build Order

Each phase must pass its acceptance checks on hardware before the next phase starts. Keep heater power disconnected until Phase 5 safety tests pass.

## Phase 0 — Repository Skeleton

Create:

```text
firmware/
hardware/wiring/
hardware/schematics/
data/calibration/
data/test-results/
tests/
```

Acceptance:

```text
[ ] Folders exist
[ ] Demo remains unmodified under docs/
[ ] README points to this master plan
```

## Phase 1 — Foundation

Bring up the 7-inch board from copied Waveshare drivers.

Tasks:

1. Create ESP-IDF project for ESP32-S3
2. Copy RGB, touch, I2C, IO expander, LVGL port, and SD components
3. Boot a simple status screen
4. Confirm touch, backlight, and `/sdcard`
5. Log free heap over serial

Acceptance:

```text
[ ] ESP32-S3 boots
[ ] Display works at 1024 × 600
[ ] Touch works
[ ] LVGL works
[ ] SD mounts at /sdcard
[ ] Serial logging works
```

## Phase 2 — Pin Map and RTC

Tasks:

1. Write `hardware/wiring/esp32-gpio-map.md`
2. Reserve free pins for RS485 UART, DE/RE, door limits, and E-stop
3. Add TinyRTC on the shared I2C bus
4. Set and read date/time from the HMI

Acceptance:

```text
[ ] GPIO map documents every used and free pin
[ ] RTC reads and writes time
[ ] RTC failure is detected
[ ] No I2C address collision with GT911 or IO expander
```

## Phase 3 — RS485 and Modbus

Tasks:

1. Implement RS485 UART driver with direction control
2. Implement Modbus RTU with CRC, timeout, and retry
3. Poll hot-air sensor at address 1
4. Poll chamber sensor at address 2
5. Control 8-channel relay at address 10

Relay map:

| Relay | Function |
|---|---|
| R1 | Elevator |
| R2 | Heater control |
| R3 | Fan |
| R4 | Door OPEN |
| R5 | Door CLOSE |
| R6–R8 | Spare |

Acceptance:

```text
[ ] Both temperature/humidity sensors respond
[ ] CRC and timeout work
[ ] All required relays toggle correctly
[ ] R4 and R5 are never both ON
```

## Phase 4 — ADS1115, Sensors, and Actuators

Tasks:

1. Implement ADS1115 driver on the shared I2C bus
2. Read moisture and EC channels
3. Store linear calibration in NVS
4. Publish one validated `SensorData` snapshot
5. Implement Actuator Manager as the only relay writer
6. Build Manual Control screen for fan, elevator, and door
7. Test heater output into the SSR/contactor control input only

Acceptance:

```text
[ ] ADS1115 responds at the configured address
[ ] Moisture and EC channels convert to voltage
[ ] Calibration loads from NVS
[ ] Sensor manager marks invalid readings
[ ] Manual fan, elevator, and door work with interlocks
[ ] Heater control line can be forced OFF by software
[ ] 0–5 V never reaches an ESP32 pin
```

## Phase 5 — Safety, Settings, Circulation

Tasks:

1. Implement Safety Manager with heater-off authority
2. Implement Settings Manager with validation
3. Implement Circulation Manager for continuous and interval modes
4. Define fault codes F001–F012 from the software design

Settings validation must reject:

```text
Cooling temperature >= drying temperature
Circulation run time >= circulation interval
Any timer <= 0
Temperatures outside safe limits
```

Acceptance:

```text
[ ] Over-temperature forces heater OFF and fan ON
[ ] Invalid sensor data blocks heating
[ ] Door OPEN and CLOSE remain interlocked
[ ] Interval circulation timing is non-blocking
[ ] Invalid settings are rejected
```

## Phase 6 — Process and Batch

Tasks:

1. Implement Process Controller state machine
2. Implement Batch Manager and batch ID generation
3. Enforce moisture completion and cooling completion
4. Implement discharge sequence
5. Store interrupted-batch recovery flag in NVS

Power-loss rule:

```text
After reboot:
  Heater OFF
  Fan OFF
  Elevator OFF
  Door safe/closed
  Operator confirmation required
  No automatic heater restart
```

Acceptance:

```text
[ ] IDLE -> PRECHECK -> DRYING -> COOLING -> DISCHARGING -> COMPLETE
[ ] Moisture ends drying
[ ] Chamber temperature ends cooling
[ ] Discharge timer opens door, runs elevator, then closes door
[ ] Minimum drying time blocks early completion
[ ] Maximum drying time faults
[ ] Interrupted batch requires operator confirmation
```

## Phase 7 — Logging and HMI

Tasks:

1. Queue-based logger at 1-second interval during active states
2. Batch summary at completion
3. Screens: Dashboard, Batch Setup, Drying, Cooling, Discharge, Complete, Manual, Settings, History, Alarms, System Info
4. HMI posts commands through the application layer only

Acceptance:

```text
[ ] CSV rows write without blocking control
[ ] History lists completed batches
[ ] Alarms show code, time, and state
[ ] Touchscreen remains responsive during logging
```

## Phase 8 — Local Web Dashboard

Tasks:

1. Start Wi-Fi AP `RiceDryer-XXXX`
2. Serve read-only dashboard at `192.168.4.1`
3. Implement the locked GET API
4. Stream CSV export from the SD card

Acceptance:

```text
[ ] Phone or laptop can join the AP without internet
[ ] Status and sensors update
[ ] CSV download works
[ ] No web endpoint can move an actuator
```

## Phase 9 — Validation

Progress from low risk to full capacity:

```text
Dry run with forced/simulated sensors
        |
        v
Empty chamber heat test
        |
        v
Small rice batch
        |
        v
10 kg validation batch
```

Record results under `data/test-results/`.

Acceptance for production readiness:

```text
[ ] Full automatic batch completes
[ ] Moisture sensor calibrated against a reference meter
[ ] Safety fault injection passes
[ ] Power-loss recovery stays safe
[ ] 10 kg batch meets target moisture and logs cleanly
```

---

# 9. Coding Sequence

Short form of the work sequence:

```text
01. Repository skeleton
02. ESP-IDF project + Waveshare board bring-up
03. GPIO map
04. TinyRTC
05. RS485 + Modbus
06. Temperature/RH sensors
07. Relay board
08. ADS1115
09. Moisture/EC calibration
10. Sensor Manager
11. Actuator Manager
12. Safety Manager
13. Settings Manager
14. Circulation Manager
15. Batch Manager
16. Process Controller
17. Logger
18. HMI screens
19. Web dashboard
20. Diagnostics + power recovery
21. Dry run
22. Empty chamber
23. Small rice test
24. 10 kg validation
```

---

# 10. Testing Strategy

## Unit

- Moisture and EC conversion
- Calibration
- Heater hysteresis
- Circulation timers
- Discharge timer
- Settings validation
- Fault detection
- State transitions

## Integration

- Sensor Manager + Modbus
- Sensor Manager + ADS1115
- Process Controller + Actuator Manager
- Safety Manager + Actuator Manager
- Logger + SD
- HMI + Process Controller

## Fault injection

```text
Sensor disconnected
RS485 cable disconnected
ADS1115 I2C failure
SD card removed
Door timeout
Elevator stuck-on
Emergency stop
Over-temperature
Power interruption
Invalid configuration
```

---

# 11. Explicit Non-Goals Until 10 kg Is Stable

Do not implement these before the baseline dryer works:

- PID heater control
- Cloud or phone remote control
- Web-based actuator control
- Inverter, MPPT, or battery monitoring
- Solar production analytics
- AI drying optimization
- Automatic loading

---

# 12. Definition of Done

The project is ready for normal operation when:

### Hardware

```text
[ ] Wiring verified and labeled
[ ] Electrical protection installed
[ ] Independent thermal protection installed
[ ] Emergency stop tested
[ ] ADS1115 moisture/EC path validated
```

### Software

```text
[ ] Drivers functional
[ ] Sensors functional
[ ] Actuators functional
[ ] Safety Manager functional
[ ] Process Controller functional
[ ] HMI functional
[ ] Logger functional
[ ] Settings functional
[ ] Read-only web monitoring functional
```

### Process

```text
[ ] PRECHECK works
[ ] DRYING works
[ ] COOLING works
[ ] DISCHARGING works
[ ] COMPLETE works
[ ] FAULT handling works
[ ] Power-loss recovery stays safe
```

### Validation

```text
[ ] Moisture calibrated
[ ] Temperature validated
[ ] Empty chamber test passed
[ ] Small batch test passed
[ ] 10 kg batch test passed
[ ] Safety tests passed
```

---

# 13. Immediate Next Step

Start Phase 0 and Phase 1:

1. Create the `firmware/` ESP-IDF project skeleton
2. Copy the Waveshare board bring-up components
3. Boot display, touch, LVGL, and SD
4. Write the GPIO map before adding RS485 or ADS1115

Do not connect the high-power heater until Phase 5 safety tests pass.
