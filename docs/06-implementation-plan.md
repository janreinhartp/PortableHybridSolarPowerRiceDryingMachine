# 06 Implementation Plan

## 1. Purpose

This document defines the implementation plan for the 10 kg/batch Hybrid Solar Power Rice Dryer.

The implementation will follow an incremental approach.

Each subsystem will be developed and tested independently before full system integration.

The development priority is:

1. Safety
2. Hardware communication
3. Sensor acquisition
4. Actuator control
5. Process control
6. HMI
7. Data logging
8. Web monitoring
9. Full system testing

---

# 2. Development Approach

The system will be developed in stages.

```text
Stage 1
Project Foundation
        |
        v
Stage 2
Hardware Drivers
        |
        v
Stage 3
Sensor System
        |
        v
Stage 4
Actuator System
        |
        v
Stage 5
Safety System
        |
        v
Stage 6
Process Controller
        |
        v
Stage 7
HMI
        |
        v
Stage 8
Data Logging
        |
        v
Stage 9
Web Dashboard
        |
        v
Stage 10
System Integration
        |
        v
Stage 11
Drying Validation
```

Each stage should have a working test before moving to the next stage.

---

# 3. Phase 1 - Project Foundation

## Objective

Create the base ESP-IDF project and establish the software structure.

### Tasks

- Create ESP-IDF project
- Configure Waveshare ESP32-S3 Touch LCD 7B
- Configure PlatformIO or selected ESP-IDF development environment
- Configure C++ support
- Configure FreeRTOS
- Configure LVGL
- Configure SD card
- Configure Wi-Fi
- Create project folder structure
- Configure logging
- Create basic error-handling system
- Create system initialization sequence

### Expected Result

The ESP32-S3 should:

- Boot successfully
- Initialize the display
- Start LVGL
- Detect the SD card
- Initialize Wi-Fi
- Display system information

### Acceptance Criteria

```text
[ ] ESP32-S3 boots
[ ] Display works
[ ] Touch input works
[ ] LVGL works
[ ] SD card detected
[ ] Wi-Fi starts
[ ] Serial logging works
```

---

# 4. Phase 2 - Hardware Driver Layer

## Objective

Create reusable drivers for all connected hardware.

Recommended structure:

```text
drivers/
├── rs485/
├── modbus/
├── adc/
├── rtc/
├── sd/
└── relay/
```

---

# 5. RS485 Driver

## Tasks

- Configure UART
- Configure RS485 mode
- Configure baud rate
- Configure parity
- Configure stop bits
- Implement transmit
- Implement receive
- Implement timeout
- Implement error handling

### Test

Connect one RS485 temperature/humidity sensor.

Verify:

```text
ESP32-S3
   |
   v
RS485
   |
   v
Sensor
```

### Acceptance Criteria

```text
[ ] Sensor responds
[ ] CRC is validated
[ ] Timeout works
[ ] Invalid response is detected
[ ] Communication errors are logged
```

---

# 6. Modbus Driver

## Tasks

Implement:

- Modbus RTU request
- Register reading
- CRC calculation
- CRC validation
- Device addressing
- Timeout handling
- Retry handling
- Communication statistics

Example interface:

```cpp
Result modbusReadHoldingRegisters(
    uint8_t address,
    uint16_t registerAddress,
    uint16_t count,
    uint16_t* data
);
```

### Acceptance Criteria

```text
[ ] Both temperature/RH sensors can be addressed
[ ] Register values are correct
[ ] Communication timeout works
[ ] CRC errors are detected
[ ] Retry mechanism works
```

---

# 7. ADC Driver

## Objective

Create the analog input interface for the moisture/EC sensor.

## Tasks

- Configure ESP32-S3 ADC
- Configure ADC channel
- Read raw ADC value
- Convert ADC value to voltage
- Apply filtering
- Detect out-of-range values

Important:

The ESP32-S3 ADC must not receive a raw 0-5 V signal.

The analog interface must include the required voltage scaling and protection hardware.

### Acceptance Criteria

```text
[ ] ADC reads correctly
[ ] Voltage conversion is correct
[ ] Filtering works
[ ] Out-of-range readings are detected
```

---

# 8. RTC Driver

## Tasks

- Initialize TinyRTC
- Read date
- Read time
- Set date/time
- Detect RTC communication errors

### Acceptance Criteria

