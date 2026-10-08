# 03 - System Architecture

## 1. Architecture Overview

The Solar Hybrid Rice Dryer uses an ESP32-S3 as the main machine controller.

The architecture is divided into five major subsystems:

1. Power System
2. Control System
3. Sensor System
4. Actuator System
5. HMI and Data System

The ESP32-S3 coordinates the drying process, but the hybrid inverter independently manages the electrical power system.

```text
                         POWER SYSTEM
                              |
        +---------------------+----------------------+
        |                                            |
  Solar Panel                                  AC Input
  620 W Bifacial                                    |
        |                                            |
        +--------------------+-----------------------+
                             |
                             v
                 ECGSOLAX Hybrid Inverter
                   2000 W / 12 V / MPPT
                             |
                             v
                      12 V Power System
                             |
              +--------------+--------------+
              |                             |
              v                             v
        Control Electronics            Dryer Loads
        ESP32 / Sensors / HMI       Heater / Fan / Motor
```

The ESP32 does not control the inverter's:

- MPPT
- Battery charging
- Solar charging
- AC/solar source selection
- Battery management
- Inverter operation

These functions remain inside the hybrid inverter and its associated power system.

---

# 2. High-Level System Architecture

```text
                         ┌─────────────────────────┐
                         │      POWER SYSTEM       │
                         │                         │
                         │  620 W Solar Panel      │
                         │          │              │
                         │          v              │
                         │  ECGSOLAX Hybrid        │
                         │  Inverter / MPPT        │
                         │          │              │
                         │          v              │
                         │      12 V System        │
                         └───────────┬─────────────┘
                                     │
                                     v
┌───────────────────────────────────────────────────────────┐
│                  ESP32-S3 CONTROL SYSTEM                  │
│                                                           │
│  ┌───────────────┐      ┌─────────────────────────────┐  │
│  │ Sensor Manager│─────>│     Process Controller      │  │
│  └───────────────┘      └──────────────┬──────────────┘  │
│                                         │                 │
│                                         v                 │
│  ┌───────────────┐      ┌─────────────────────────────┐  │
│  │ Safety Manager│─────>│      Actuator Manager       │  │
│  └───────────────┘      └──────────────┬──────────────┘  │
│                                         │                 │
│                                         v                 │
│  ┌───────────────┐      ┌─────────────────────────────┐  │
│  │ Batch Manager │      │       Data Logger           │  │
│  └───────────────┘      └─────────────────────────────┘  │
│                                                           │
└───────────────────────┬───────────────────────────────────┘
                        │
          ┌─────────────┼─────────────┐
          │             │             │
          v             v             v
       Sensors       Actuators       HMI
          │             │             │
          v             v             v
      RS485/ADC      RS485 Relay    LCD/Touch
```

---

# 3. Main Hardware Architecture

The primary controller is the Waveshare ESP32-S3-Touch-LCD-7B.

It provides:

- ESP32-S3 processing
- Touchscreen HMI
- Wi-Fi
- SD card interface
- GPIO
- Communication interfaces

The ESP32-S3 communicates with external devices through:

- RS485
- Analog input interface
- RTC interface
- Relay controller
- SD card
- Display and touch interface
- Wi-Fi

---

# 4. Sensor Architecture

The sensor subsystem consists of:

- Hot-air temperature/humidity sensor
- Drying-chamber temperature/humidity sensor
- Rice moisture/EC sensor
- RTC

Architecture:

```text
                   ┌───────────────────────┐
                   │       ESP32-S3        │
                   │                       │
                   │    Sensor Manager     │
                   └───────────┬───────────┘
                               │
                  ┌────────────┴────────────┐
                  │                         │
                  v                         v
             RS485 Modbus                 ADC
                  │                         │
          ┌───────┴────────┐                │
          │                │                │
          v                v                v
     Hot-Air Sensor   Chamber Sensor   Moisture / EC
     Temp + RH        Temp + RH        Sensor
```

---

# 5. RS485 Sensor Network

The temperature and humidity sensors use RS485 Modbus RTU.

Example network:

```text
ESP32-S3
   |
   | RS485
   |
   +---- Address 01
   |     Hot-Air Temp/RH
   |
   +---- Address 02
         Chamber Temp/RH
```

