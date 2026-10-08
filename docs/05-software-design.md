# 05 Software Design

## 1. Purpose

This document defines the software architecture and implementation design for the 10 kg/batch Hybrid Solar Power Rice Dryer.

The software runs on the Waveshare ESP32-S3 Touch LCD 7B using:

- ESP-IDF
- C/C++
- FreeRTOS
- LVGL
- Modbus RTU over RS485
- SD card storage
- RTC
- NVS for persistent settings
- Local Wi-Fi web server

The software is responsible for:

- Dryer process control
- Sensor acquisition
- Heater control
- Fan control
- Rice circulation
- Discharge control
- Safety monitoring
- HMI operation
- Batch management
- Data logging
- Configuration management
- Diagnostics
- Local web monitoring

The software does not control the inverter, MPPT, battery charging, or power-source selection.

---

# 2. Software Architecture

The software follows a layered architecture.

```text
+--------------------------------------------------+
|                    HMI Layer                     |
|              LVGL Touchscreen UI                 |
+--------------------------------------------------+
|              Application Layer                  |
| Dryer Controller | Process | Safety | Batch     |
+--------------------------------------------------+
|                Service Layer                    |
| Sensors | Actuators | Circulation | Logger      |
| Settings | Diagnostics | Web | RTC              |
+--------------------------------------------------+
|                 Driver Layer                    |
| RS485 | Modbus | ADS1115 | RTC | SD | Relay    |
+--------------------------------------------------+
|              ESP-IDF / FreeRTOS                 |
+--------------------------------------------------+
|                 Hardware                        |
| ESP32-S3 | Sensors | Relay | SD | RTC           |
+--------------------------------------------------+
```

The application layer determines what the dryer should do.

The service layer provides reusable functionality.

The driver layer communicates directly with hardware.

---

# 3. Software Modules

## 3.1 Application Modules

```text
app/
├── dryer_controller/
├── process_controller/
├── safety_manager/
└── batch_manager/
```

### Dryer Controller

Responsible for overall system coordination.

Responsibilities:

- System startup
- Main process coordination
- Module initialization
- Process state coordination
- Fault coordination
- Shutdown handling

---

### Process Controller

Responsible for the dryer state machine.

States:

```text
IDLE
  |
  v
PRECHECK
  |
  v
DRYING
  |
  v
COOLING
  |
  v
DISCHARGING
  |
  v
COMPLETE
```

Any active state can transition to:

```text
FAULT
```

The Process Controller does not directly manipulate GPIO or relay outputs.

It sends commands through the Actuator Manager.

---

### Safety Manager

The Safety Manager has the highest software priority.

Responsibilities:

- Monitor critical sensor conditions
- Monitor actuator faults
- Monitor communication faults
- Monitor temperature limits
- Monitor emergency stop status
- Prevent unsafe heater operation
- Force safe actuator states
- Generate fault conditions

Safety Manager output has priority over normal process commands.

---

### Batch Manager

Responsible for batch lifecycle and batch data.

Responsibilities:

- Create batch
- Generate batch ID
- Store batch start time
- Track batch state
- Track drying duration
- Track cooling duration
- Track discharge duration
- Detect batch completion
- Store batch summary
- Prepare batch log file

The operator manually loads the rice before starting the batch.

There is no software-controlled loading stage.

---

# 4. Service Modules

```text
services/
├── sensor_manager/
├── circulation_manager/
├── actuator_manager/
├── settings_manager/
├── logger/
├── diagnostics/
├── web_server/
└── rtc_manager/
```

---

## 4.1 Sensor Manager

The Sensor Manager provides a common interface for all sensors.

Sensors include:

- Moisture sensor
- EC sensor
- Hot-air temperature
- Hot-air relative humidity
- Drying chamber temperature
- Drying chamber relative humidity
- RTC
- Hardware safety inputs where available

The Sensor Manager is responsible for:

- Reading sensors
- Applying filtering
- Validating readings
- Detecting stale readings
- Detecting out-of-range readings
- Providing the latest valid values

Example sensor structure:

