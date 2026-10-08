# Hybrid Solar Power Rice Dryer

A 10 kg/batch automated rice drying system designed for controlled drying, rice circulation, cooling, and discharge.

The system uses an **ESP32-S3** as the main controller with a touchscreen HMI, temperature and humidity sensors, analog moisture/EC sensing, RS485 Modbus communication, SD card data logging, and local Wi-Fi monitoring.

The dryer is designed to operate **offline** and does not require cloud services or an internet connection.

---

## 1. Project Overview

The system is designed to dry up to **10 kg of rice per batch**.

The operator manually loads the rice into the drying chamber.

The system then automatically manages:

1. Precheck
2. Drying
3. Rice circulation
4. Moisture monitoring
5. Heater control
6. Cooling
7. Rice discharge
8. Batch logging

The main drying process is controlled using rice moisture as the primary completion condition.

---

# 2. Main Process

```text
MANUAL LOAD
     |
     v
   START
     |
     v
 PRECHECK
     |
     v
  DRYING
     |
     | Target Moisture Reached
     v
  COOLING
     |
     | Cooling Temperature Reached
     v
DISCHARGING
     |
     | Discharge Timer Complete
     v
 COMPLETE
```

Any active process state can enter:

```text
FAULT
```

when a critical safety or system condition occurs.

---

# 3. Drying Process

During DRYING:

```text
Fan      = ON
Heater   = Hysteresis Controlled
Elevator = Continuous OR Interval
Door     = CLOSED
```

The system continuously monitors:

- Rice moisture
- Electrical conductivity
- Hot-air temperature
- Hot-air humidity
- Drying chamber temperature
- Drying chamber humidity

Data is logged to the SD card during the process.

---

# 4. Rice Circulation

The internal elevator is used to circulate rice during drying.

It supports two operating modes.

### Continuous

```text
Elevator = ON
```

The elevator operates continuously during DRYING.

### Interval

Example:

```text
Interval    = 2 minutes
Run Time    = 10 seconds
```

Operation:

```text
Elevator OFF
     |
  2 minutes
     |
Elevator ON
     |
  10 seconds
     |
Elevator OFF
     |
  Repeat
```

Both the interval and run time are configurable.

The elevator is also used during the final discharge stage.

---

# 5. Cooling Process

When the target moisture is reached:

```text
Heater = OFF
Fan    = ON
Elevator = OFF
Door   = CLOSED
```

The fan continues running until the configured cooling temperature is reached.

Example:

```text
Cooling Temperature = 35°C
```

After the cooling temperature is reached, the system enters DISCHARGING.

---

# 6. Discharge Process

The discharge sequence is:

```text
Heater OFF
Fan OFF
Door OPEN
Elevator ON
```

The elevator runs for the configured discharge time.

Example:

```text
Discharge Time = 60 seconds
```

After the timer expires:

```text
Elevator OFF
Door CLOSED
```

The batch is then marked as COMPLETE.

---

# 7. Example Default Settings

Initial configuration:

| Parameter | Default |
|---|---:|
| Target Moisture | 14% |
| Drying Temperature | 50°C |
| Temperature Hysteresis | ±2°C |
| Cooling Temperature | 35°C |
| Circulation Mode | Interval |
| Circulation Interval | 2 min |
| Circulation Run Time | 10 sec |
| Discharge Time | 60 sec |

These values are starting parameters.

Actual values must be validated during rice drying tests.

---

# 8. Hardware

## Main Controller

**Waveshare ESP32-S3 Touch LCD 7B**

Used for:

- Main control
- Touchscreen HMI
- Sensor processing
- Process control
- Data logging
- Local web server
- Wi-Fi communication

---

## Relay Controller

**12 V 8-channel RS485 Modbus RTU relay board**

Logical mapping:

| Relay | Function |
|---|---|
| R1 | Elevator |
| R2 | Heater Control |
| R3 | Fan |
| R4 | Door OPEN |
| R5 | Door CLOSE |
| R6 | Spare |
| R7 | Spare |
| R8 | Spare |

High-power loads must use properly rated switching and protection hardware.

---

## Sensors

### Temperature and Humidity

Two waterproof RS485 Modbus temperature/humidity probes:

- Hot-air/heater area
- Drying chamber

### Moisture and EC

A 0-5 V analog sensor is used for moisture and EC measurement.