Each device must have a unique Modbus address.

The Sensor Manager is responsible for:

- Polling sensors
- Validating responses
- Checking CRC
- Detecting communication failures
- Applying retries
- Converting raw values
- Providing validated sensor data to the Process Controller

---

# 6. Moisture Sensor Architecture

The moisture/EC sensor provides an analog signal.

Because the selected sensor uses a 0-5 V output, the signal must pass through an analog interface before reaching the ESP32-S3.

```text
Moisture / EC Sensor
        |
        | 0-5 V
        v
┌────────────────────┐
│ Analog Interface   │
│                    │
│ Voltage Scaling    │
│ Protection         │
│ Filtering          │
└─────────┬──────────┘
          |
          | Safe ADC Voltage
          v
      ESP32-S3 ADC
          |
          v
    Sensor Manager
          |
          v
   Calibration Model
          |
          v
   Moisture / EC Value
```

The ESP32-S3 ADC must never receive a voltage above its permitted input range.

The exact analog interface shall be finalized based on the selected sensor's electrical specifications.

If the final sensor provides separate moisture and EC analog outputs, each output shall use its own ADC input.

---

# 7. RTC Architecture

The RTC provides independent timekeeping.

```text
          ESP32-S3
              |
              |
             I2C
              |
              v
          TinyRTC
              |
              v
       Date / Time Data
```

The RTC is used for:

- Batch timestamps
- Sensor logs
- Fault logs
- System event logs
- File naming
- Historical records

---

# 8. Actuator Architecture

The actuator subsystem contains:

- Heater
- Fan
- Rice circulation elevator
- Discharge door

The ESP32 communicates with the RS485 relay controller.

```text
                    ESP32-S3
                        |
                        | RS485 Modbus
                        v
              ┌────────────────────┐
              │ 8-Channel Relay    │
              │ Modbus Controller  │
              └─────────┬──────────┘
                        |
       +----------------+----------------+
       |                |                |
       v                v                v
    Elevator          Heater            Fan
       |
       |
       v
 Rice Circulation
```

The discharge door uses two relay outputs:

```text
R4 -> Door OPEN
R5 -> Door CLOSE
```

The controller shall enforce an interlock so OPEN and CLOSE cannot be activated simultaneously.

---

# 9. Relay Architecture

Initial relay mapping:

```text
R1 = Rice Circulation Elevator
R2 = Heater Control
R3 = Fan
R4 = Discharge Door OPEN
R5 = Discharge Door CLOSE
R6 = Spare
R7 = Spare
R8 = Spare
```

The heater relay should normally control an appropriately rated SSR or contactor.

The relay board should not be assumed to directly switch the final heater load unless its electrical ratings are confirmed to be suitable.

---

# 10. Rice Circulation Architecture

The elevator is an internal rice circulation mechanism.

The operator manually loads rice into the drying chamber.

The elevator does not perform automated loading.

During DRYING, the Rice Circulation Manager controls the elevator using one of two modes.

## Continuous Mode

```text
DRYING
   |
   v
Elevator ON
   |
   v
Continue until drying completes
```

## Interval Mode

```text
                ┌───────────────────┐
                │ Circulation Timer │
                └─────────┬─────────┘
                          |
                          v
                     Elevator ON
                          |
                          v
                 Run for configured
                    run duration
                          |
                          v
                     Elevator OFF
                          |
                          v
                Wait configured interval
                          |
                          +───────────────┐
                                          |
                                          v
                                    Repeat cycle
```

The following parameters are configurable:

- Circulation mode
- Circulation interval
- Circulation run duration

---

# 11. Drying Airflow Architecture

The airflow system consists of:

- Heater
- Fan
- Drying chamber
- Hot-air sensor

Conceptually:

```text
                 ┌───────────────┐
                 │    Heater     │
                 └───────┬───────┘
                         |
                         v
                 Heated Air Flow
                         |
                         v
                ┌─────────────────┐
                │ Drying Chamber  │
                │                 │
                │  Rice           │
                │  Circulation    │
                └────────┬────────┘
                         |
                         v
                       Fan
                         |
                         v
                    Air Exhaust
```