```cpp
struct SensorData {
    float moisture;
    float ec;

    float hotAirTemperature;
    float hotAirHumidity;

    float chamberTemperature;
    float chamberHumidity;

    bool moistureValid;
    bool ecValid;
    bool hotAirTempValid;
    bool hotAirHumidityValid;
    bool chamberTempValid;
    bool chamberHumidityValid;

    uint32_t timestamp;
};
```

---

# 5. RS485 and Modbus Software

The two temperature/humidity probes communicate using Modbus RTU over RS485.

Software structure:

```text
Sensor Manager
      |
      v
Modbus Manager
      |
      v
RS485 Driver
      |
      v
UART
      |
      v
RS485 Bus
```

The software should support:

- Device addressing
- Register reading
- CRC validation
- Communication timeout
- Retry handling
- Communication error counters
- Device offline detection

The system must not block the main process controller while waiting for a Modbus response.

---

# 6. Analog Moisture and EC Interface

The moisture/EC sensor provides an analog signal. An ADS1115 reads that signal over I2C and returns a 16-bit code.

```text
Sensor
   |
   v
Signal Conditioning
   |
   v
ADS1115
   |
   | I2C
   v
ADS1115 Driver
   |
   v
Sensor Manager
```

The ADS1115 driver selects the input channel and gain, then provides the raw code and the converted voltage.

The Sensor Manager converts that voltage into engineering units.

Example:

```cpp
float convertVoltageToMoisture(float voltage);
float convertVoltageToEC(float voltage);
```

The exact conversion must follow the selected sensor datasheet and calibration results.

The software must not assume that a single 0-5 V signal represents two independent measurements unless confirmed by the sensor documentation.

---

# 7. Moisture Calibration

The system must support user calibration.

Calibration data is stored in NVS.

Example:

```text
Calibration Point 1
ADS1115 Voltage -> Reference Moisture

Calibration Point 2
ADS1115 Voltage -> Reference Moisture
```

The initial implementation may use linear interpolation.

Future versions may support multi-point calibration.

Calibration must be performed using reference moisture measurements.

---

# 8. EC Measurement

EC is monitored as a process parameter.

EC does not directly determine batch completion in the initial design.

The system shall:

- Read EC
- Validate EC
- Display EC
- Log EC
- Detect abnormal readings
- Store calibration parameters

EC processing must remain separate from moisture processing.

---

# 9. Actuator Manager

The Actuator Manager provides the only normal software interface for controlling physical outputs.

```text
Process Controller
        |
        v
Actuator Manager
        |
        v
Relay Driver
        |
        v
RS485 Relay Board
```

Example actuator interface:

```cpp
void setHeater(bool state);
void setFan(bool state);
void setElevator(bool state);

void openDoor();
void closeDoor();

void stopAllActuators();
```

The Process Controller should not directly write relay GPIO or Modbus commands.

This keeps hardware control centralized.

---

# 10. Relay Control Mapping

The software uses the following logical outputs:

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

The physical relay implementation must enforce mutually exclusive commands where required.

For example:

```text
Door OPEN  -> R4 ON
Door CLOSE -> R5 ON
```

R4 and R5 must never be activated simultaneously.

---

# 11. Process State Machine

The main state machine is:

```text
IDLE
 |
 | Start Batch
 v
PRECHECK
 |
 | All Conditions Valid
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
 |
 | Reset
 v
IDLE
```

Any active state can transition to:

```text
FAULT
```

---

# 12. IDLE State

In IDLE:

```text
Heater   = OFF
Fan      = OFF
Elevator = OFF
Door     = CLOSED
```

The HMI displays:

- System status
- Sensor values
- Current settings
- Start Batch button
- Settings button
- History button
- Manual Control button

The operator manually loads rice into the drying chamber.

The software does not automatically control loading.

---

# 13. PRECHECK State

PRECHECK validates that the system is ready to start.

Checks may include:

- Temperature sensor available
- Humidity sensors available
- Moisture sensor available
- Required Modbus devices available
- RTC available
- SD card available
- No active fault
- Emergency stop released
- Door in safe position
- Heater initially OFF
- Elevator initially OFF
- Fan initially OFF

If all required conditions are valid:

```text
PRECHECK -> DRYING
```

If a critical condition fails:

```text
PRECHECK -> FAULT
```

---

