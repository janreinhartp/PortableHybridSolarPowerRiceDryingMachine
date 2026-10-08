# 02 - Technical Requirements

## 1. Purpose

This document defines the technical requirements for the 10 kg/batch Solar Hybrid Rice Dryer.

The system is based on an ESP32-S3 controller with touchscreen HMI, RS485 Modbus RTU sensors and relay control, analog moisture sensing, RTC timekeeping, SD card data logging, and a local Wi-Fi web dashboard.

The controller operates the rice drying process but does not manage the hybrid inverter's power-management functions.

---

# 2. Controller Requirements

## 2.1 Main Controller

The main controller shall be:

- Waveshare ESP32-S3-Touch-LCD-7B
- ESP32-S3
- Integrated 7-inch touchscreen display
- ESP-IDF firmware
- C/C++ application code
- FreeRTOS task architecture
- LVGL-based HMI

The ESP32-S3 shall coordinate:

- Sensor acquisition
- Process control
- Heater control
- Fan control
- Rice circulation
- Discharge control
- Safety monitoring
- HMI
- Data logging
- RTC
- Local web server

---

# 3. Operating Environment

The controller software shall support:

- Offline operation
- Local Wi-Fi Access Point operation
- Local touchscreen operation
- SD card operation
- Continuous batch operation
- Sensor communication failures
- Actuator failures
- Unexpected power interruption

The system shall not require cloud connectivity for normal operation.

---

# 4. Drying Process Requirements

## 4.1 Process State Machine

The main process state machine shall be:

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

Any active state shall be able to transition to:

```text
FAULT
```

The controller shall not include an automated rice-loading state.

Rice is manually loaded by the operator before starting the batch.

---

# 5. IDLE State

When the system is IDLE:

- Heater shall be OFF.
- Fan shall be OFF.
- Elevator shall be OFF.
- Discharge door shall be CLOSED.
- No batch shall be actively running.
- The HMI shall allow batch configuration.
- The system shall monitor critical faults.

The system shall remain in IDLE until the operator starts a batch.

---

# 6. PRECHECK State

Before starting DRYING, the controller shall verify required system conditions.

Minimum precheck requirements:

- Temperature sensor communication available
- Humidity sensor communication available
- Moisture sensor available
- RS485 communication available
- Relay controller communication available
- RTC available
- SD card available
- No critical active fault
- Discharge door in a safe state
- Required process settings available
- Actuator outputs initialized

If required precheck conditions fail:

```text
PRECHECK -> FAULT
```

The HMI shall display the reason for the failed precheck.

---

# 7. DRYING State

During DRYING:

- Fan shall be ON.
- Heater shall be controlled according to the configured temperature.
- Elevator shall circulate rice.
- Moisture shall be monitored.
- Temperature shall be monitored.
- Humidity shall be monitored.
- EC shall be monitored when available.
- Process data shall be logged.
- Safety monitoring shall remain active.

The controller shall evaluate the target moisture continuously.

When:

```text
Measured Moisture <= Target Moisture
```

the controller shall:

1. Turn heater OFF.
2. Transition to COOLING.
3. Keep fan ON.

---

# 8. Heater Control

## 8.1 Initial Control Method

The initial heater control method shall use hysteresis.

Example:

```text
Target Temperature = 50°C
Hysteresis = 2°C
```

Control behavior:

```text
Temperature <= 48°C
    Heater ON

Temperature >= 52°C
    Heater OFF
```

The heater shall maintain its previous state between the lower and upper hysteresis limits.

Example:

```text
48°C -> ON
49°C -> maintain previous state
50°C -> maintain previous state
51°C -> maintain previous state
52°C -> OFF
```

The fan shall remain ON regardless of temporary heater cycling while the machine remains in DRYING.

---

# 9. Heater Safety

The controller shall implement a maximum temperature limit.

If:

```text
Temperature >= Maximum Safe Temperature
```