```text
[ ] RTC initializes
[ ] Current time can be read
[ ] Date can be read
[ ] Time can be set
[ ] RTC failure is detected
```

---

# 9. SD Card Driver

## Tasks

- Initialize SD card
- Detect card
- Create directories
- Create files
- Write data
- Read data
- Flush data
- Handle card errors

### Acceptance Criteria

```text
[ ] SD card detected
[ ] File can be created
[ ] Data can be written
[ ] Data can be read
[ ] Write failure is detected
```

---

# 10. Relay Driver

The relay driver communicates with the 8-channel RS485 Modbus relay board.

Logical outputs:

| Relay | Function |
|---|---|
| R1 | Elevator |
| R2 | Heater |
| R3 | Fan |
| R4 | Door OPEN |
| R5 | Door CLOSE |
| R6 | Spare |
| R7 | Spare |
| R8 | Spare |

## Tasks

- Initialize relay board
- Read relay status if supported
- Turn relay ON
- Turn relay OFF
- Verify commands
- Handle communication failures

### Acceptance Criteria

```text
[ ] All required relays respond
[ ] Each relay activates the correct output
[ ] Relay commands are verified
[ ] Communication failures are detected
```

---

# 11. Phase 3 - Sensor Manager

## Objective

Create a common software interface for all sensors.

Recommended structure:

```text
services/
└── sensor_manager/
    ├── sensor_manager.cpp
    ├── sensor_manager.h
    ├── moisture_sensor.cpp
    ├── moisture_sensor.h
    └── modbus_sensors.cpp
```

---

# 12. Temperature and Humidity Sensors

Connect:

- Hot-air sensor
- Drying chamber sensor

The Sensor Manager should provide:

```cpp
float getHotAirTemperature();
float getHotAirHumidity();

float getChamberTemperature();
float getChamberHumidity();
```

### Validation

Each sensor reading should be checked for:

- Range
- Timeout
- Communication validity
- Stale data

---

# 13. Moisture Sensor

## Tasks

- Read ADC
- Convert ADC to voltage
- Apply calibration
- Calculate moisture
- Filter reading
- Validate range

Example:

```cpp
float getMoisture();
```

The calibration algorithm must be based on the actual sensor and reference moisture measurements.

---

# 14. EC Sensor

## Tasks

- Read EC signal
- Convert to engineering units
- Apply calibration
- Filter reading
- Validate range

EC should initially be treated as a monitoring parameter.

It does not determine batch completion.

---

# 15. Sensor Filtering

Sensor values should not immediately control the heater or process state using a single noisy reading.

Possible filtering methods:

- Moving average
- Exponential moving average
- Median filter

The initial implementation should use a simple and computationally efficient filter.

Example:

```cpp
filtered =
    (previous * 0.8f) +
    (current * 0.2f);
```

Filter parameters should be configurable if necessary.

---

# 16. Phase 4 - Actuator Manager

## Objective

Create a centralized interface for physical outputs.

Recommended structure:

```text
services/
└── actuator_manager/
    ├── actuator_manager.cpp
    └── actuator_manager.h
```

Functions:

```cpp
setHeater(bool state);
setFan(bool state);
setElevator(bool state);

openDoor();
closeDoor();

stopAllActuators();
```

The Process Controller must not directly control relay outputs.

---

# 17. Actuator Test Mode

Before connecting the complete process controller, create an actuator test mode.

Example:

```text
Actuator Test

[ Heater ON ]
[ Heater OFF ]

[ Fan ON ]
[ Fan OFF ]

[ Elevator ON ]
[ Elevator OFF ]

[ Door OPEN ]
[ Door CLOSE ]

[ STOP ALL ]
```

The test mode must include safety restrictions.

For example:

```text
Door OPEN
+
Door CLOSE
=
NOT ALLOWED
```

---

# 18. Elevator Testing

The elevator must be tested independently.

Test:

- Start
- Stop
- Continuous operation
- Short-duration operation
- Timeout
- Repeated cycles

Verify that the motor does not overheat under the expected duty cycle.

---

# 19. Door Testing

Test:

```text
OPEN
CLOSE
OPEN -> CLOSE
CLOSE -> OPEN
```

If position feedback is installed, test:

```text
Fully Open
Fully Closed
Timeout
Unknown Position
```

The door must never receive simultaneous OPEN and CLOSE commands.

---

# 20. Heater Control Hardware Test

The heater control chain should be tested without allowing uncontrolled heater operation.

Test:

```text
ESP32
   |
   v
Relay Output
   |
   v
SSR / Contactor
   |
   v
Heater
```

Verify:

- ON command
- OFF command
- Emergency shutdown
- Overtemperature protection
- Relay failure behavior

High-power heater switching must be protected independently from the ESP32.

---

# 21. Phase 5 - Safety Manager

## Objective

Implement the safety system before implementing automatic drying.

Safety must be available before the heater is allowed to operate automatically.

---

# 22. Safety Conditions

The Safety Manager should monitor:

```text
Emergency Stop
Temperature Limit
Sensor Failure
Modbus Failure
Heater Conditions
Elevator Timeout
Door Timeout
Invalid Configuration
Critical SD Failure
```

---

# 23. Safe State

Default safe state:

```text
Heater   = OFF
Elevator = OFF
Door     = CLOSED
```

Fan behavior depends on the fault.

For overtemperature:

```text
Heater = OFF
Fan = ON
```

The final safety behavior must be defined per fault.

---

# 24. Safety Testing

Test each condition independently.

Example:

### Overtemperature

```text
Temperature > Maximum
        |
        v
Heater OFF
        |
        v
FAULT
```

### Emergency Stop

```text
Emergency Stop
        |
        v
Safety Manager
        |
        v
Safe Outputs
```

### Sensor Failure

```text
Sensor Failure
      |
      v
Safety Manager
      |
      v
Process Halt
```

---

# 25. Phase 6 - Process Controller

## Objective

Implement the complete dryer state machine.

State enumeration:

```cpp
enum class DryerState {
    IDLE,
    PRECHECK,
    DRYING,
    COOLING,
    DISCHARGING,
    COMPLETE,
    FAULT
};
```

---

# 26. State Transition Implementation

Implement:

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
 |
 v
IDLE
```

Any active state can enter:

```text
FAULT
```

---

# 27. IDLE Implementation

Outputs:

```text
Heater   OFF
Fan      OFF
Elevator OFF
Door     CLOSED
```

The HMI allows the operator to:

- View sensors
- Configure settings
- Review history
- Start a batch

---

# 28. PRECHECK Implementation

The Process Controller requests a precheck.

Verify:

```text
Sensors
RTC
SD Card
Emergency Stop
Door
Actuators
Configuration
Fault Status
```

If valid:

```text
PRECHECK -> DRYING
```

Otherwise:

```text
PRECHECK -> FAULT
```

---

# 29. DRYING Implementation

During DRYING:

```text
Fan = ON
Heater = Hysteresis Controlled
Elevator = Circulation Mode
Door = CLOSED
```

The controller continuously evaluates:

```text
Temperature
Moisture
Humidity
EC
Safety
Timers
```

---

# 30. Heater Hysteresis Implementation

Example:

```text
Target = 50°C
Hysteresis = 2°C
```

Logic:

```text
Temperature <= 48°C
    -> Heater ON

Temperature >= 52°C
    -> Heater OFF

48°C < Temperature < 52°C
    -> Maintain previous state
```

Safety limits always override this logic.

---

# 31. Circulation Implementation

Support:

```text
Continuous
Interval
```

### Continuous

```text
Elevator = ON
```

### Interval

Example:

```text
Interval = 2 minutes
Run Time = 10 seconds
```

The Circulation Manager controls the timing.

The Process Controller only requests the configured circulation mode.

---

# 32. Drying Completion

Primary completion condition:

```text
Moisture <= Target Moisture
```

Before transition:

```text
Heater OFF
Elevator OFF
Fan ON
Door CLOSED
```

Then:

```text
DRYING -> COOLING
```

---

# 33. COOLING Implementation

Outputs:

```text
Heater   OFF
Fan      ON
Elevator OFF
Door     CLOSED
```

The controller monitors chamber temperature.

When:

```text
Temperature <= Cooling Temperature
```

transition:

```text
COOLING -> DISCHARGING
```

---

# 34. DISCHARGING Implementation

Sequence:

```text
Heater OFF
Fan OFF
Door OPEN
```

Wait for door-open confirmation if position feedback is available.

Then:

```text
Elevator ON
```

Run for:

```text
Discharge Time
```

Then:

```text
Elevator OFF
Door CLOSE
```

After successful closing:

```text
DISCHARGING -> COMPLETE
```

---

# 35. COMPLETE Implementation

On completion:

```text
Heater OFF
Fan OFF
Elevator OFF
Door CLOSED
```

Batch Manager saves:

- Final moisture
- Final EC
- Maximum temperature
- Total drying time
- Cooling time
- Discharge time
- Completion status

---

# 36. Phase 7 - Settings Manager

## Objective

Implement persistent configuration.

Settings:

```text
Target Moisture
Drying Temperature
Hysteresis
Cooling Temperature