# 14. DRYING State

DRYING is the main rice drying process.

During DRYING:

```text
Fan      = ON
Heater   = Hysteresis Controlled
Elevator = Continuous OR Interval
Door     = CLOSED
```

The software continuously monitors:

- Rice moisture
- EC
- Hot-air temperature
- Hot-air humidity
- Chamber temperature
- Chamber humidity

Data logging is active.

---

# 15. Heater Control

The initial heater controller uses hysteresis.

Example:

```text
Target Temperature = 50°C
Hysteresis = 2°C
```

Control limits:

```text
48°C -> Heater ON
52°C -> Heater OFF
```

Between the limits, the previous heater state is maintained.

Example:

```cpp
if (temperature <= targetTemperature - hysteresis) {
    heaterOn();
}
else if (temperature >= targetTemperature + hysteresis) {
    heaterOff();
}
```

The exact implementation must also include safety limits.

---

# 16. Heater Safety

Normal temperature control is separate from safety temperature limits.

Example:

```text
Normal Target       = 50°C
Normal Hysteresis   = ±2°C
Maximum Safe Temp   = Configurable
```

If the maximum safe temperature is exceeded:

```text
Heater = OFF
FAULT  = ACTIVE
```

Hardware thermal protection must remain active even if the ESP32 software fails.

The software must never rely on software temperature control as the only heater protection.

---

# 17. Fan Control

The fan is ON throughout DRYING.

The fan is not controlled by heater state.

Example:

```text
Heater ON  -> Fan ON
Heater OFF -> Fan ON
```

This ensures continuous airflow through the drying process.

---

# 18. Rice Circulation Manager

The Elevator has two operating functions:

1. Rice circulation during DRYING
2. Rice discharge after drying

The Circulation Manager controls the DRYING circulation function.

The operator can configure:

```text
Continuous
OR
Interval
```

---

# 19. Continuous Circulation

When continuous mode is selected:

```text
Elevator = ON
```

The elevator remains active while DRYING.

The software must still monitor:

- Motor timeout
- Relay status where available
- Fault conditions
- Process state

---

# 20. Interval Circulation

When interval mode is selected, two parameters are used:

```text
Circulation Interval
Circulation Run Time
```

Example:

```text
Interval = 2 minutes
Run Time = 10 seconds
```

Operation:

```text
00:00  Elevator OFF
02:00  Elevator ON
02:10  Elevator OFF
04:10  Elevator ON
04:20  Elevator OFF
...
```

The timing must be non-blocking.

The software must not use long `delay()` calls for circulation control.

---

# 21. Circulation Configuration

Recommended settings:

| Setting | Example |
|---|---:|
| Circulation Mode | Interval |
| Interval | 2 min |
| Run Time | 10 sec |

The values must be configurable through Settings.

The system must validate that:

```text
Run Time < Interval
```

Invalid settings must not be accepted.

---

# 22. Circulation Fault

If the elevator runs beyond its configured maximum runtime:

```text
Elevator = OFF
Fault = ELEVATOR_TIMEOUT
```

The process must stop safely.

The system should also detect:

- Repeated elevator failures
- Invalid relay commands
- Door/elevator command conflicts
- Unexpected actuator state

---

# 23. Drying Completion

The primary drying completion condition is moisture.

Example:

```text
Target Moisture = 14%
```

When:

```text
Moisture <= Target Moisture
```

the process transitions:

```text
DRYING -> COOLING
```

Before entering COOLING:

```text
Heater = OFF
Elevator = OFF
Fan = ON
Door = CLOSED
```

The exact moisture threshold and filtering behavior should be validated experimentally.

---

# 24. Minimum and Maximum Drying Time

Optional protection settings should be available.

Example:

```text
Minimum Drying Time
Maximum Drying Time
```

Minimum drying time prevents premature completion.

Maximum drying time prevents the system from remaining in DRYING indefinitely.

If maximum drying time is reached:

```text
Heater = OFF
Fan = ON
Fault = MAX_DRYING_TIME
```

The operator must acknowledge the fault before continuing.

---

# 25. COOLING State

COOLING begins after the target moisture is reached.

Outputs:

```text
Heater   = OFF
Fan      = ON
Elevator = OFF
Door     = CLOSED
```

