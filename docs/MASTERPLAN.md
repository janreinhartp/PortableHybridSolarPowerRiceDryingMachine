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
| [07-hmi-ux-notes.md](07-hmi-ux-notes.md) | Phase 7 HMI UX debt record |
| [08-hmi-design-guideline.md](08-hmi-design-guideline.md) | Active SCADA HMI design guideline |
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
Firmware Phases 0–4       COMPLETE (firmware + hardware bench)
Phase 1 board bench       COMPLETE
Phase 2 RTC bench         COMPLETE
Phase 3 fieldbus bench    COMPLETE (2× XY-MD03 + N4D8B08)
Phase 4 moisture/EC bench COMPLETE (ADS1115 path)
Firmware Phase 5          IN TREE
Firmware Phase 6          IN TREE
Firmware Phase 7          IN TREE
Firmware Phase 8          IN TREE
Hardware Integration      NOT STARTED
Dry Run                   NOT STARTED (checklist ready)
Rice Validation           NOT STARTED (checklist ready)
```

What exists today:

- Design docs under `docs/01` through `docs/08`, summarized in [README.md](../README.md)
- Waveshare board demo at [ESP32-S3-Touch-LCD-7B-Demo/15_LVGL_SLIDER](ESP32-S3-Touch-LCD-7B-Demo/15_LVGL_SLIDER), kept as a board reference
- Product firmware under `firmware/` through Phase 8: board bring-up, TinyRTC, RS485/Modbus, XY-MD03, N4D8B08, ADS1115, Sensor Manager, Actuator Manager, Safety Manager, Settings Manager, Circulation Manager, Process Controller, Batch Manager, queue-based SD logger, product SCADA HMI, and local SoftAP read-only web dashboard with CSV export. Manual control remains available for bench tests.
- Phase 9 hardware validation checklist at [data/test-results/phase9-validation-checklist.md](../data/test-results/phase9-validation-checklist.md)
- GPIO and fieldbus map at [hardware/wiring/esp32-gpio-map.md](../hardware/wiring/esp32-gpio-map.md)
- Placeholder folders for schematics, PCB, mechanical, calibration data, test results, and tests

Known as-built gaps (honest, not blockers for Phase 9 start):

- Door and elevator use open-loop travel timeouts (no limit-switch GPIOs on the first prototype)
- F002 (E-stop) is defined for UI/logging but is not raised by the ESP32 (panel electrical only)
- Manual door-stop API exists; Manual screen has open/close only
- RTC set/adjust APIs exist; not wired in the HMI
- Power-loss recovery flags an interrupted batch and requires operator ACK; it does not auto-resume the batch
- RS485 product baud is **19200**; commission N4D8B08 DIP + XY-MD03 baud reg `0x0102` before flashing if devices still ship at 9600

As-built heater control: chamber PID with time-proportioned relay duty + hot-air hard cap (`max_hot_air_temperature_c`). Moisture: 3-point piecewise calibration via System → Calibrate Moisture. EC is not used.

Next implementation step: **Phase 9 — Validation** (dry run → empty chamber → small batch → 10 kg).

---

# 2. Waveshare Demo Review

The Waveshare ESP-IDF demo `15_LVGL_SLIDER` remains the board reference. Product firmware lives in `firmware/`.

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

The product `firmware/` ESP-IDF project already exists. Board bring-up came from the demo components below; keep those reuse rules when extending drivers.

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

RS485 UART and RS485 direction control must use only remaining free pins.

Door travel limits are **not** ESP32 inputs — the linear actuator has internal limit switches.
Emergency stop is **not** an ESP32 input — it is hard-wired to drop the panel main contactor.

**First firmware document before Modbus or ADS1115 work:** `hardware/wiring/esp32-gpio-map.md`

## 4.2 Shared I2C Bus

One I2C bus serves:

| Device | Typical address | Role |
|---|---|---|
| GT911 | board-defined | Touch |
| IO expander | `0x24` | Backlight, resets, SD select |
| TinyRTC | `0x68` | Date and time |
| ADS1115 | `0x48` default | Moisture ADC (AIN2); AIN3 unused |

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
| Door position feedback | None to ESP32; actuator internal limit switches end travel |
| Emergency stop | Electrical only — E-stop kills the main contactor; no ESP32 GPIO |
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
| Chamber PID (Kp/Ki/Kd) + window | 4.0 / 0.05 / 8.0 / 20 s |
| Hot-air max (cap + F001) | 70°C |
| Cooling temperature | 35°C |
| Circulation mode | Interval |
| Circulation interval | 2 min |
| Circulation run time | 10 s |
| Discharge time | 60 s |

---

# 6. Firmware Layout

## 6.1 As-built (Phases 0–8)

Low-level drivers live under `firmware/components/`, not under `main/drivers/`.

```text
firmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── components/
│   ├── rgb_lcd_port/      # Waveshare
│   ├── touch/
│   ├── i2c/
│   ├── io_extension/
│   ├── lvgl_port/
│   ├── sd/
│   ├── gpio/
│   ├── rs485/            # Phase 3
│   ├── modbus/
│   ├── rtc/
│   ├── relay/
│   └── ads1115/          # Phase 4
└── main/
    ├── main.cpp
    ├── system_status.h
    ├── board/
    │   ├── board_hal.c/h
    │   └── gpio_map.h
    ├── app/
    │   ├── dryer_controller/   # bring-up + fieldbus poll
    │   ├── process_controller/ # Phase 6 state machine
    │   ├── safety_manager/     # Phase 5
    │   └── batch_manager/      # Phase 6 lifecycle + NVS recovery
    ├── services/
    │   ├── th_sensor/          # XY-MD03 helper used by Sensor Manager
    │   ├── sensor_manager/     # Phase 4 snapshot + NVS calibration
    │   ├── actuator_manager/   # Phase 4 sole relay writer
    │   ├── settings_manager/   # Phase 5
    │   ├── circulation_manager/# Phase 5
    │   ├── logger/             # Phase 7 queue + SD
    │   ├── web_server/         # Phase 8 SoftAP read-only
    │   └── rtc_manager/
    └── ui/
        ├── theme/              # SCADA palette + typography
        ├── widgets/            # shared KPI / bar / actuator tiles
        ├── trend_buffer/       # RAM moisture + chamber trends
        ├── ui_manager/         # shell, navigation, 1 Hz refresh
        └── screens/            # Phase 7 product HMI (08 guideline)
            ├── dashboard/      # Home
            ├── batch_setup/
            ├── run/            # Drying / Cooling / Discharging
            ├── complete/
            ├── alarms/
            ├── more/
            ├── manual_control/
            ├── settings/
            ├── history/
            ├── system_info/
            └── commission/