Circulation Mode
Circulation Interval
Circulation Run Time

Discharge Time
Minimum Drying Time
Maximum Drying Time

Temperature Limits
Calibration Parameters
```

Use ESP-IDF NVS.

---

# 37. Settings Validation

Before saving:

```text
Cooling Temperature < Drying Temperature

Circulation Run Time < Circulation Interval

Maximum Drying Time > Minimum Drying Time

All temperatures within safe limits

All timer values > 0
```

Invalid settings must be rejected.

---

# 38. Phase 8 - Batch Manager

## Objective

Implement batch lifecycle management.

When the operator starts a batch:

```text
Generate Batch ID
Record Start Time
Record Initial Sensor Values
Initialize Batch Log
```

Example:

```text
BATCH-0001
```

During the batch:

```text
Record Sensor Data
Record State
Record Actuator Status
Record Faults
```

At completion:

```text
Record End Time
Calculate Durations
Save Summary
Close Log
```

---

# 39. Phase 9 - Data Logger

## Objective

Implement reliable SD card logging.

Recommended logging rate:

```text
1 second
```

Data:

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

---

# 40. Logging Architecture

The Logger should use a queue.

```text
Process Controller
       |
       v
 Logging Queue
       |
       v
 Logger Task
       |
       v
 SD Card
```

The Process Controller should not block while writing to the SD card.

---

# 41. Phase 10 - HMI Implementation

## Objective

Implement the touchscreen interface using LVGL.

Recommended implementation order:

```text
1. Dashboard
2. Batch Setup
3. Drying
4. Cooling
5. Discharge
6. Complete
7. Manual Control
8. Settings
9. History
10. Alarms
11. System Information
```

---

# 42. Dashboard Implementation

Display:

```text
System State
Moisture
Temperature
Humidity
EC

Heater
Fan
Elevator
Door

Batch Status
```

Buttons:

```text
START
SETTINGS
HISTORY
```

---

# 43. Batch Setup Implementation

Allow the operator to review:

```text
Target Moisture
Drying Temperature
Cooling Temperature

Circulation Mode
Circulation Interval
Circulation Run Time

Discharge Time
```

The Start button should only become available when PRECHECK requirements can be satisfied.

---

# 44. Drying Screen Implementation

Display:

```text
DRYING

Current Moisture
Target Moisture

Temperature
Humidity

Heater
Fan
Elevator

Elapsed Time
```

The screen should update without blocking the control task.

---

# 45. Cooling Screen Implementation

Display:

```text
COOLING

Current Temperature
Cooling Target

Fan ON
Heater OFF
Elevator OFF

Cooling Time
```

---

# 46. Discharge Screen Implementation

Display:

```text
DISCHARGING

Door OPEN
Elevator ON

Remaining Time
```

A progress indicator may be included.

---

# 47. Alarm Screen

Display:

```text
Fault Code
Fault Description
Timestamp
Current State
```

Example:

```text
F001
OVER TEMPERATURE

Heater has been disabled.
Check the dryer before continuing.
```

The operator must acknowledge the fault where appropriate.

Acknowledgement must not bypass an active safety condition.

---

# 48. Manual Control Implementation

Manual control is intended for:

- Commissioning
- Maintenance
- Troubleshooting

Controls:

```text
Heater
Fan
Elevator
Door Open
Door Close
```

Safety restrictions remain active.

Manual mode must not allow unsafe heater operation.

---

# 49. Phase 11 - Local Web Dashboard

## Objective

Provide remote local monitoring.

Initial implementation is read-only.

Display:

- Current process state
- Sensor values
- Actuator status
- Batch information
- Faults
- Settings

---

# 50. Web API Implementation

Implement:

```text
GET /api/status
GET /api/sensors
GET /api/actuators
GET /api/batch
GET /api/settings
GET /api/history
GET /api/faults
```

The web server reads system data.

It does not directly control actuators.

---

# 51. Phase 12 - Power Loss Recovery

Implement startup recovery detection.

At important state transitions, save recovery information:

```text
Batch ID
State
Timestamp
Moisture
Temperature
```

After restart:

```text
Previous Batch Detected
```

The system should display:

```text
Previous batch was interrupted.
Manual confirmation required.
```

The heater must remain OFF until the operator starts a new or approved recovery operation.

---

# 52. Phase 13 - Diagnostics

Implement system diagnostics.

Monitor:

```text
CPU usage
Free heap
Task status
Sensor status
Modbus status
SD status
RTC status
Wi-Fi status
System uptime
Fault count
```

Display through:

```text
System Information
```

---

# 53. Phase 14 - Full Hardware Integration

Connect the complete system.

```text
ESP32-S3
 |
 +-- Display
 |
 +-- SD
 |
 +-- RTC
 |
 +-- RS485
 |     +-- Hot-Air Sensor
 |     +-- Chamber Sensor
 |     +-- Relay Board
 |
 +-- ADC
       +-- Moisture/EC Sensor