The fan continues removing heat from the rice and drying chamber.

Cooling continues until the configured cooling temperature is reached.

Example:

```text
Cooling Temperature = 35°C
```

When:

```text
Temperature <= Cooling Temperature
```

the process transitions:

```text
COOLING -> DISCHARGING
```

---

# 26. DISCHARGING State

DISCHARGING removes the dried rice from the chamber.

Sequence:

```text
Heater OFF
Fan OFF
Door OPEN
Elevator ON
```

The door opens first.

The software should verify that the door has reached the required position if position feedback is available.

After the door is open:

```text
Elevator = ON
```

The elevator runs for the configured discharge time.

Example:

```text
Discharge Time = 60 sec
```

After the timer expires:

```text
Elevator = OFF
Door = CLOSE
```

The system then transitions:

```text
DISCHARGING -> COMPLETE
```

---

# 27. Door Control

The door is controlled using two outputs:

```text
Door Open  -> R4
Door Close -> R5
```

The software must enforce:

```text
R4 != ON
R5 != ON
```

at the same time.

If door position sensors are added, the software should use them to verify:

- Fully open
- Fully closed
- Moving
- Unknown position

A door timeout should generate a fault.

---

# 28. COMPLETE State

When discharge finishes:

```text
Heater   = OFF
Fan      = OFF
Elevator = OFF
Door     = CLOSED
```

The Batch Manager:

- Records completion time
- Calculates batch duration
- Stores final sensor values
- Saves batch summary
- Closes the batch log
- Updates batch history

The HMI displays:

```text
Batch Complete
Final Moisture
Drying Time
Total Batch Time
```

The operator can return to IDLE.

---

# 29. FAULT State

FAULT is entered when a critical condition occurs.

Examples:

- Overtemperature
- Emergency stop
- Sensor failure
- Moisture sensor failure
- Modbus communication failure
- Elevator timeout
- Door timeout
- SD card failure where logging is mandatory
- Maximum drying time exceeded
- Invalid actuator state

Default safe behavior:

```text
Heater = OFF
Elevator = OFF
```

Fan behavior depends on the fault.

For an overheating condition, the fan may remain ON to remove heat.

The Safety Manager determines the final safe actuator state.

---

# 30. Safety Priority

Control priority:

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

Normal process commands must never override a safety shutdown.

---

# 31. HMI Software

The LVGL interface should be divided into screens.

Recommended screens:

```text
Dashboard
Batch Setup
Drying Process
Manual Control
Batch History
Settings
Alarms
System Information
```

---

# 32. Dashboard Screen

The dashboard displays:

- Current process state
- Moisture
- EC
- Hot-air temperature
- Hot-air humidity
- Chamber temperature
- Chamber humidity
- Heater status
- Fan status
- Elevator status
- Door status
- Current batch status

The dashboard should provide quick access to:

```text
START BATCH
SETTINGS
HISTORY
```

---

# 33. Batch Setup Screen

Before starting a batch, the operator can review:

- Target moisture
- Drying temperature
- Cooling temperature
- Circulation mode
- Circulation interval
- Circulation run time
- Discharge time

The operator confirms the settings before starting.

---

# 34. Drying Process Screen

During DRYING, the screen should display:

```text
DRYING

Moisture:       XX.X %
Target:         XX.X %

Temperature:    XX.X °C
Humidity:       XX.X %

Heater:         ON/OFF
Fan:            ON
Elevator:       ON/OFF

Elapsed Time:   HH:MM:SS
```

A progress indicator may be provided based on moisture reduction.

---

# 35. Cooling Screen

During COOLING:

```text
COOLING

Temperature:    XX.X °C
Target:         XX.X °C

Fan:            ON
Heater:         OFF
Elevator:       OFF
Door:           CLOSED
```

---

# 36. Discharge Screen

During DISCHARGING:

```text
DISCHARGING

Door:           OPEN
Elevator:       ON

Remaining:
00:45
```

The remaining discharge time should be clearly visible.

---

# 37. Manual Control Screen

Manual control is intended for testing and maintenance.

Available controls may include:

- Heater
- Fan
- Elevator
- Door Open
- Door Close