the controller shall:

- Immediately request heater OFF.
- Generate a critical fault.
- Record the fault.
- Prevent normal heating until the fault is cleared according to the safety procedure.

The hardware design should also include an independent thermal protection mechanism.

Software shall not be the only protection against heater over-temperature.

---

# 10. Rice Circulation Requirements

The elevator shall be treated as the rice circulation mechanism.

The elevator shall support two operating modes.

## 10.1 Continuous Circulation

When configured for continuous circulation:

```text
Elevator = ON
```

The elevator remains ON throughout the applicable DRYING period.

---

## 10.2 Interval Circulation

When configured for interval circulation:

```text
Wait for configured interval
        |
        v
Elevator ON
        |
        v
Run for configured duration
        |
        v
Elevator OFF
        |
        v
Repeat
```

Example:

```text
Circulation Interval = 2 minutes
Run Duration = 10 seconds
```

The interval timer and run timer shall operate independently from the main control loop.

---

# 11. Circulation Configuration

The following parameters shall be configurable:

| Parameter | Example | Requirement |
|---|---:|---|
| Circulation Mode | Continuous | Required |
| Circulation Interval | 2 min | Required |
| Circulation Run Time | 10 sec | Required |

The controller shall validate configured values before accepting them.

The interval and run duration shall have configurable minimum and maximum limits.

The elevator shall not be allowed to operate if a critical safety fault is active.

---

# 12. Elevator Motor Protection

The controller shall include software timeout protection.

If the elevator is commanded ON for longer than its permitted runtime:

- Elevator shall be turned OFF.
- A fault shall be generated.
- The event shall be logged.

Hardware motor protection should be provided independently where required by the motor and electrical design.

---

# 13. Cooling Requirements

When the target moisture is reached:

```text
Heater = OFF
Fan = ON
```

The controller shall enter COOLING.

During COOLING:

- Heater shall remain OFF.
- Fan shall remain ON.
- Temperature shall continue to be monitored.
- Cooling time shall be logged.
- Safety monitoring shall remain active.

When:

```text
Temperature <= Cooling Temperature
```

the controller shall transition to DISCHARGING.

Example:

```text
Cooling Temperature = 35°C
```

---

# 14. Discharge Requirements

When cooling is complete:

1. Confirm heater is OFF.
2. Confirm required safety conditions.
3. Open discharge door.
4. Confirm or wait for door-open condition where position feedback is available.
5. Start elevator.
6. Run elevator for configured discharge duration.
7. Stop elevator.
8. Close discharge door.
9. Confirm or wait for door-closed condition where position feedback is available.
10. Transition to COMPLETE.

---

# 15. Discharge Timer

The discharge time shall be configurable.

Example:

```text
Discharge Time = 60 seconds
```

The elevator shall run only for the configured discharge period.

If the discharge timer expires:

```text
Elevator -> OFF
```

The controller shall then close the discharge door.

---

# 16. Discharge Door Interlock

The discharge door uses separate OPEN and CLOSE controls.

The controller shall guarantee:

```text
OPEN = OFF
CLOSE = OFF
```

or only one direction active at a time.

The controller shall never intentionally command:

```text
OPEN = ON
CLOSE = ON
```

simultaneously.

If door position sensors are installed, the controller shall use them to verify:

- Door fully open
- Door fully closed
- Door movement timeout

A door movement timeout shall generate a fault.

---

# 17. Fan Requirements

The fan shall be controlled according to process state.

| State | Fan |
|---|---|
| IDLE | OFF |
| PRECHECK | OFF |
| DRYING | ON |
| COOLING | ON |
| DISCHARGING | OFF |
| COMPLETE | OFF |
| FAULT | Safe-state dependent |

The fan shall remain ON while the heater cycles during DRYING.

---

# 18. Sensor Requirements

## 18.1 Temperature and Humidity Sensors