An ADS1115 16-bit I2C ADC reads that sensor. The 0-5 V signal must not connect to an ESP32 GPIO.

The exact electrical interface must follow the selected sensor datasheet and the ADS1115 datasheet.

### RTC

TinyRTC is used for:

- Batch timestamps
- Data logging
- Fault timestamps
- Offline timekeeping

---

# 9. Power System

The dryer uses a hybrid solar power system.

Main components:

```text
620 W Solar Panel
       |
       v
ECGSOLAX 2000 W 12 V Hybrid Inverter
       |
       +---- 100 Ah Battery
       |
       +---- AC Power
       |
       v
Dryer Electrical Loads
```

The inverter handles:

- Solar charging
- MPPT
- Battery charging
- Power-source management
- AC/solar power management

The ESP32 does **not** control:

- MPPT
- Battery charging
- Solar charging
- AC/solar source selection
- Inverter operation

The ESP32 is only responsible for the dryer control system.

---

# 10. Software

The firmware is based on:

- ESP-IDF
- C/C++
- FreeRTOS
- LVGL
- Modbus RTU
- RS485
- SD card
- NVS
- Local Wi-Fi

---

# 11. Software Architecture

```text
+--------------------------------------+
|              LVGL HMI                |
+------------------+-------------------+
                   |
+------------------v-------------------+
|          Application Layer           |
|                                      |
| Dryer Controller                     |
| Process Controller                   |
| Safety Manager                       |
| Batch Manager                        |
+------------------+-------------------+
                   |
+------------------v-------------------+
|            Service Layer             |
|                                      |
| Sensor Manager                       |
| Actuator Manager                     |
| Circulation Manager                  |
| Settings Manager                     |
| Logger                               |
| Diagnostics                          |
| Web Server                           |
| RTC Manager                          |
+------------------+-------------------+
                   |
+------------------v-------------------+
|             Driver Layer             |
|                                      |
| RS485 | Modbus | ADS1115 | RTC | SD |
| Relay                                |
+------------------+-------------------+
                   |
+------------------v-------------------+
|              Hardware                |
+--------------------------------------+
```

---

# 12. Project Structure

```text
main/
│
├── app/
│   ├── dryer_controller/
│   ├── process_controller/
│   ├── safety_manager/
│   └── batch_manager/
│
├── services/
│   ├── sensor_manager/
│   ├── circulation_manager/
│   ├── actuator_manager/
│   ├── settings_manager/
│   ├── logger/
│   ├── diagnostics/
│   ├── web_server/
│   └── rtc_manager/
│
├── drivers/
│   ├── rs485/
│   ├── modbus/
│   ├── ads1115/
│   ├── rtc/
│   ├── sd/
│   └── relay/
│
├── ui/
│   ├── screens/
│   ├── components/
│   └── ui_manager/
│
└── main.cpp
```

---

# 13. Process State Machine

The main firmware state machine is:

```text
                 +-------+
                 | IDLE  |
                 +---+---+
                     |
                     v
               +-----+------+
               |  PRECHECK  |
               +-----+------+
                     |
                     v
               +-----+------+
               |   DRYING   |
               +-----+------+
                     |
             Target Moisture
                     |
                     v
               +-----+------+
               |  COOLING   |
               +-----+------+
                     |
            Cooling Temperature
                     |
                     v
             +-------+-------+
             |  DISCHARGING |
             +-------+-------+
                     |
              Discharge Timer
                     |
                     v
               +-----+------+
               |  COMPLETE  |
               +------------+
```

Any active state can transition to:

```text
+--------+
| FAULT  |
+--------+
```

---

# 14. Safety Architecture

Safety is implemented using multiple layers.

```text
Hardware Safety
       |
       v
Safety Manager
       |
       v
Process Controller
       |
       v
Actuator Manager
       |
       v
Physical Actuator
```

The software must never be the only safety mechanism for high-power equipment.

Required hardware safety includes:

- Emergency stop
- Independent thermal protection
- Proper electrical protection
- Proper heater switching
- Motor protection
- Electrical isolation where required

---

# 15. Heater Control

Initial heater control uses hysteresis.

Example:

```text
Target Temperature = 50°C
Hysteresis = 2°C
```

Control:

```text
<= 48°C
    |
    v
Heater ON

48°C - 52°C
    |
    v
Maintain Previous State

>= 52°C
    |
    v
Heater OFF
```