Manual control must only be available when process control permits it.

The software must prevent unsafe combinations.

Examples:

```text
Door OPEN + Door CLOSE = prohibited
Door OPEN + Elevator ON = allowed only when safe
Heater ON during FAULT = prohibited
```

Safety Manager always has final authority.

---

# 38. Settings Manager

The Settings Manager handles persistent configuration.

Settings include:

```text
Target Moisture
Drying Temperature
Temperature Hysteresis
Cooling Temperature

Circulation Mode
Circulation Interval
Circulation Run Time

Discharge Time
Minimum Drying Time
Maximum Drying Time

Temperature Limits
Sensor Calibration
```

Settings are stored using ESP-IDF NVS.

---

# 39. Default Settings

Initial default configuration:

```text
Target Moisture       = 14%
Drying Temperature    = 50°C
Temperature Hysteresis = 2°C
Cooling Temperature   = 35°C

Circulation Mode      = Interval
Circulation Interval  = 2 min
Circulation Run Time  = 10 sec

Discharge Time        = 60 sec
```

These are starting values.

Actual operating values must be validated during testing.

---

# 40. Data Logger

The Logger stores batch data on the SD card.

Logging is active during:

```text
DRYING
COOLING
DISCHARGING
```

Recommended logging interval:

```text
1 second
```

The interval should be configurable if needed.

---

# 41. Batch Log Data

Recommended CSV fields:

```text
Timestamp
Batch ID
State

Moisture
EC

Hot Air Temperature
Hot Air Humidity

Chamber Temperature
Chamber Humidity

Heater
Fan
Elevator
Door

Fault Code
```

Example:

```text
2026-10-08 14:30:01,
BATCH-0001,
DRYING,
18.4,
1.25,
49.8,
32.1,
45.2,
35.4,
1,
1,
0,
0,
NONE
```

---

# 42. Batch Summary

At the end of each batch, store:

```text
Batch ID
Start Time
End Time

Initial Moisture
Final Moisture

Target Moisture

Maximum Temperature
Average Temperature

Drying Duration
Cooling Duration
Discharge Duration
Total Batch Duration

Final EC

Completion Status
Fault Status
```

---

# 43. SD Card File Structure

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
│   ├── BATCH-0001.txt
│   └── ...
│
├── logs/
│   └── system.log
│
└── config/
    └── calibration.dat
```

The exact format may be adjusted during implementation.

---

# 44. RTC Manager

The RTC Manager provides local date and time.

The RTC is used for:

- Batch timestamps
- Sensor logs
- Fault logs
- Batch history
- System events

The system does not require internet access for timekeeping.

---

# 45. Local Web Server

The ESP32-S3 provides a local web interface through Wi-Fi.

Initial implementation should be read-only.

The web interface can display:

- Current state
- Sensor readings
- Actuator status
- Batch status
- Faults
- Current settings
- Batch history

The touchscreen remains the primary control interface.

This prevents command conflicts between the touchscreen and web interface.

---

# 46. Web API

Example endpoints:

```text
GET /api/status
GET /api/sensors
GET /api/actuators
GET /api/batch
GET /api/settings
GET /api/history
GET /api/faults
```

Example status response:

```json
{
    "state": "DRYING",
    "moisture": 16.2,
    "temperature": 49.7,
    "humidity": 31.4,
    "heater": true,
    "fan": true,
    "elevator": false
}
```

The API must not bypass the Safety Manager.

---

# 47. Wi-Fi Operation

The system should operate without internet access.

Recommended architecture:

```text
ESP32-S3
   |
   +---- Wi-Fi Access Point
             |
             +---- Phone
             +---- Tablet
             +---- Laptop