The exact physical airflow direction depends on the final mechanical design.

The hot-air sensor should be positioned so that it provides useful feedback for heater control and monitoring.

---

# 12. Process Control Architecture

The Process Controller is the main software component responsible for coordinating the drying process.

```text
Sensors
   |
   v
Sensor Manager
   |
   v
Validated Sensor Data
   |
   v
Process Controller
   |
   +-------------------+
   |                   |
   v                   v
Safety Manager    Batch Manager
   |
   v
Actuator Manager
   |
   v
Relay Controller
   |
   v
Physical Actuators
```

The Process Controller shall not directly control hardware outputs.

All actuator requests shall pass through the Safety Manager and Actuator Manager.

---

# 13. Safety Architecture

Safety has final authority over actuator commands.

```text
                 Process Request
                       |
                       v
               ┌───────────────┐
               │ Safety Manager│
               └───────┬───────┘
                       |
               Safety Conditions OK?
                  /           \
                YES            NO
                 |              |
                 v              v
        Actuator Manager      FAULT
                 |
                 v
          Physical Output
```

Critical safety conditions include:

- Over-temperature
- Invalid temperature readings
- Critical sensor failure
- RS485 communication failure
- Elevator timeout
- Door movement timeout
- Relay communication failure

For a critical heater fault:

```text
Heater -> OFF
System -> FAULT
```

---

# 14. Main Process State Machine

The complete process state machine is:

```text
                         ┌─────────────┐
                         │    IDLE     │
                         └──────┬──────┘
                                |
                         Start Batch
                                |
                                v
                       ┌────────────────┐
                       │    PRECHECK    │
                       └───────┬────────┘
                               |
                         Precheck OK
                               |
                               v
                       ┌────────────────┐
                       │     DRYING     │
                       └───────┬────────┘
                               |
                    Target Moisture Reached
                               |
                               v
                       ┌────────────────┐
                       │    COOLING     │
                       └───────┬────────┘
                               |
                    Cooling Temperature Reached
                               |
                               v
                       ┌────────────────┐
                       │  DISCHARGING   │
                       └───────┬────────┘
                               |
                       Discharge Complete
                               |
                               v
                       ┌────────────────┐
                       │    COMPLETE    │
                       └────────────────┘
```

Any active process state can transition to:

```text
FAULT
```

---

# 15. DRYING State Architecture

During DRYING:

```text
                 DRYING
                   |
        +----------+----------+
        |          |          |
        v          v          v
      Fan ON    Heater      Elevator
                 Control      Control
                   |            |
                   v            v
               Hysteresis   Continuous
               Control      or Interval
```

The controller continuously evaluates:

- Rice moisture
- Hot-air temperature
- Chamber temperature
- Humidity
- EC
- Safety conditions
- Drying time

### Heater

The heater uses hysteresis.

Example:

```text
Target = 50°C
Hysteresis = 2°C

<= 48°C -> Heater ON
>= 52°C -> Heater OFF
```

### Fan

The fan remains ON throughout DRYING.

### Elevator

The elevator operates in:

- Continuous mode
- Interval mode

---

# 16. Drying Completion Logic

Moisture is the primary drying completion parameter.

```text
              Sensor Reading
                    |
                    v
              Moisture Value
                    |
                    v
             <= Target Moisture?
                /          \
              NO            YES
              |              |
              v              v
        Continue Drying   Heater OFF
                             |
                             v
                           COOLING
```

The system should also enforce:

- Minimum drying time
- Maximum drying time
- Maximum safe temperature

These are protective conditions and should not replace the primary moisture target.

---

# 17. Cooling Architecture

Cooling starts when target moisture is reached.

```text
                    COOLING
                       |
              +--------+--------+
              |                 |
              v                 v
          Heater OFF         Fan ON
                                |
                                v
                       Monitor Temperature
                                |
                                v
                     Temperature <= Threshold
                                |
                                v
                           DISCHARGING
```

The cooling temperature is configurable.

Example:

```text
Cooling Threshold = 35°C
```

---

# 18. Discharge Architecture

The discharge sequence is:

```text
Cooling Complete
       |
       v
Door OPEN
       |
       v
Elevator ON
       |
       v
Discharge Timer
       |
       v
Elevator OFF
       |
       v
Door CLOSE
       |
       v
COMPLETE
```