The fan remains ON throughout normal DRYING.

Safety temperature limits always override normal heater control.

---

# 16. HMI

The touchscreen interface will include:

```text
Dashboard
Batch Setup
Drying
Cooling
Discharge
Manual Control
History
Settings
Alarms
System Information
```

The touchscreen is the primary control interface.

---

# 17. Local Web Dashboard

The ESP32 provides a local web dashboard through Wi-Fi.

Initial implementation is read-only.

The dashboard displays:

- Current process state
- Sensor values
- Actuator status
- Batch status
- Faults
- Settings
- Batch history

Example API endpoints:

```text
GET /api/status
GET /api/sensors
GET /api/actuators
GET /api/batch
GET /api/settings
GET /api/history
GET /api/faults
```

The web interface does not directly control actuators in the initial implementation.

---

# 18. Data Logging

Batch data is stored locally on the SD card.

Recommended logging interval:

```text
1 second
```

Example data:

```text
Timestamp
Batch ID
State
Moisture
EC
Hot-Air Temperature
Hot-Air Humidity
Chamber Temperature
Chamber Humidity
Heater
Fan
Elevator
Door
Fault
```

Recommended structure:

```text
/sdcard/
│
├── batches/
│   ├── BATCH-0001.csv
│   ├── BATCH-0002.csv
│   └── ...
│
├── summaries/
│   └── ...
│
├── logs/
│   └── system.log
│
└── config/
    └── calibration.dat
```

---

# 19. Safety Behavior

Default startup state:

```text
Heater   = OFF
Fan      = OFF
Elevator = OFF
Door     = CLOSED
```

Examples of critical faults:

```text
F001 OVER_TEMPERATURE
F002 EMERGENCY_STOP
F003 MOISTURE_SENSOR
F004 HOT_AIR_SENSOR
F005 CHAMBER_SENSOR
F006 MODBUS_COMMUNICATION
F007 ELEVATOR_TIMEOUT
F008 DOOR_TIMEOUT
F009 SD_CARD_ERROR
F010 MAX_DRYING_TIME
F011 RTC_ERROR
F012 INVALID_CONFIGURATION
```

Critical faults must force the system into a safe state.

---

# 20. Power Loss Recovery

The dryer must not automatically restart after power loss.

On restart:

```text
Heater   = OFF
Elevator = OFF
Door     = CLOSED
```

The system should detect whether a previous batch was interrupted.

The operator must manually decide whether to start a new batch or perform an approved recovery procedure.

---

# 21. Development Plan

Development will follow an incremental approach.

```text
1. Project Foundation
2. Hardware Drivers
3. Sensor System
4. Actuator System
5. Safety System
6. Process Controller
7. HMI
8. Data Logger
9. Web Dashboard
10. Full Integration
11. Dry Run
12. Empty Chamber Test
13. Small Rice Test
14. 10 kg Validation
15. Commissioning
```

Each subsystem should be tested before full integration.

---

# 22. Recommended Implementation Order

```text
01. ESP32-S3 Project
02. Display and LVGL
03. SD Card
04. RTC
05. RS485
06. Modbus
07. Temperature/RH Sensors
08. Relay Board
09. ADS1115
10. Moisture/EC Sensor
11. Sensor Manager
12. Actuator Manager
13. Safety Manager
14. Settings Manager
15. Circulation Manager
16. Batch Manager
17. Process Controller
18. Logger
19. HMI Screens
20. Web Dashboard
21. Diagnostics
22. Power Recovery
23. Full Integration
24. Dry Run
25. Rice Testing
26. 10 kg Validation
```

---

# 23. Testing Strategy

Testing will be performed at multiple levels.

## Unit Testing

Test:

- Sensor conversion
- Calibration
- Hysteresis
- Timers
- Settings validation
- Fault detection
- State transitions

## Integration Testing

Test:

- RS485 + sensors
- Modbus + relay board
- Sensors + Process Controller
- Process Controller + Actuator Manager
- Logger + SD
- HMI + Process Controller

## System Testing

Test the complete process:

```text
Manual Load
     |
     v
Start
     |
     v
Precheck
     |
     v
Drying
     |
     v
Cooling
     |
     v
Discharging
     |
     v
Complete
```

---

# 24. Validation Process