```

Actuators:

```text
Relay Board
 |
 +-- Heater Control
 +-- Fan
 +-- Elevator
 +-- Door Open
 +-- Door Close
```

---

# 54. Integration Sequence

Do not connect everything at once.

Recommended sequence:

```text
1. ESP32 + Display
2. ESP32 + SD
3. ESP32 + RTC
4. ESP32 + RS485
5. RS485 + Sensor 1
6. RS485 + Sensor 2
7. RS485 + Relay Board
8. ADC + Moisture Sensor
9. Elevator
10. Fan
11. Door
12. Heater Control
```

This makes troubleshooting easier.

---

# 55. Phase 15 - Dry Run Testing

Before using rice, perform a complete dry run.

The system should execute:

```text
IDLE
 ->
PRECHECK
 ->
DRYING
 ->
COOLING
 ->
DISCHARGING
 ->
COMPLETE
```

Use simulated or controlled sensor values where appropriate.

Verify:

```text
[ ] State transitions
[ ] Heater hysteresis
[ ] Fan control
[ ] Elevator circulation
[ ] Door operation
[ ] Cooling sequence
[ ] Discharge timer
[ ] Fault handling
[ ] Logging
[ ] HMI
```

---

# 56. Phase 16 - Sensor Validation

Before real drying tests, validate the sensors.

## Temperature

Compare against a trusted reference thermometer.

## Humidity

Compare against a reference humidity measurement where possible.

## Moisture

Compare rice moisture readings against a calibrated/reference moisture meter.

## EC

Compare against a suitable reference method or known test sample.

---

# 57. Moisture Calibration Procedure

Recommended procedure:

1. Prepare rice samples with known moisture levels.
2. Measure each sample using a reference moisture meter.
3. Measure the same samples using the dryer sensor.
4. Record ADC/voltage values.
5. Calculate the calibration curve.
6. Store calibration parameters.
7. Repeat the measurements.
8. Calculate measurement error.

Example:

| Reference | Sensor |
|---:|---:|
| 10% | 10.3% |
| 12% | 11.8% |
| 14% | 14.2% |
| 16% | 15.7% |
| 18% | 18.4% |

The final calibration curve should be based on actual test results.

---

# 58. Phase 17 - Empty Chamber Test

Run the dryer without rice.

Objectives:

- Verify airflow
- Verify temperature rise
- Verify heater response
- Verify fan operation
- Verify sensor placement
- Verify circulation mechanism
- Verify door operation
- Verify emergency shutdown

Record:

```text
Temperature vs Time
Humidity vs Time
Heater State
Fan State
```

---

# 59. Phase 18 - Low-Risk Rice Test

Begin with a small amount of rice before running a full 10 kg batch.

Objectives:

- Verify moisture measurement
- Verify rice circulation
- Verify temperature uniformity
- Verify drying behavior
- Verify discharge

Monitor for:

- Hot spots
- Poor circulation
- Sensor errors
- Uneven drying
- Elevator jams
- Door problems

---

# 60. Phase 19 - 10 kg Validation Test

After successful small-scale testing, perform the full 10 kg test.

Test conditions should be documented:

```text
Rice Variety
Initial Moisture
Ambient Temperature
Ambient Humidity

Target Moisture
Drying Temperature
Cooling Temperature

Circulation Mode
Circulation Interval
Circulation Run Time
```

Record the complete batch log.

---

# 61. Drying Performance Metrics

Evaluate:

### Moisture Reduction

```text
Initial Moisture
        -