Two RS485 Modbus RTU temperature/humidity sensors shall be supported.

Suggested assignments:

```text
Sensor 1 = Hot-Air Temperature/Humidity
Sensor 2 = Drying Chamber Temperature/Humidity
```

Each sensor shall have a unique Modbus address.

Example:

```text
Sensor 1 = Address 1
Sensor 2 = Address 2
```

The final addresses shall be configurable.

---

# 19. RS485 Modbus Requirements

The RS485 bus shall support:

- Modbus RTU
- CRC validation
- Request timeout
- Response timeout
- Retry handling
- Communication error detection
- Device address validation
- Invalid response detection

The system shall detect loss of communication with critical sensors.

Recommended initial retry behavior:

```text
Request
  |
  v
Timeout?
  |
 Yes
  |
Retry
  |
  v
Retry limit reached?
  |
 Yes
  |
Generate fault
```

The exact retry count shall be configurable in firmware.

---

# 20. RS485 Device Architecture

The RS485 network shall support:

```text
ESP32-S3
    |
    +---- RS485 Temperature/Humidity Sensor 1
    |
    +---- RS485 Temperature/Humidity Sensor 2
    |
    +---- RS485 Relay Controller
```

All devices shall have unique Modbus addresses.

Example:

```text
01 = Hot-Air Sensor
02 = Chamber Sensor
10 = Relay Controller
```

The final addresses shall be configurable.

---

# 21. Moisture Sensor Requirements

The rice moisture/EC sensor provides an analog output.

The exact electrical interface shall be verified against the final sensor datasheet.

The ESP32-S3 ADC shall not be connected directly to a 0-5 V signal.

A signal-conditioning stage shall be provided.

The interface shall include, where required:

- Voltage divider
- Input protection
- RC filtering
- Over-voltage protection
- Appropriate grounding
- ADC scaling

The firmware shall convert the ADC reading into an engineering value using a calibration model.

Example:

```text
ADC Reading
    |
    v
Voltage Conversion
    |
    v
Calibration
    |
    v
Moisture %
```

If the selected sensor provides separate moisture and EC outputs, each output shall use an independent ADC input.

The final implementation shall follow the selected sensor's datasheet.

---

# 22. Moisture Calibration

The moisture measurement system shall support calibration.

Calibration data shall be stored in non-volatile memory.

The system should support:

- Calibration offset
- Calibration scale
- Reference measurement
- Calibration date
- Calibration status

The HMI shall provide a maintenance interface for authorized calibration.

---

# 23. EC Measurement

If the selected moisture sensor provides EC measurement, the system shall store and display the EC value.

EC measurement shall not be used as the primary automatic drying completion condition unless validated during testing.

Primary drying completion shall remain:

```text
Moisture <= Target Moisture
```

---

# 24. RTC Requirements

The RTC shall provide:

- Date
- Time
- Batch timestamps
- Log timestamps
- Fault timestamps
- System event timestamps

The controller shall use the RTC for persistent timestamps when internet time synchronization is unavailable.

---

# 25. SD Card Requirements

The SD card shall be used for local data storage.

The system shall store:

- Batch records
- Raw process logs
- Fault logs
- System logs
- Configuration backups where required

Example directory structure:

```text
/data
    /batches
    /logs
    /faults
    /system
    /exports
```

Example batch file:

```text
batch_20261009_001.csv
```

---

# 26. Data Logging Requirements

During active drying, process data shall initially be logged every 1 second.

Minimum logged parameters:

```text
Timestamp
Batch ID
Process State
Moisture
EC
Hot-Air Temperature
Hot-Air Humidity
Chamber Temperature
Chamber Humidity
Heater State
Fan State
Elevator State
Door State
Fault Code
```

The logger shall avoid blocking the main control loop.

Logging failures shall not cause uncontrolled heater operation.

---

# 27. Batch Management

Each batch shall have a unique Batch ID.

Example:

```text
20261009-001
```