Testing should progress from low risk to full production conditions.

```text
Empty Chamber
      |
      v
Small Rice Quantity
      |
      v
Partial Batch
      |
      v
10 kg Batch
```

Important measurements:

- Initial moisture
- Final moisture
- Drying time
- Temperature profile
- Humidity profile
- Moisture uniformity
- Energy consumption
- Circulation performance
- Discharge performance

---

# 25. Documentation

The project documentation is divided into six design documents plus the master plan.

| Document | Purpose |
|---|---|
| `docs/MASTERPLAN.md` | Implementation roadmap and locked build decisions |
| `01-product-requirements.md` | Product scope and functional requirements |
| `02-technical-requirements.md` | Technical and software requirements |
| `03-system-architecture.md` | Complete system architecture |
| `04-hardware-design.md` | Hardware and electrical design |
| `05-software-design.md` | Firmware and software architecture |
| `06-implementation-plan.md` | Development and testing plan |

Start implementation from [`docs/MASTERPLAN.md`](docs/MASTERPLAN.md).

---

# 26. Documentation Map

```text
README.md
   |
   +-- MASTERPLAN
   |       |
   |       +-- How to build from the current repo
   |
   +-- 01 Product Requirements
   |       |
   |       +-- What the system must do
   |
   +-- 02 Technical Requirements
   |       |
   |       +-- Technical constraints
   |
   +-- 03 System Architecture
   |       |
   |       +-- How the system is structured
   |
   +-- 04 Hardware Design
   |       |
   |       +-- Physical hardware
   |
   +-- 05 Software Design
   |       |
   |       +-- Firmware architecture
   |
   +-- 06 Implementation Plan
           |
           +-- Detailed phase checklist
```

---

# 27. Project Status

Current project status:

```text
Requirements              COMPLETE
Technical Requirements    COMPLETE
System Architecture       COMPLETE
Hardware Design           COMPLETE
Software Design           COMPLETE
Implementation Plan       COMPLETE
Firmware Development       NOT STARTED
Hardware Integration       NOT STARTED
Dry Run                    NOT STARTED
Rice Validation             NOT STARTED
```

The documentation phase is complete.

The next phase is hardware and firmware implementation.

---

# 28. Current Design Decisions

The following decisions are considered part of the baseline design.

### Process

- Rice is manually loaded.
- There is no automatic loading stage.
- The elevator is used for internal circulation and discharge.
- DRYING uses continuous fan operation.
- Heater uses initial hysteresis control.
- Rice circulation supports continuous and interval modes.
- Circulation interval and run time are configurable.
- Moisture determines drying completion.
- Fan remains ON during cooling.
- Cooling temperature determines the end of cooling.
- Discharge uses a door and elevator.
- Discharge time is configurable.

### Control

- ESP32-S3 is the main controller.
- ESP-IDF is the firmware framework.
- FreeRTOS manages system tasks.
- LVGL provides the HMI.
- Safety Manager has final software authority.
- Actuator Manager controls physical outputs.
- Process Controller manages the state machine.

### Data

- SD card stores batch history.
- RTC provides offline timestamps.
- Local Wi-Fi provides monitoring.
- Initial web dashboard is read-only.
- No cloud service is required.

### Power

- Hybrid inverter manages the power system.
- ESP32 does not manage MPPT.
- ESP32 does not manage battery charging.
- ESP32 does not manage AC/solar source selection.

---

# 29. Important Hardware Notes

Several hardware details must be confirmed during implementation.

## Moisture/EC Sensor

The exact electrical behavior of the selected 0-5 V sensor must be confirmed.

Do not assume:

```text
0-5 V = Moisture
0-5 V = EC
```

unless the sensor datasheet explicitly defines separate outputs or an appropriate multiplexed interface.

The ADS1115 interface must include:

- Supply and programmable-gain selection
- I2C level shifting when the ADS1115 is powered from 5 V
- Voltage scaling when the ADS1115 is powered from 3.3 V
- Input protection
- Filtering
- Appropriate grounding

---

## ADS1115

The ADS1115 is the 16-bit I2C ADC for the moisture and EC sensor.

It shares the controller I2C bus with the TinyRTC. The default address is 0x48. Moisture and EC use separate inputs when the sensor provides both signals.

The 0-5 V sensor output connects to the ADS1115, not to an ESP32 pin.

---

## Heater