```

The local dashboard must continue working without cloud services.

---

# 48. FreeRTOS Task Architecture

Recommended tasks:

```text
Safety Task
Control Task
Sensor Task
Modbus Task
HMI Task
Logger Task
Web Task
RTC Task
```

Example responsibilities:

| Task | Responsibility |
|---|---|
| Safety Task | Safety monitoring |
| Control Task | State machine and process control |
| Sensor Task | Sensor processing |
| Modbus Task | RS485 communication |
| HMI Task | LVGL/UI |
| Logger Task | SD logging |
| Web Task | HTTP/API |
| RTC Task | Time management |

Critical control tasks must receive appropriate priority.

---

# 49. Non-Blocking Design

The software must avoid blocking delays in process control.

Do not use:

```cpp
delay(60000);
```

for process timing.

Instead use:

```cpp
millis()
```

or FreeRTOS timing mechanisms.

Example:

```cpp
if (millis() - lastRun >= circulationInterval) {
    startCirculation();
}
```

This allows the system to continue:

- Sensor monitoring
- Safety monitoring
- HMI updates
- Logging
- Communication

while timers are running.

---

# 50. Control Loop

The main control loop should periodically:

1. Read latest sensor values.
2. Validate sensor data.
3. Check safety conditions.
4. Evaluate the current process state.
5. Update heater control.
6. Update fan control.
7. Update circulation.
8. Update door control.
9. Check state transition conditions.
10. Update HMI data.
11. Queue logging data.

Safety checks must have priority.

---

# 51. Example Control Logic

```cpp
switch (processState) {

    case IDLE:
        stopHeater();
        stopFan();
        stopElevator();
        ensureDoorClosed();
        break;

    case PRECHECK:
        runPrecheck();
        break;

    case DRYING:
        setFan(true);
        controlHeaterWithHysteresis();
        updateCirculation();
        monitorDryingSensors();

        if (targetMoistureReached()) {
            transitionTo(COOLING);
        }
        break;

    case COOLING:
        setHeater(false);
        setFan(true);
        setElevator(false);

        if (coolingTemperatureReached()) {
            transitionTo(DISCHARGING);
        }
        break;

    case DISCHARGING:
        setHeater(false);
        setFan(false);

        openDoor();

        if (doorIsOpen()) {
            runDischargeElevator();
        }

        if (dischargeTimerComplete()) {
            stopElevator();
            closeDoor();
            transitionTo(COMPLETE);
        }
        break;

    case COMPLETE:
        stopAllActuators();
        break;

    case FAULT:
        applySafeState();
        break;
}
```

The actual implementation must include validation and safety handling around every actuator command.

---

# 52. Power Loss Recovery

The software must assume that power can be interrupted.

On startup:

```text
Heater OFF
Fan OFF
Elevator OFF
Door -> Safe/CLOSED
```

The system must not automatically restart the heater or continue drying without operator confirmation.

The previous batch state may be stored for recovery analysis.

Example:

```text
Previous State: DRYING
Power Lost
System Restarted

Status:
"Previous batch interrupted."
```

The operator can decide whether to restart or perform a recovery procedure.

---

# 53. Startup Sequence

Recommended startup sequence:

```text
Power ON
   |
   v
Initialize ESP-IDF
   |
   v
Initialize Drivers
   |
   v
Initialize Sensors
   |
   v
Initialize SD
   |
   v
Initialize RTC
   |
   v
Initialize Settings
   |
   v
Initialize Safety Manager
   |
   v
Initialize Actuator Manager
   |
   v
Initialize LVGL
   |
   v
Initialize Web Server
   |
   v
Apply Safe Outputs
   |
   v
IDLE
```

All outputs must default to safe states.

---

# 54. Error Handling

Each module should return explicit status information.

Example:

```cpp
enum class Result {
    OK,
    TIMEOUT,
    INVALID_DATA,
    COMMUNICATION_ERROR,
    HARDWARE_ERROR,
    NOT_READY
};
```

Errors should be logged when appropriate.

Critical errors should be forwarded to the Safety Manager.

---

# 55. Diagnostics

The Diagnostics module monitors:

- Sensor communication
- Modbus communication
- ADS1115 readings
- SD card status
- RTC status
- Relay communication
- Task health
- Memory usage
- Fault history
- System uptime

Diagnostics information should be available through the System Information screen.

---

# 56. Fault Codes

Example fault codes:

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

Fault codes should be stored with timestamps.

---

# 57. Configuration Validation

Before saving settings, validate:

```text
Target Moisture > 0
Drying Temperature within safe range
Cooling Temperature < Drying Temperature
Hysteresis > 0

Circulation Interval > 0
Circulation Run Time > 0
Circulation Run Time < Interval