Final Moisture
```

### Drying Time

```text
Batch Start
     ->
Target Moisture
```

### Temperature Stability

Measure:

```text
Minimum Temperature
Maximum Temperature
Average Temperature
```

### Moisture Uniformity

Take samples from different areas of the batch.

Compare final moisture levels.

---

# 62. Acceptance Testing

The complete system should pass:

## Functional

```text
[ ] Batch starts correctly
[ ] Precheck works
[ ] Drying works
[ ] Cooling works
[ ] Discharge works
[ ] Batch completes
```

## Sensor

```text
[ ] Temperature readings valid
[ ] Humidity readings valid
[ ] Moisture readings valid
[ ] EC readings valid
```

## Actuator

```text
[ ] Heater control works
[ ] Fan control works
[ ] Elevator works
[ ] Door works
```

## Safety

```text
[ ] Emergency stop works
[ ] Overtemperature protection works
[ ] Sensor failure handling works
[ ] Elevator timeout works
[ ] Door timeout works
[ ] Power-loss recovery works
```

## Data

```text
[ ] Batch logging works
[ ] SD storage works
[ ] Batch history works
[ ] RTC timestamps work
```

---

# 63. Performance Testing

Test the system continuously for a complete drying cycle.

Monitor:

```text
CPU Usage
Free Heap
Task Stability
SD Logging
Modbus Communication
Sensor Stability
Display Responsiveness
```

The system should not experience:

- Memory leaks
- Watchdog resets
- Uncontrolled actuator operation
- Missing critical logs
- Unexpected state transitions

---

# 64. Fault Injection Testing

Intentionally simulate failures.

Test:

```text
Sensor disconnected
RS485 cable disconnected
SD card removed
Door timeout
Elevator timeout
Emergency stop
Overtemperature
Power interruption
Invalid configuration
```

For each fault record:

```text
Fault Detected
Safe State
Fault Display
Recovery Procedure
```

---

# 65. Commissioning Checklist

Before normal operation:

```text
Hardware
[ ] All wiring verified
[ ] Grounding verified
[ ] Fuses/protection installed
[ ] Emergency stop tested
[ ] Thermal protection tested

Sensors
[ ] Temperature calibrated
[ ] Humidity validated
[ ] Moisture calibrated
[ ] EC validated

Actuators
[ ] Heater tested
[ ] Fan tested
[ ] Elevator tested
[ ] Door tested

Software
[ ] State machine tested
[ ] Safety tested
[ ] HMI tested
[ ] Logging tested
[ ] Web dashboard tested

Process
[ ] Empty chamber test completed
[ ] Small rice test completed
[ ] 10 kg test completed
```

---

# 66. Recommended Development Order

The actual coding sequence should be:

```text
01. Project Initialization
02. Display and LVGL
03. SD Driver
04. RTC Driver
05. RS485 Driver
06. Modbus Driver
07. Temperature/RH Sensors
08. Relay Driver
09. ADC Driver
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

# 67. Recommended Implementation Milestones

## Milestone 1 - Controller Running

```text
ESP32-S3
Display
LVGL
SD
RTC
```

System boots successfully.

---

## Milestone 2 - Sensors Running

```text
Temperature
Humidity
Moisture
EC
```

All sensor values are visible on the HMI.

---

## Milestone 3 - Actuators Running

```text
Heater
Fan
Elevator
Door
```

All actuators can be tested independently.

---

## Milestone 4 - Safety Running

The system can detect:

```text
Overtemperature
Sensor Failure
Emergency Stop
Actuator Timeout
```

---

## Milestone 5 - Automatic Drying

The system can execute:

```text
PRECHECK
DRYING
COOLING
```

automatically.

---

## Milestone 6 - Complete Batch

The system executes:

```text
DRYING
 ->
COOLING
 ->
DISCHARGING
 ->
COMPLETE
```

without manual intervention.

---

## Milestone 7 - Data System

The system records:

```text
Sensor Data
Batch Data
Faults
History
```

to the SD card.

---

## Milestone 8 - Monitoring

The local web dashboard displays real-time dryer information.

---

# 68. Version Control Strategy

Use Git for source control.

Recommended branches:

```text
main
develop
feature/*
bugfix/*
```

Example:

```text
feature/rs485-driver
feature/moisture-sensor
feature/process-controller
feature/lvgl-dashboard
feature/sd-logger
```