The ESP32 relay board should not directly switch a high-power heater unless the relay and wiring are specifically rated for the heater load.

Recommended architecture:

```text
ESP32
  |
  v
Relay / Control Output
  |
  v
SSR or Contactor
  |
  v
Heater
```

Independent thermal protection is required.

---

# 30. Design Principles

The project follows these principles:

1. Safety first.
2. Keep the control system modular.
3. Use explicit process states.
4. Keep hardware drivers separate from application logic.
5. Centralize actuator control.
6. Centralize safety control.
7. Avoid blocking operations.
8. Operate without internet dependency.
9. Store important batch data locally.
10. Validate sensor readings before using them.
11. Keep the initial system simple.
12. Validate the physical drying process before adding advanced control algorithms.

---

# 31. Future Development

The following features may be added after the baseline system is proven:

- PID heater control
- Advanced moisture control
- Automatic drying recipes
- Door position feedback
- Elevator motor feedback
- Energy monitoring
- Solar generation monitoring
- Battery monitoring
- Inverter communication
- Web-based configuration
- Remote monitoring
- Mobile application
- Advanced drying analytics
- Automated drying optimization

These features should only be added after the core drying process is stable and validated.

---

# 32. Final System Goal

The final system should provide a reliable and repeatable 10 kg rice drying process.

The operator should only need to:

```text
1. Load rice
2. Configure or confirm settings
3. Start the batch
4. Monitor the process
5. Collect the dried rice
```

The dryer should automatically handle:

```text
Precheck
   ↓
Drying
   ↓
Rice Circulation
   ↓
Moisture Monitoring
   ↓
Heater Control
   ↓
Cooling
   ↓
Discharge
   ↓
Batch Logging
   ↓
Complete
```

The primary objective is not maximum automation.

The primary objective is **safe, repeatable, measurable, and maintainable rice drying**.

---

# 33. Project Summary

**Project:** Hybrid Solar Power Rice Dryer

**Capacity:** 10 kg/batch

**Controller:** ESP32-S3

**HMI:** Waveshare ESP32-S3 Touch LCD 7B

**Firmware:** ESP-IDF

**RTOS:** FreeRTOS

**UI:** LVGL

**Communication:** RS485 Modbus RTU

**Data Storage:** SD Card

**Timekeeping:** TinyRTC

**Analog input:** ADS1115 16-bit I2C ADC

**Power System:** ECGSOLAX 2000 W 12 V Hybrid Inverter

**Battery:** 100 Ah

**Solar:** 620 W bifacial solar panel

**Primary Drying Completion:** Rice moisture target

**Cooling Completion:** Configured cooling temperature

**Discharge Completion:** Configured discharge timer

**Internet:** Not required

**Local Monitoring:** Wi-Fi web dashboard

---

# 34. Repository Structure

Recommended repository structure:

```text
hybrid-solar-rice-dryer/
│
├── README.md
│
├── docs/
│   ├── 01-product-requirements.md
│   ├── 02-technical-requirements.md
│   ├── 03-system-architecture.md
│   ├── 04-hardware-design.md
│   ├── 05-software-design.md
│   └── 06-implementation-plan.md
│
├── firmware/
│   ├── platformio.ini
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   └── main/
│
├── hardware/
│   ├── schematics/
│   ├── wiring/
│   ├── pcb/
│   └── mechanical/
│
├── data/
│   ├── calibration/
│   └── test-results/
│
└── tests/
    ├── unit/
    ├── integration/
    └── system/
```

---

# 35. Getting Started

Development should begin with:

```text
1. Verify Waveshare ESP32-S3 Touch LCD 7B
2. Create ESP-IDF project
3. Initialize LVGL
4. Verify touchscreen
5. Verify SD card
6. Verify RTC
7. Implement RS485
8. Implement Modbus
9. Connect sensors
10. Implement relay communication
```

Do not connect the high-power heater to the system until the low-voltage control system and safety functions have been tested.

---

# 36. Next Development Step

Follow the execution order in:

[`docs/MASTERPLAN.md`](docs/MASTERPLAN.md)

The first firmware milestone is:

```text
ESP32-S3
   +
LVGL
   +
Touchscreen
   +
SD Card
   +
RTC
```

Once this foundation is stable, implement the RS485, Modbus, and ADS1115 layers, then continue through the master plan phases.