Batch information shall include:

- Batch ID
- Start time
- End time
- Initial moisture
- Final moisture
- Target moisture
- Drying temperature
- Cooling temperature
- Maximum temperature
- Total drying duration
- Cooling duration
- Discharge duration
- Heater runtime
- Fan runtime
- Fault count
- Completion status

---

# 28. HMI Requirements

The LVGL touchscreen interface shall provide:

- Dashboard
- Batch Setup
- Drying Process
- Manual Control
- History
- Settings
- Alarm
- System Information

The HMI shall display current:

- State
- Moisture
- Temperature
- Humidity
- EC
- Heater status
- Fan status
- Elevator status
- Door status
- Fault status

The HMI shall not directly manipulate hardware outputs.

All commands shall pass through the application control layer.

---

# 29. Batch Setup Parameters

The HMI shall allow configuration of:

```text
Target Moisture
Drying Temperature
Temperature Hysteresis
Cooling Temperature
Minimum Drying Time
Maximum Drying Time
Circulation Mode
Circulation Interval
Circulation Run Time
Discharge Time
```

The system shall validate all parameters before a batch can start.

---

# 30. Manual Control

Manual control shall be available for maintenance and commissioning.

Supported functions:

- Heater ON/OFF
- Fan ON/OFF
- Elevator ON/OFF
- Door OPEN
- Door CLOSE

Manual control shall be subject to safety interlocks.

Examples:

- Heater cannot operate during a critical over-temperature fault.
- Door OPEN and CLOSE cannot operate simultaneously.
- Elevator cannot operate when a critical elevator fault is active.
- Unsafe combinations shall be rejected by the control layer.

---

# 31. Safety Manager

The Safety Manager shall have final authority over actuator commands.

The software architecture shall follow:

```text
User / Process Request
        |
        v
Safety Validation
        |
        v
Actuator Manager
        |
        v
Physical Output
```

Safety conditions shall be evaluated independently from the HMI.

The HMI shall never bypass the Safety Manager.

---

# 32. Critical Safety Conditions

The controller shall detect:

- Over-temperature
- Critical temperature sensor failure
- Critical moisture sensor failure
- RS485 communication failure
- Relay communication failure
- Elevator timeout
- Discharge door timeout
- Invalid sensor readings
- SD card failure
- RTC failure
- Unexpected power restart

The appropriate actuator response shall be defined for each fault.

---

# 33. Fault Behavior

For a critical drying fault:

```text
Heater -> OFF
```

The controller shall transition to FAULT.

The system shall log:

- Fault code
- Timestamp
- Process state
- Sensor readings
- Actuator states
- Relevant diagnostic information

The HMI shall display the fault.

The operator shall be required to acknowledge or clear the fault according to its severity.

---

# 34. Startup Safety

After controller startup:

```text
Heater = OFF
Elevator = OFF
Fan = OFF
Door = Safe State
```

The controller shall initialize communication and verify outputs before allowing process operation.

The system shall not automatically resume heating after an uncontrolled restart.

---

# 35. Power Loss Recovery

The controller shall detect an interrupted batch after restart.

The previous batch state shall be stored periodically in non-volatile memory where practical.

After recovery:

- Heater remains OFF.
- Elevator remains OFF.
- Fan remains OFF initially.
- Door remains in a safe state.
- Interrupted batch is identified.
- Operator is notified.
- Automatic heater restart is prohibited.

The operator must explicitly start a new or recovery batch.

---

# 36. Local Wi-Fi Requirements

The ESP32 shall provide a local Wi-Fi Access Point.

Example:

```text
SSID: RiceDryer-XXXX
IP: 192.168.4.1
```

The exact SSID format shall be configurable.

The web interface shall operate without internet connectivity.

---

# 37. Web Dashboard Requirements

The initial web dashboard shall be read-only.

Required information:

- Machine state
- Sensor readings
- Actuator states
- Current batch
- Batch history
- Active alarms
- System status
- Logs

The web interface shall not directly control GPIO or relay outputs.

---

# 38. Web API

The firmware should provide endpoints similar to:

```text
GET /api/status
GET /api/sensors
GET /api/actuators
GET /api/alarms
GET /api/batches
GET /api/batches/{id}
GET /api/logs/system
GET /api/logs/faults
GET /api/export/{file}
```

Large files should be streamed from the SD card instead of loading the entire file into RAM.

---

# 39. Settings Storage

Persistent configuration shall be stored using ESP32 non-volatile storage.

Settings shall include:

- Target moisture
- Drying temperature
- Temperature hysteresis
- Cooling temperature
- Minimum drying time
- Maximum drying time
- Circulation mode
- Circulation interval
- Circulation run time
- Discharge time
- Sensor calibration
- Modbus configuration
- Wi-Fi configuration

Factory-default values shall be available.

---

# 40. Recommended Default Settings

Initial prototype defaults:

| Parameter | Default |
|---|---:|
| Target Moisture | 14% |
| Drying Temperature | 50°C |
| Temperature Hysteresis | 2°C |
| Cooling Temperature | 35°C |
| Circulation Mode | Interval |
| Circulation Interval | 2 min |
| Circulation Run Time | 10 sec |
| Discharge Time | 60 sec |

These values are starting points for testing and shall be validated experimentally before production use.

---

# 41. FreeRTOS Task Requirements

The software should be divided into logical FreeRTOS tasks.

Recommended tasks:

```text
Safety Task
Control Task
Sensor Task
Modbus Task
HMI Task
Logger Task
Web Server Task
RTC Task
```

The Control Task shall coordinate the main drying state machine.

The Safety Task shall continuously evaluate critical safety conditions.

The Sensor and Modbus tasks shall provide validated sensor data.

The Logger Task shall handle SD card operations.

---

# 42. Control Timing

Recommended initial control timing:

| Function | Target |
|---|---:|
| Main control loop | 100-500 ms |
| Safety evaluation | <=100 ms where practical |
| Sensor polling | 500-1000 ms |
| HMI update | 100-250 ms |
| Active process logging | 1 sec |
| Web status update | 1 sec |
| Elevator interval timing | 1 sec resolution |

The exact task periods shall be validated during implementation.

---

# 43. Sensor Filtering

Sensor readings shall be filtered before being used for process decisions.

The system may use:

- Moving average
- Exponential moving average
- Median filtering
- Range validation

The filtering method shall not introduce excessive delay into temperature or moisture control.

The raw and processed values should be available for diagnostics where practical.

---

# 44. Process Control Rules

The process controller shall follow these rules:

### DRYING

```text
Fan = ON

Heater =
    ON below lower temperature limit
    OFF above upper temperature limit

Elevator =
    Continuous
    OR
    Interval

If moisture <= target:
    Heater = OFF
    State = COOLING
```

### COOLING

```text
Heater = OFF
Fan = ON

If temperature <= cooling threshold:
    State = DISCHARGING
```

### DISCHARGING

```text
Heater = OFF
Fan = OFF
Door = OPEN
Elevator = ON

After discharge timer:
    Elevator = OFF
    Door = CLOSED
    State = COMPLETE
```

---

# 45. Actuator Manager

The Actuator Manager shall be the only software module responsible for executing physical actuator commands.

Example interface:

```text
setHeater(bool state)
setFan(bool state)
setElevator(bool state)
openDischargeDoor()
closeDischargeDoor()
stopDischargeDoor()
```

The Actuator Manager shall enforce output interlocks.

The process controller shall request actions rather than directly manipulating physical outputs.

---

# 46. Relay Mapping

Initial relay mapping:

| Relay | Function |
|---|---|
| R1 | Elevator |
| R2 | Heater control |
| R3 | Fan |
| R4 | Discharge Door OPEN |
| R5 | Discharge Door CLOSE |
| R6 | Spare |
| R7 | Spare |
| R8 | Spare |