Each feature should be tested before merging.

---

# 69. Commit Strategy

Use small, meaningful commits.

Examples:

```text
Add ESP32-S3 project foundation
Add RS485 driver
Add Modbus RTU communication
Add temperature sensor manager
Add relay driver
Add actuator manager
Add safety manager
Add dryer state machine
Add circulation manager
Add batch logger
Add drying HMI
Add local web dashboard
```

Avoid large commits containing unrelated changes.

---

# 70. Documentation During Development

Maintain documentation for:

- Pin assignments
- Modbus addresses
- Modbus registers
- Sensor calibration
- Relay mapping
- Configuration parameters
- Fault codes
- Test results
- Wiring changes
- Mechanical changes
- Firmware versions

Any hardware change should be reflected in the hardware documentation.

Any software behavior change should be reflected in the relevant software documentation.

---

# 71. Configuration Baseline

Initial firmware configuration:

```text
Target Moisture       = 14%
Drying Temperature    = 50°C
Hysteresis            = 2°C
Cooling Temperature   = 35°C

Circulation Mode      = Interval
Circulation Interval  = 2 minutes
Circulation Run Time  = 10 seconds

Discharge Time        = 60 seconds
```

These values are starting parameters.

They must be validated during actual rice drying tests.

---

# 72. Future Software Enhancements

The following should not be implemented until the base system is stable:

- PID temperature control
- Advanced moisture prediction
- Automatic drying recipes
- Remote cloud monitoring
- Mobile application
- Remote actuator control
- Energy monitoring
- Solar production monitoring
- Battery monitoring
- Inverter communication
- AI-based drying optimization

The initial goal is a reliable offline dryer controller.

---

# 73. Final Implementation Sequence

The complete development flow is:

```text
                     START
                       |
                       v
              Project Foundation
                       |
                       v
                Hardware Drivers
                       |
                       v
                 Sensor System
                       |
                       v
                Actuator System
                       |
                       v
                 Safety System
                       |
                       v
                Process Control
                       |
                       v
                     HMI
                       |
                       v
                  Data Logger
                       |
                       v
                Web Dashboard
                       |
                       v
              Full Integration
                       |
                       v
                 Dry Run Test
                       |
                       v
              Empty Chamber Test
                       |
                       v
               Small Rice Test
                       |
                       v
                10 kg Test
                       |
                       v
             Performance Validation
                       |
                       v
                 Commissioning
                       |
                       v
                  PRODUCTION
```

---

# 74. Definition of Done

The project is considered ready for normal operation when:

### Hardware

```text
[ ] All hardware installed
[ ] Wiring verified
[ ] Electrical protection installed
[ ] Thermal protection installed
[ ] Emergency stop tested
```

### Software

```text
[ ] All drivers functional
[ ] Sensors functional
[ ] Actuators functional
[ ] Safety Manager functional
[ ] Process Controller functional
[ ] HMI functional
[ ] Logger functional
[ ] Settings functional
[ ] Web monitoring functional
```

### Process

```text
[ ] PRECHECK works
[ ] DRYING works
[ ] COOLING works
[ ] DISCHARGING works
[ ] COMPLETE works
[ ] FAULT handling works
```

### Validation

```text
[ ] Moisture sensor calibrated
[ ] Temperature validated
[ ] Empty chamber test passed
[ ] Small batch test passed
[ ] 10 kg batch test passed
[ ] Drying performance documented
[ ] Safety tests passed
```

---

# 75. Final Goal

The implementation should produce a reliable offline rice drying controller capable of managing a 10 kg batch from manually loaded rice through drying, cooling, discharge, and batch completion.

The final automatic process is:

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
     +--> Fan ON
     |
     +--> Heater Hysteresis
     |
     +--> Rice Circulation
     |
     +--> Sensor Monitoring
     |
     +--> Data Logging
     |
     v
TARGET MOISTURE
     |
     v
COOLING
     |
     +--> Heater OFF
     +--> Fan ON
     +--> Elevator OFF
     |
     v
COOLING TEMPERATURE
     |
     v
DISCHARGING
     |
     +--> Door OPEN
     +--> Elevator ON
     |
     v
DISCHARGE TIMER
     |
     +--> Elevator OFF
     +--> Door CLOSED
     |
     v
COMPLETE
```

The implementation should prioritize reliability and safety over feature count.

The system should first become a reliable dryer controller before adding advanced automation or energy-monitoring features.