The discharge timer is configurable.

Example:

```text
Discharge Time = 60 seconds
```

The elevator used for discharge is the same internal rice circulation mechanism.

---

# 19. HMI Architecture

The touchscreen is the primary local user interface.

```text
             Touchscreen
                  |
                  v
             HMI Manager
                  |
                  v
          Application Layer
                  |
        +---------+---------+
        |                   |
        v                   v
 Process Controller    Settings Manager
        |
        v
   Safety Manager
        |
        v
 Actuator Manager
```

The HMI shall not directly manipulate relay outputs.

---

# 20. HMI Screen Architecture

The application shall provide:

```text
Dashboard
    |
    +-- Batch Setup
    |
    +-- Drying Process
    |
    +-- Manual Control
    |
    +-- History
    |
    +-- Settings
    |
    +-- Alarms
    |
    +-- System Information
```

The Dashboard shall provide real-time information.

The Drying Process screen shall focus on the active batch.

The Manual Control screen shall be restricted by safety logic.

---

# 21. Data Architecture

The system uses the SD card as the primary local data store.

```text
Sensor Data
     |
     v
Sensor Manager
     |
     v
Process Controller
     |
     +------------------+
     |                  |
     v                  v
Batch Manager       Data Logger
                        |
                        v
                     SD Card
```

The logger shall store:

- Raw process data
- Batch data
- Fault data
- System events

---

# 22. Batch Data Flow

```text
Batch Start
    |
    v
Create Batch ID
    |
    v
Record Initial Conditions
    |
    v
Start Process Logging
    |
    v
Record Sensor + Actuator Data
    |
    v
Moisture Target Reached
    |
    v
Record Cooling Data
    |
    v
Discharge
    |
    v
Record Final Conditions
    |
    v
Close Batch File
    |
    v
Batch History
```

---

# 23. SD Card Storage Architecture

Recommended structure:

```text
/data
    |
    +-- /batches
    |      |
    |      +-- batch_YYYYMMDD_001.csv
    |
    +-- /logs
    |      |
    |      +-- process logs
    |
    +-- /faults
    |      |
    |      +-- fault logs
    |
    +-- /system
    |      |
    |      +-- system logs
    |
    +-- /exports
```

The exact filesystem structure can be refined during implementation.

---

# 24. Local Web Architecture

The ESP32-S3 provides a local Wi-Fi Access Point.

```text
                ESP32-S3
                    |
                    v
              Wi-Fi Access Point
                    |
          +---------+---------+
          |                   |
          v                   v
      Smartphone           Laptop
          |                   |
          +---------+---------+
                    |
                    v
              Web Dashboard
```

Example:

```text
192.168.4.1
```

The web dashboard is initially read-only.

---

# 25. Web Data Flow

```text
Physical Sensors
      |
      v
Sensor Manager
      |
      v
Application State
      |
      v
Web API
      |
      v
Web Dashboard
```

The web server shall not directly access hardware drivers to control actuators.

The web server reads validated application data.

---

# 26. Web API Architecture

Recommended API structure:

```text
/api/status
/api/sensors
/api/actuators
/api/alarms
/api/batches
/api/batches/{id}
/api/logs/system
/api/logs/faults
/api/export/{file}
```

The API shall provide structured data suitable for a local browser dashboard.

Large CSV files should be streamed from the SD card.

---

# 27. Configuration Architecture

Persistent settings shall be stored in ESP32 non-volatile storage.

```text
                 Settings Screen
                        |
                        v
                Settings Manager
                        |
                        v
                       NVS
                        |
                        v
              Process Controller
```

Settings include:

- Target moisture
- Drying temperature
- Temperature hysteresis
- Cooling temperature
- Minimum drying time
- Maximum drying time
- Circulation mode
- Circulation interval
- Circulation run duration
- Discharge time
- Sensor calibration
- Modbus configuration
- Wi-Fi configuration

---

# 28. Software Layer Architecture

The software architecture shall use layered responsibilities.