Discharge Time > 0
Maximum Drying Time > Minimum Drying Time
```

Unsafe settings must be rejected.

---

# 58. Software Safety Rules

The software must enforce:

1. Heater cannot operate during FAULT.
2. Heater cannot operate when required temperature sensors are invalid.
3. Door OPEN and Door CLOSE outputs cannot operate simultaneously.
4. Elevator must stop on timeout.
5. Heater must turn OFF before COOLING.
6. Heater must remain OFF during DISCHARGING.
7. Elevator must not run during COOLING in the initial design.
8. Fan must remain ON during normal DRYING.
9. Outputs must start in safe states.
10. Safety Manager can override normal process control.
11. Power restoration must not automatically restart the batch.
12. Hardware thermal protection must operate independently of software.

---

# 59. Software Project Structure

Recommended project structure:

```text
main/
│
├── app/
│   ├── dryer_controller/
│   │   ├── dryer_controller.cpp
│   │   └── dryer_controller.h
│   │
│   ├── process_controller/
│   │   ├── process_controller.cpp
│   │   └── process_controller.h
│   │
│   ├── safety_manager/
│   │   ├── safety_manager.cpp
│   │   └── safety_manager.h
│   │
│   └── batch_manager/
│       ├── batch_manager.cpp
│       └── batch_manager.h
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
│   │   ├── dashboard/
│   │   ├── batch_setup/
│   │   ├── drying/
│   │   ├── cooling/
│   │   ├── discharge/
│   │   ├── manual_control/
│   │   ├── history/
│   │   ├── settings/
│   │   ├── alarms/
│   │   └── system_info/
│   │
│   ├── components/
│   └── ui_manager/
│
└── main.cpp
```

---

# 60. Module Responsibilities

Each module should have a single primary responsibility.

Example:

```text
Process Controller
    -> Determines process state

Safety Manager
    -> Determines whether operation is safe

Sensor Manager
    -> Provides validated sensor data

Actuator Manager
    -> Controls physical outputs

Circulation Manager
    -> Controls rice circulation timing

Batch Manager
    -> Manages batch lifecycle

Settings Manager
    -> Stores configuration

Logger
    -> Stores historical data

Web Server
    -> Provides local monitoring

HMI
    -> Provides operator interface
```

Modules should communicate through defined interfaces rather than accessing each other's internal variables directly.

---

# 61. Data Flow

Normal process data flow:

```text
Sensors
   |
   v
Sensor Manager
   |
   +----> Safety Manager
   |
   +----> Process Controller
   |
   +----> Logger
   |
   +----> HMI
   |
   +----> Web Server
```

Process command flow:

```text
HMI / Process Controller
          |
          v
   Safety Manager
          |
          v
   Actuator Manager
          |
          v
      Drivers
          |
          v
       Relays
          |
          v
     Actuators
```

---

# 62. Testing Strategy

Software testing should be performed at multiple levels.

## Unit Testing

Test individual modules:

- Moisture conversion
- Calibration
- Hysteresis control
- Circulation timer
- Discharge timer
- Settings validation
- Fault detection
- State transitions

## Integration Testing

Test:

- Sensor Manager + Modbus
- Sensor Manager + ADS1115
- Process Controller + Actuator Manager
- Logger + SD
- HMI + Process Controller
- Safety Manager + Actuator Manager

## System Testing

Test complete workflows:

```text
Manual Load
    ->
Start
    ->
Precheck
    ->
Drying
    ->
Moisture Target
    ->
Cooling
    ->
Discharging
    ->