```

## 6.2 Later additions (post–Phase 8)

Optional modules not required for Phase 9 validation. Keep Phase 3–8 modules until replaced.

```text
firmware/main/services/
└── diagnostics/          # later — deeper field diagnostics
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

Phases 0–8 firmware is in the tree. Phases 1–4 hardware bench is complete (board, RTC, 2× XY-MD03, N4D8B08, ADS1115 moisture/EC). Phase 9 hardware validation is next — see `data/test-results/phase9-validation-checklist.md`. Keep heater power disconnected until Phase 5 safety tests pass.

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

Status: folders, the untouched Waveshare demo, and the README link to this plan are in the repository.

Acceptance:

```text
[x] Folders exist
[x] Demo remains unmodified under docs/
[x] README points to this master plan
```

## Phase 1 — Foundation

Bring up the 7-inch board from copied Waveshare drivers.

Tasks:

1. Create ESP-IDF project for ESP32-S3
2. Copy RGB, touch, I2C, IO expander, LVGL port, and SD components
3. Boot a simple status screen
4. Confirm touch, backlight, and `/sdcard`
5. Log free heap over serial

Status: firmware and board bench complete.

Acceptance:

```text
[x] ESP32-S3 boots
[x] Display works at 1024 × 600
[x] Touch works
[x] LVGL works
[x] SD mounts at /sdcard
[x] Serial logging works
```

## Phase 2 — Pin Map and RTC

Tasks:

1. Write `hardware/wiring/esp32-gpio-map.md`
2. Reserve free pins for RS485 UART and DE/RE only (no door-limit or E-stop GPIOs)
3. Add TinyRTC on the shared I2C bus
4. Set and read date/time from the HMI

Status: GPIO map, TinyRTC driver, and RTC bench are complete.

Acceptance:

```text
[x] GPIO map documents every used and free pin
[x] RTC reads and writes time
[x] RTC failure is detected
[x] No I2C address collision with GT911 or IO expander
```

## Phase 3 — RS485 and Modbus

Tasks:

1. Implement RS485 UART driver (Waveshare onboard auto-DE: TX16 / RX15)
2. Implement Modbus RTU with CRC, timeout, and retry
3. Control **N4D8B08** relay board (FC03/FC06); product address **10** = DIP **A1+A3 ON** (raw binary; A0 alone → ID 1 for bench)
4. Poll **XY-MD03** hot-air sensor at address **1**
5. Poll **XY-MD03** chamber sensor at address **2**
6. Add **sensor address commissioning** in firmware + HMI (no DIP switches on XY-MD03)

### XY-MD03 temperature/humidity probes

Model: XY-MD03 (PE probe, SHT40), Modbus-RTU over RS485. Product bus rate: **19200 8N1** (factory default is often 9600 — commission baud before relying on the ESP).

| Item | Value |
|---|---|
| Power | DC 5–28 V (Red VCC, Black GND) |
| Bus | Yellow = A, White = B (+ common GND) |
| Read T/H | FC **0x04**, start reg **0x0001**, count **2** → temp×10, humidity×10 |
| Device address register | Holding **0x0101** (FC03 read / FC06 write), range **1–247** |
| Baud register | Holding **0x0102** — codes **0=9600**, **1=14400**, **2=19200**; helper `th_sensor_set_baud(addr, 2)` |

There is **no address DIP**. Both probes usually ship as slave **1**. Address is changed by writing register `0x0101` over Modbus (and must persist after power cycle).

**Baud + address commissioning (full procedure):**

Detailed switch tables and order live in [`hardware/wiring/esp32-gpio-map.md`](../hardware/wiring/esp32-gpio-map.md) §5.1. Summary:

1. **Relay N4D8B08 (power off):** address DIP **A1+A3 ON** → ID **10**. Set baud DIP pair to **19200** (typical: BR1=OFF, BR0=ON — confirm silk/PDF). Power on.
2. **Hot-air XY-MD03 (one probe only, host still @ 9600 if factory):** FC06 write holding **0x0102 = 2**, power-cycle probe, then assign address **1** (HMI Commission or `0x0101`).
3. **Chamber XY-MD03 (second probe alone @ 9600):** same baud write **0x0102 = 2**, power-cycle, assign address **2**.
4. **ESP32:** run firmware at **19200**. Reconnect all three devices; verify hot-air @1, chamber @2, relay @10.

**Address-only reminder (after baud is already 19200):**

1. Relay already at **10**.
2. Connect **only one** XY-MD03 → HMI Commission → **Assign Hot Air (1)** or **Assign Chamber (2)**.
3. Power-cycle the probe; confirm it answers at the new ID.
4. Repeat for the second probe; reconnect both.

Firmware must expose:

- `th_sensor_read(addr)` using XY-MD03 register map
- `th_sensor_set_address(current_addr, new_addr)` via FC06 → `0x0101`
- HMI buttons/screen for assign Hot-air / Chamber (and status feedback)

Status: firmware and fieldbus bench are complete (`rs485`, `modbus_rtu`, `th_sensor`, `relay_modbus`, HMI sensor setup and manual relay buttons; both XY-MD03 probes and N4D8B08 verified on the bus).

Commissioning note:

- Sensor setup must assign a lone factory-default probe. With another unit already answering, the first successful read can be rewritten to the new role.

Resolved in Phase 4 Actuator Manager / relay driver:

- Door OPEN/CLOSE/STOP discover the relay before writing (same path as fan/elevator/heater).
- All-off keeps the last known coil state when the Modbus write fails.

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
[x] Both temperature/humidity sensors respond at unique addresses
[x] Sensor address can be set from HMI (or serial) without DIP switches
[x] CRC and timeout work
[x] All required relays toggle correctly
[x] R4 and R5 are never both ON
[x] Relay at 10 does not collide with sensors at 1 and 2
```

## Phase 4 — ADS1115, Sensors, and Actuators

Status: firmware and moisture/EC bench are complete (`ads1115`, `sensor_manager`, `actuator_manager`, Manual Control HMI via Actuator Manager, NVS linear calibration). Channel map as wired: **AIN2** moisture, **AIN3** EC.

Tasks (done):

1. Implement ADS1115 driver on the shared I2C bus
2. Read moisture and EC channels
3. Store linear calibration in NVS
4. Publish one validated `SensorData` snapshot
5. Implement Actuator Manager as the only relay writer
6. Refactor bring-up HMI relay buttons behind Actuator Manager (Manual Control)
7. Heater control-line bench available (keep high-power heater disconnected until Phase 5 safety tests pass)

Acceptance:

```text
[x] ADS1115 responds at the configured address
[x] Moisture and EC channels convert to voltage
[x] Calibration loads from NVS
[x] Sensor manager marks invalid readings
[x] Manual fan, elevator, and door work with interlocks
[x] Heater control line can be forced OFF by software
[x] 0–5 V never reaches an ESP32 pin
```

## Phase 5 — Safety, Settings, Circulation

Status: firmware is in the tree (`safety_manager`, `settings_manager`, `circulation_manager`, fault codes F001–F012). Keep Manual Control for bench tests. Keep heater power disconnected until safety tests pass.

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
[x] Over-temperature forces heater OFF and fan ON
[x] Invalid sensor data blocks heating
[x] Door OPEN and CLOSE remain interlocked
[x] Interval circulation timing is non-blocking
[x] Invalid settings are rejected
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

Acceptance (firmware for these behaviors is **in tree**; checkboxes mean hardware dry-run sign-off in Phase 9, not “still to code”):

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

Firmware for logging and the product HMI is in the tree. The operator SCADA redesign is **implemented** per [08-hmi-design-guideline.md](08-hmi-design-guideline.md). Phase 7 baseline UX debt remains recorded in [07-hmi-ux-notes.md](07-hmi-ux-notes.md). Full dry-run / soak acceptance on hardware is still open.

Tasks:

1. Queue-based logger at 1-second interval during active states
2. Batch summary at completion
3. Screens: Home (Dashboard), Batch Setup, Run (Drying/Cooling/Discharging), Complete, Alarms, More → Manual, Settings, History, System Info, Commission
4. HMI posts commands through the application layer only
5. Light SCADA theme, gauges with Target lines, 1 Hz trend buffer, bottom nav (Home / Run / Alarms / More)

Acceptance:

```text
[x] CSV rows write without blocking control (queued logger in tree)
[x] History lists completed batches
[x] Alarms show code, time, and state
[x] Operator SCADA screens per docs/08-hmi-design-guideline.md
[ ] Touchscreen soak / full dry-run acceptance on hardware
```

UX redesign:

```text
[x] HMI design guideline agreed (docs/08-hmi-design-guideline.md)
[x] Operator-facing screens redesigned per that guideline
```

## Phase 8 — Local Web Dashboard

Firmware for SoftAP + read-only HTTP dashboard is in the tree (`services/web_server/`).

Tasks:

1. Start Wi-Fi AP `RiceDryer-XXXX`
2. Serve read-only dashboard at `192.168.4.1`
3. Implement the locked GET API
4. Stream CSV export from the SD card

Acceptance:

```text
[x] SoftAP + GET API + dashboard HTML in firmware
[x] CSV export streams from /sdcard (path-validated)
[x] No web endpoint can move an actuator (GET-only; POST/PUT/DELETE rejected)
[ ] Phone or laptop join / live update soak on hardware
```

## Phase 9 — Validation

Progress from low risk to full capacity. Use the checklist at `data/test-results/phase9-validation-checklist.md`.

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
Door command timeout
Elevator stuck-on
Over-temperature
Power interruption / E-stop contactor drop (electrical)
Invalid configuration
```

Electrical E-stop is validated on the panel (contactor drop), not as an ESP32 input.

---

# 11. Explicit Non-Goals Until 10 kg Is Stable

Do not implement these before the baseline dryer works:

- Cloud or phone remote control
- Web-based actuator control
- Inverter, MPPT, or battery monitoring
- Solar production analytics
- AI drying optimization
- Automatic loading

Chamber PID + hot-air cap, 3-point moisture calibration HMI, boot splash, and RS485 @ 19200 are in the baseline.

---

# 12. Definition of Done

The project is ready for normal operation when:

### Hardware

```text
[ ] Wiring verified and labeled
[ ] Electrical protection installed
[ ] Independent thermal protection installed
[ ] Emergency stop tested electrically (main contactor drop)
[ ] ADS1115 moisture path validated
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

Start **Phase 9** hardware validation using [data/test-results/phase9-validation-checklist.md](../data/test-results/phase9-validation-checklist.md):

1. Dry run with forced/simulated sensors (include SoftAP dashboard check)
2. Empty chamber heat test (only after Phase 5 safety tests pass)
3. Small rice batch with moisture calibration vs reference meter
4. 10 kg validation batch

Phase 8 SoftAP web dashboard is in the tree. Keep Manual Control available for bench tests. Do not connect the high-power heater until Phase 5 safety tests pass.