```text
┌────────────────────────────────────────────┐
│                 UI LAYER                   │
│          LVGL / Touchscreen / Web          │
└──────────────────────┬─────────────────────┘
                       |
┌──────────────────────v─────────────────────┐
│             APPLICATION LAYER              │
│ Dryer Controller / Process / Batch /       │
│ Safety                                     │
└──────────────────────┬─────────────────────┘
                       |
┌──────────────────────v─────────────────────┐
│              SERVICE LAYER                 │
│ Sensors / Circulation / Actuators /        │
│ Logging / Settings / Diagnostics            │
└──────────────────────┬─────────────────────┘
                       |
┌──────────────────────v─────────────────────┐
│              DRIVER LAYER                  │
│ RS485 / Modbus / ADC / RTC / SD / Relay   │
└──────────────────────┬─────────────────────┘
                       |
┌──────────────────────v─────────────────────┐
│                HARDWARE                    │
│ ESP32 / Sensors / Relay / Motors / HMI    │
└────────────────────────────────────────────┘
```

---

# 29. FreeRTOS Architecture

The firmware should use separate FreeRTOS tasks for major functions.

Recommended architecture:

```text
                     FreeRTOS
                        |
       +----------------+----------------+
       |                |                |
       v                v                v
 Safety Task       Control Task      Sensor Task
       |                |                |
       +----------------+----------------+
                        |
       +----------------+----------------+
       |                |                |
       v                v                v
  Modbus Task       HMI Task        Logger Task
                        |
                        v
                   Web Task
```

Tasks shall communicate using appropriate FreeRTOS mechanisms such as:

- Queues
- Event groups
- Mutexes
- Notifications
- Shared state protected by synchronization

---

# 30. Control and Safety Priority

Safety shall have higher priority than normal process control.

The logical hierarchy is:

```text
User Request
     |
     v
Process Logic
     |
     v
Safety Validation
     |
     v
Actuator Command
     |
     v
Hardware
```

The system shall never allow:

```text
UI -> Direct GPIO
Web -> Direct Relay
Sensor -> Direct Relay
```

All actions must pass through the appropriate application and safety layers.

---

# 31. Power Architecture

The electrical architecture is separated into power management and machine control.

```text
             620 W Solar Panel
                    |
                    v
          ECGSOLAX Hybrid Inverter
                    |
          +---------+---------+
          |                   |
          v                   v
       Battery             AC Output
          |                   |
          +---------+---------+
                    |
                    v
             Power Distribution
                    |
       +------------+------------+
       |                         |
       v                         v
 Control Electronics         Dryer Loads
       |                         |
       v                         v
 ESP32 / Sensors          Heater / Fan / Motor
```

The ESP32 is not responsible for selecting the power source.

---

# 32. Electrical Isolation

The architecture shall separate:

- Low-voltage logic
- Analog sensor signals
- RS485 communication
- Relay control
- Motor circuits
- Heater power

High-power switching components shall be appropriately isolated from the ESP32 control electronics.

Particular attention shall be given to:

- Heater switching
- Elevator motor switching
- Fan motor switching
- Door actuator switching

---

# 33. Fault Architecture

Fault handling follows:

```text
Sensor / Hardware
       |
       v
Fault Detection
       |
       v
Safety Manager
       |
       v
Fault State
       |
       +---------> Heater OFF
       |
       +---------> Safe Actuator State
       |
       +---------> Fault Log
       |
       +---------> HMI Alarm
```

Critical faults shall prevent unsafe process continuation.

---

# 34. Power Recovery Architecture

After an unexpected restart:

```text
ESP32 Boot
    |
    v
Initialize Hardware
    |
    v
Set Safe Outputs
    |
    v
Initialize Sensors
    |
    v
Check Previous Batch
    |
    v
Notify Operator
    |
    v
IDLE / Recovery Decision
```

The system shall not automatically restart heating.

---

# 35. Timing Architecture

The system shall use non-blocking timing mechanisms.

Recommended approach:

- `millis()`-style timing where appropriate
- FreeRTOS timers
- FreeRTOS task delays
- Event-driven state changes

The control system shall avoid long blocking delays.

This is especially important for:

- Heater control
- Elevator interval timing
- Discharge timing
- Sensor polling
- HMI updates
- SD logging
- Web requests

---