Complete
```

---

# 63. Safety Test Cases

The following must be tested:

### Overtemperature

Expected:

```text
Heater OFF
Fault ACTIVE
```

### Emergency Stop

Expected:

```text
Heater OFF
Elevator OFF
Fault ACTIVE
```

### Moisture Sensor Failure

Expected:

```text
Drying process stops safely
Fault displayed
```

### Modbus Sensor Failure

Expected:

```text
Fault detected
Safe response applied
```

### Elevator Timeout

Expected:

```text
Elevator OFF
Fault displayed
```

### Door Timeout

Expected:

```text
Door operation stops
Fault displayed
```

### Power Loss

Expected:

```text
All outputs return to safe state
System does not automatically resume
```

---

# 64. Performance Requirements

The software should provide:

- Responsive touchscreen operation
- Non-blocking process control
- Continuous safety monitoring
- Reliable sensor acquisition
- Stable Modbus communication
- Reliable SD logging
- No unnecessary network dependency
- No uncontrolled memory growth
- Stable operation for the full drying cycle

---

# 65. Maintainability

The code should follow these principles:

- Small modules
- Clear interfaces
- Minimal global variables
- Explicit state transitions
- Centralized actuator control
- Centralized safety control
- Configuration separated from logic
- Hardware drivers separated from application logic
- No duplicated process logic
- No blocking delays in control functions

---

# 66. Future Expansion

The software architecture should allow future additions without major changes.

Possible future features:

- PID temperature control
- More advanced moisture estimation
- Additional temperature sensors
- Additional EC analysis
- Door position sensors
- Motor feedback
- Remote monitoring
- Web-based configuration
- Exportable batch reports
- Inverter/energy monitoring
- Solar generation monitoring
- Battery monitoring
- Automatic recipe management

These features should not compromise the current safety architecture.

---

# 67. Final Software Architecture

The complete software architecture is:

```text
                    +----------------------+
                    |     LVGL HMI         |
                    +----------+-----------+
                               |
                    +----------v-----------+
                    |  Dryer Controller    |
                    +----------+-----------+
                               |
              +----------------+----------------+
              |                |                |
      +-------v------+ +-------v------+ +-------v------+
      |   Process    | |    Safety    | |    Batch     |
      |  Controller  | |   Manager    | |   Manager    |
      +-------+------+ +-------+------+ +-------+------+
              |                |                |
              +----------------+----------------+
                               |
                    +----------v-----------+
                    |    Service Layer     |
                    |                      |
                    | Sensor Manager       |
                    | Actuator Manager     |
                    | Circulation Manager  |
                    | Settings Manager     |
                    | Logger               |
                    | Diagnostics          |
                    | Web Server           |
                    | RTC Manager          |
                    +----------+-----------+
                               |
                    +----------v-----------+
                    |    Driver Layer      |
                    |                      |
                    | RS485 / Modbus       |
                    | ADS1115              |
                    | RTC                  |
                    | SD                   |
                    | Relay               |
                    +----------+-----------+
                               |
                    +----------v-----------+
                    |      Hardware        |
                    +----------------------+
```

---

# 68. Design Principles

The software shall follow these principles:

1. **Safety first**
2. **Process control is state-based**
3. **Sensors are validated before use**
4. **Actuators are centrally controlled**
5. **Safety can override process commands**
6. **Timing is non-blocking**
7. **The system operates offline**
8. **Batch data is stored locally**
9. **Settings are persistent**
10. **The touchscreen is the primary control interface**
11. **The initial web dashboard is read-only**
12. **Power loss must result in a safe restart**
13. **Hardware safety remains independent of software**
14. **The system must be modular and maintainable**
15. **Rice circulation must support continuous and interval modes**

---

# 69. Summary

The software architecture provides a modular and safety-oriented control system for the 10 kg hybrid solar rice dryer.

The finalized process is:

```text
IDLE
  |
  v
PRECHECK
  |
  v
DRYING
  |
  | Moisture target reached
  v
COOLING
  |
  | Cooling temperature reached
  v
DISCHARGING
  |
  | Discharge timer complete
  v
COMPLETE
```

During DRYING:

```text
Fan = ON
Heater = Hysteresis Controlled
Elevator = Continuous OR Interval
```

During COOLING:

```text
Heater = OFF
Fan = ON
Elevator = OFF
```

During DISCHARGING:

```text
Heater = OFF
Fan = OFF
Door = OPEN
Elevator = ON
```

After discharge:

```text
Elevator = OFF
Door = CLOSED
Batch = COMPLETE
```

The architecture separates process logic, safety, sensor acquisition, actuator control, circulation, logging, HMI, and hardware drivers.

This provides a solid foundation for implementation using ESP-IDF, FreeRTOS, LVGL, Modbus RTU, SD storage, and the ESP32-S3.