The exact mapping shall remain configurable where practical.

The heater relay output shall control the appropriate SSR, contactor, or power-control stage rather than directly carrying high heater current unless the relay is specifically rated for the final heater load.

---

# 47. Hardware Abstraction

The software shall isolate hardware-specific functions behind drivers or managers.

Recommended interfaces:

```text
RS485 Driver
Modbus Driver
Sensor Manager
Relay Manager
ADC Driver
RTC Driver
SD Driver
Display Driver
Touch Driver
Wi-Fi/Web Server
```

Application logic should not directly depend on low-level GPIO or ADC implementation.

---

# 48. Diagnostics

The system shall provide diagnostic information for:

- RS485 devices
- Sensor values
- ADC readings
- Relay states
- Door state
- Elevator state
- SD card
- RTC
- Wi-Fi
- Free heap
- CPU/task status
- Active faults

Diagnostics shall be accessible through the maintenance interface.

---

# 49. Performance Requirements

The system shall:

- Maintain stable control operation during continuous drying.
- Avoid blocking the control loop with SD operations.
- Avoid blocking the control loop with web requests.
- Handle sensor communication retries without freezing the HMI.
- Maintain responsive touchscreen operation.
- Prevent excessive dynamic memory allocation.
- Recover from temporary communication failures where possible.

---

# 50. Reliability Requirements

The firmware shall use:

- Watchdog protection
- Timeout handling
- Communication retries
- Input validation
- Output interlocks
- Fault detection
- Safe startup states
- Persistent configuration
- Controlled error recovery

The controller shall fail to a safe state whenever a critical condition cannot be reliably determined.

---

# 51. Maintainability Requirements

The firmware shall use modular components.

Recommended project structure:

```text
main/
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
│   ├── adc/
│   ├── rtc/
│   ├── sd/
│   └── relay/
│
└── ui/
    ├── screens/
    ├── components/
    └── ui_manager/
```

The elevator control logic should be implemented as a rice circulation module rather than a loading-elevator module.

---

# 52. Security Requirements

The local web dashboard shall initially be read-only.

The system should provide:

- Configurable Wi-Fi AP credentials
- Optional AP password
- No external cloud connection
- No unnecessary network services
- Input validation for web requests
- Protection against invalid file paths during file export

The web server shall not expose direct hardware-control endpoints in the initial version.

---

# 53. Technical Acceptance Criteria

The implementation shall be considered technically acceptable when:

1. ESP32-S3 boots into a safe state.
2. All required sensors can be detected.
3. RS485 Modbus communication is reliable.
4. Moisture readings can be calibrated.
5. Temperature readings are validated.
6. The fan starts when DRYING begins.
7. The heater operates using hysteresis control.
8. The elevator supports continuous circulation.
9. The elevator supports interval circulation.
10. Circulation interval is configurable.
11. Circulation run time is configurable.
12. Target moisture ends the drying stage.
13. Heater turns OFF when drying completes.
14. Fan remains ON during COOLING.
15. Cooling temperature ends the cooling stage.
16. Discharge door opens after cooling.
17. Elevator runs for the configured discharge duration.
18. Elevator stops after the discharge timer.
19. Discharge door closes after discharge.
20. Heater cannot operate during critical over-temperature conditions.
21. Door OPEN and CLOSE outputs are interlocked.
22. Elevator timeout generates a fault.
23. Sensor communication failure generates an appropriate fault.
24. Batch data is stored on the SD card.
25. Fault data is stored on the SD card.
26. RTC timestamps are available.
27. HMI displays real-time process information.
28. Local web dashboard displays process information.
29. CSV batch data can be exported.
30. Unexpected restart does not automatically restart heating.
31. The system can operate without internet access.
32. The control loop remains responsive during SD and web operations.