# 36. Recommended Module Architecture

The main firmware modules should include:

```text
app/
├── dryer_controller
├── process_controller
├── safety_manager
└── batch_manager

services/
├── sensor_manager
├── circulation_manager
├── actuator_manager
├── settings_manager
├── logger
├── diagnostics
├── web_server
└── rtc_manager

drivers/
├── rs485
├── modbus
├── adc
├── rtc
├── sd
└── relay

ui/
├── ui_manager
├── dashboard
├── batch_setup
├── drying_process
├── manual_control
├── history
├── settings
├── alarms
└── system_info
```

---

# 37. Main Data Flow

The complete control and monitoring flow is:

```text
                    SENSOR INPUT
                         |
                         v
                  Sensor Manager
                         |
                         v
                Validated Sensor Data
                         |
                         v
                 Process Controller
                         |
               +---------+---------+
               |                   |
               v                   v
        Safety Manager        Batch Manager
               |
               v
        Actuator Manager
               |
               v
       Modbus Relay Controller
               |
       +-------+-------+-------+
       |       |       |       |
       v       v       v       v
    Heater    Fan   Elevator  Door
```

At the same time:

```text
Sensor Data
     |
     +------> HMI
     |
     +------> Data Logger
     |
     +------> Web API
     |
     +------> Diagnostics
```

---

# 38. Complete Batch Flow

The complete machine operation is:

```text
1. Operator manually loads rice
              |
              v
2. Operator configures batch
              |
              v
3. Operator starts batch
              |
              v
4. PRECHECK
              |
              v
5. DRYING
       |
       +--> Fan ON
       |
       +--> Heater controlled by hysteresis
       |
       +--> Elevator continuous or interval
       |
       +--> Sensors monitored
       |
       +--> Data logged
              |
              v
6. Target moisture reached
              |
              v
7. Heater OFF
              |
              v
8. COOLING
       |
       +--> Fan ON
       |
       +--> Temperature monitored
              |
              v
9. Cooling threshold reached
              |
              v
10. DISCHARGING
       |
       +--> Door OPEN
       |
       +--> Elevator ON
       |
       +--> Discharge timer
       |
       +--> Elevator OFF
       |
       +--> Door CLOSE
              |
              v
11. COMPLETE
```

---

# 39. Architecture Principles

The system shall follow these principles:

### Principle 1: Safety First

Safety logic has final authority over actuator operation.

### Principle 2: Separation of Concerns

UI, process logic, safety, services, and hardware drivers remain separated.

### Principle 3: No Direct Hardware Control from UI

The HMI requests actions through the application layer.

### Principle 4: No Direct Hardware Control from Web

The web dashboard initially provides monitoring only.

### Principle 5: Sensors Do Not Directly Control Actuators

Sensor values are processed by the Process Controller and validated by the Safety Manager.

### Principle 6: Non-Blocking Operation

Long-running hardware operations must not block the main control system.

### Principle 7: Local-First Operation

The machine must remain fully functional without internet connectivity.

### Principle 8: Power Management Is External

The hybrid inverter manages the solar and battery power system.

### Principle 9: Manual Loading

Rice is manually loaded into the drying chamber.

### Principle 10: Elevator Has Two Functions

The elevator provides:

- Internal rice circulation during drying
- Rice movement during discharge

---

# 40. Architecture Summary

The Solar Hybrid Rice Dryer is built around an ESP32-S3 control architecture.

The controller:

- Reads temperature and humidity through RS485 Modbus.
- Reads moisture/EC through an analog interface.
- Maintains time using the RTC.
- Controls the heater, fan, elevator, and discharge door through the relay controller.
- Runs the drying state machine.
- Uses moisture as the primary drying completion condition.
- Uses temperature as the cooling completion condition.
- Supports continuous or interval rice circulation.
- Records process data to the SD card.
- Provides touchscreen control.
- Provides a local read-only web dashboard.
- Detects faults and transitions to a safe state.

The hybrid inverter remains responsible for the solar, battery, AC, and MPPT power-management functions.

The overall architecture is designed to remain modular so that additional capabilities such as PID temperature control, advanced sensor processing, inverter monitoring, and web-based control can be added later without restructuring the core machine-control architecture.