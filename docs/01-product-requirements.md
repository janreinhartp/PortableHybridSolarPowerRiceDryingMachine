# 01 - Product Requirements

## 1. Product Overview

The Solar Hybrid Rice Dryer is a 10 kg/batch automated rice drying machine designed for small-scale rice drying operations.

The system uses a hybrid solar power system with battery storage and AC input. An ESP32-S3-based controller manages the drying process, monitors sensors, controls actuators, provides a touchscreen HMI, stores drying records, and provides a local web dashboard.

The system operates without internet access.

### Primary Goals

- Dry up to 10 kg of rice per batch.
- Maintain a controlled drying temperature.
- Monitor rice moisture during the drying process.
- Circulate rice inside the drying chamber using an elevator.
- Provide controlled airflow during heating, drying, and cooling.
- Automatically stop heating when target moisture is reached.
- Automatically cool the rice before discharge.
- Automatically discharge the dried rice.
- Record drying data to an SD card.
- Provide a local touchscreen HMI.
- Provide a local Wi-Fi web dashboard.
- Operate using solar, battery, or AC power through the hybrid inverter.

---

# 2. System Scope

The system includes:

- ESP32-S3 controller
- 7-inch touchscreen HMI
- Rice drying chamber
- Internal rice circulation elevator
- Heating system
- Air circulation fan
- Automatic discharge door
- Rice moisture and EC sensor
- Temperature and humidity sensors
- RTC
- RS485 Modbus communication
- Relay control system
- SD card data logging
- Local Wi-Fi web dashboard
- Hybrid solar power system
- Battery storage

The system does not manage the solar inverter's internal power-management functions.

The hybrid inverter handles:

- Solar charging
- Battery charging
- AC input
- Solar input
- Power source selection
- Battery protection
- Inverter operation
- MPPT operation

The ESP32 controls only the rice dryer system.

---

# 3. Target Capacity

### Batch Capacity

Target:

10 kg of rice per batch

The system should support consistent drying operation within the intended 10 kg batch capacity.

The user manually loads the rice into the drying chamber before starting a batch.

The system does not automatically load rice into the chamber.

---

# 4. Drying Process

The drying process is based primarily on rice moisture.

The user:

1. Loads rice into the drying chamber.
2. Closes the chamber.
3. Starts the drying process through the touchscreen HMI.
4. The system performs a precheck.
5. The fan starts.
6. The heater starts according to the configured temperature control.
7. The elevator starts circulating rice.
8. The system continuously monitors moisture and temperature.
9. The system continues drying until the target moisture is reached.
10. The heater turns OFF.
11. The fan remains ON for cooling.
12. The system waits until the configured cooling temperature is reached.
13. The discharge door opens.
14. The elevator runs for the configured discharge duration.
15. The elevator stops.
16. The discharge door closes.
17. The batch is marked complete.

---

# 5. Process States

The main process state machine is:

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

Any active process state may transition to:

```text
FAULT
```

## 5.1 IDLE

Machine waiting for the next batch.

Expected outputs:

- Heater OFF
- Fan OFF
- Elevator OFF
- Discharge door CLOSED

The user may configure batch parameters and system settings.

---

## 5.2 PRECHECK

The controller verifies the required system conditions before starting.

Precheck includes:

- Required sensors available
- Temperature sensors responding
- Moisture sensor available
- RS485 communication available
- RTC available
- SD card available
- Required settings available
- Discharge door in a safe position
- No active critical fault
- Actuator system ready

The batch starts only when required precheck conditions pass.

---

## 5.3 DRYING

The drying state performs heating, airflow, and rice circulation.

During drying:

- Fan is ON.
- Heater operates according to the configured temperature control.
- Elevator circulates rice.
- Moisture is continuously monitored.
- Temperature is continuously monitored.
- Humidity is monitored.
- Data is logged to the SD card.

### Elevator Circulation Modes

The elevator supports two configurable circulation modes.

#### Continuous Mode

The elevator runs continuously during drying.

```text
Elevator ON
      |
      v
Continue until drying completes
```

#### Interval Mode

The elevator runs periodically based on configurable settings.

Example:

```text
Circulation Interval: 2 minutes
Circulation Run Time: 10 seconds
```

Process:

```text
Elevator ON
   |
   v
Run for 10 seconds
   |
   v
Elevator OFF
   |
   v
Wait 2 minutes
   |
   v
Repeat
```

The circulation interval and elevator run duration should be configurable through the Settings screen.

### Fan Operation During Drying

The fan remains ON throughout the heating and drying process.

The fan provides airflow through the drying chamber while the heater is operating.

If the heater temporarily turns OFF because the temperature reaches the upper control limit, the fan remains ON.

---

# 6. Temperature Control

The initial heater control method is hysteresis control.

Example configuration:

```text
Target Temperature: 50°C
Hysteresis: 2°C
```

Example behavior:

```text
Temperature <= 48°C
        |
        v
Heater ON

Temperature >= 52°C
        |
        v
Heater OFF
```

The fan remains ON throughout the DRYING state.

PID temperature control may be added in a future version.

---

# 7. Moisture-Based Drying Completion

The primary drying completion condition is rice moisture.

Example:

```text
Target Moisture: 14%
```

When measured moisture reaches or falls below the configured target:

```text
Moisture <= Target Moisture
        |
        v
Drying Complete
        |
        v
Heater OFF
        |
        v
Cooling
```

Additional protection limits should be supported:

- Minimum drying time
- Maximum drying time
- Maximum allowed temperature
- Sensor fault detection

These limits prevent incorrect sensor readings or abnormal operating conditions from causing unsafe operation.

---

# 8. Cooling Process

After the target moisture is reached:

- Heater turns OFF.
- Fan remains ON.
- Elevator stops unless a future requirement specifies continued circulation during cooling.
- Temperature continues to be monitored.

The system remains in the COOLING state until the configured cooling temperature is reached.

Example:

```text
Cooling Temperature: 35°C
```

When:

```text
Temperature <= 35°C
```

the system proceeds to discharge.

---

# 9. Discharging Process

The discharge process automatically removes the dried rice from the chamber.

Sequence:

```text
Cooling Complete
      |
      v
Discharge Door OPEN
      |
      v
Elevator ON
      |
      v
Run for configured discharge time
      |
      v
Elevator OFF
      |
      v
Discharge Door CLOSED
      |
      v
COMPLETE
```

### Discharge Timer

The discharge duration must be configurable.

Example:

```text
Discharge Time: 60 seconds
```

The elevator runs only for the configured discharge period.

The controller must prevent simultaneous activation of the discharge door's OPEN and CLOSE outputs.

---

# 10. Sensors

The system uses the following sensors.

## 10.1 Rice Moisture / EC Sensor

Purpose:

- Monitor rice moisture.
- Monitor EC when supported by the sensor configuration.

The sensor provides an analog output. An ADS1115 16-bit I2C ADC reads that output. The 0-5 V signal must not be connected to an ESP32 GPIO.

The exact sensor output configuration must be confirmed from the sensor datasheet before final hardware implementation.

---

## 10.2 Hot-Air Temperature / Humidity Sensor

Installed near the heated air path.

Purpose:

- Monitor hot-air temperature.
- Monitor hot-air relative humidity.
- Support drying process monitoring.
- Support heater safety.

Communication:

RS485 Modbus RTU.

---

## 10.3 Drying Chamber Temperature / Humidity Sensor

Installed inside or near the drying chamber.

Purpose:

- Monitor chamber temperature.
- Monitor chamber relative humidity.
- Support drying monitoring.
- Support cooling control.

Communication:

RS485 Modbus RTU.

---

## 10.4 RTC

The RTC provides timekeeping when the system is offline.

Purpose:

- Timestamp sensor logs.
- Timestamp batch records.
- Timestamp fault records.
- Maintain date/time without internet access.

---

# 11. Actuators

## 11.1 Heater

Purpose:

Provide controlled heat for rice drying.

The heater is controlled based on the configured drying temperature.

The heater must be switched through an appropriately rated power-control device such as an SSR or contactor.

The ESP32 must not directly switch a high-power heater.

---

## 11.2 Fan

Purpose:

- Move heated air through the drying chamber.
- Maintain airflow during drying.
- Cool the rice after heating.
- Support uniform drying conditions.

Fan behavior:

```text
DRYING  → ON
COOLING → ON
DISCHARGING → OFF
IDLE → OFF
```

---

## 11.3 Elevator

The elevator is the primary rice circulation mechanism.

It performs two functions:

1. Circulates rice inside the drying chamber during drying.
2. Moves rice toward the discharge path during final discharge.

Drying operation supports:

- Continuous circulation
- Interval circulation

Discharge operation uses:

- Configurable discharge timer

---

## 11.4 Discharge Door

The discharge door controls the rice outlet.

Functions:

- OPEN
- CLOSE

The system must prevent simultaneous OPEN and CLOSE commands.

Where supported by the mechanical design, limit switches should confirm the door's OPEN and CLOSED positions.

---

# 12. Touchscreen HMI

The touchscreen is the primary machine interface.

The HMI should provide the following screens.

## Dashboard

Display:

- Current machine state
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
- Active alarms
- Current batch information

---

## Batch Setup

User-configurable parameters:

- Target moisture
- Drying temperature
- Cooling temperature
- Maximum drying time
- Minimum drying time
- Elevator circulation mode
- Circulation interval
- Circulation run time
- Discharge time

---

## Drying Process

Display:

- Current moisture
- Target moisture
- Current temperature
- Target temperature
- Drying elapsed time
- Current machine state
- Elevator status
- Heater status
- Fan status
- Progress information
- Active alarms

---

## Manual Control

Authorized users should be able to manually control individual actuators for testing and maintenance.

Available controls:

- Heater
- Fan
- Elevator
- Discharge door OPEN
- Discharge door CLOSE

Manual controls must include appropriate safety interlocks.

Manual heater operation should require temperature and safety checks.

---

## History

Display completed batch records.

Information includes:

- Batch ID
- Start time
- End time
- Total duration
- Initial moisture
- Final moisture
- Target moisture
- Maximum temperature
- Average temperature
- Heater runtime
- Fan runtime
- Fault count
- Completion status

---

## Settings

System settings include:

### Drying Settings

- Target moisture
- Drying temperature
- Temperature hysteresis
- Minimum drying time
- Maximum drying time

### Elevator Settings

- Circulation mode
- Circulation interval
- Circulation run time
- Discharge time

### Cooling Settings

- Cooling temperature

### System Settings

- Date/time
- Temperature calibration
- Moisture calibration
- Sensor configuration
- Modbus addresses
- Logging settings
- Wi-Fi AP settings

---

## Alarm Screen

Display:

- Active alarms
- Alarm code
- Alarm description
- Time detected
- Machine state
- Sensor values
- Recommended operator action

---

# 13. Data Logging

The SD card is the primary local data storage.

The system must record:

- Sensor readings
- Machine state
- Actuator states
- Batch information
- Faults
- System events
- Configuration changes

The initial active drying logging interval is:

```text
1 second
```

The logging interval should be configurable in a future version if required.

---

# 14. Batch Data

Each completed batch should contain:

- Batch ID
- Start timestamp
- End timestamp
- Total drying duration
- Initial moisture
- Final moisture
- Target moisture
- Maximum temperature
- Average temperature
- Heater runtime
- Fan runtime
- Fault count
- Completion status

Raw process data should include:

```text
Timestamp
State
Moisture
EC
Hot-air temperature
Hot-air humidity
Chamber temperature
Chamber humidity
Heater status
Fan status
Elevator status
Door status
Fault
```

---

# 15. Fault Logging

The system should record:

- Sensor failures
- Modbus communication errors
- Moisture sensor errors
- Temperature sensor errors
- Over-temperature events
- Elevator faults
- Door faults
- SD card errors
- RTC errors
- Configuration errors
- Batch interruptions
- Power recovery events

Each fault record should include:

- Timestamp
- Fault code
- Description
- Machine state
- Sensor values
- Active actuator states

---

# 16. Local Web Dashboard

The system provides a local web dashboard through the ESP32 Wi-Fi Access Point.

Example:

```text
http://192.168.4.1
```

The system does not require internet access.

The web dashboard should provide:

- Machine status
- Sensor readings
- Actuator status
- Current batch information
- Active alarms
- Batch history
- Log viewing
- CSV export
- System information

The initial web dashboard is read-only for machine control.

The touchscreen HMI remains the primary control interface.

---

# 17. Power System

The dryer uses a hybrid solar power system.

Main components:

- 620 W bifacial solar panel
- ECGSOLAX 2000 W 12 V hybrid solar inverter
- Built-in 100 A MPPT
- 100 Ah battery
- AC input

The hybrid inverter manages the power system.

The dryer controller does not directly control:

- MPPT
- Solar charging
- Battery charging
- AC/solar source selection
- Inverter operation

The ESP32 only controls the dryer process and machine electronics.

---

# 18. Safety Requirements

The system must include software and hardware safety mechanisms.

### Temperature Safety

- Maximum temperature limit
- Heater shutdown on over-temperature
- Sensor failure detection
- Fan monitoring where supported

### Elevator Safety

- Motor timeout
- Fault handling
- Manual control interlock
- Appropriate motor protection

### Discharge Door Safety

- OPEN/CLOSE interlock
- Door movement timeout
- Limit switch support
- Safe startup position

### Sensor Safety

- Communication timeout
- Invalid reading detection
- Out-of-range detection
- Sensor disconnect detection

### Electrical Safety

- Proper circuit protection
- Appropriate fusing
- Proper grounding
- Separation of control and high-power wiring
- Proper protection for inductive loads
- Emergency stop or equivalent hardware safety mechanism

Critical safety functions should not rely solely on software.

---

# 19. Power Loss Recovery

The controller should detect unexpected power loss and recover safely when power returns.

After restart:

- Heater remains OFF.
- Fan remains OFF unless a defined recovery condition is implemented.
- Elevator remains OFF.
- Discharge door remains in a safe state.
- Previous batch status is reviewed.
- The operator is notified if a batch was interrupted.
- The system requires operator confirmation before restarting the batch.

The system must never automatically restart the heater after an uncontrolled power interruption.

---

# 20. Offline Operation

The dryer must operate without internet connectivity.

Required local functions:

- Drying control
- Sensor monitoring
- Touchscreen HMI
- RTC timekeeping
- SD logging
- Fault logging
- Batch history
- Local web dashboard
- CSV export

Internet access is not required for normal operation.

---

# 21. User Roles

The initial system may use a simple access model.

### Operator

Allowed to:

- Start a batch
- Stop a batch
- View sensor data
- View history
- Adjust permitted batch settings
- View alarms

### Maintenance / Admin

Allowed to:

- Configure system settings
- Calibrate sensors
- Configure Modbus addresses
- Test actuators
- View diagnostics
- Clear certain faults

---

# 22. Product Success Criteria

The prototype is considered successful when it meets the following requirements:

1. Processes up to 10 kg of rice per batch.
2. Allows manual loading of rice before batch start.
3. Starts the fan during the heating/drying cycle.
4. Controls the heater based on configured temperature.
5. Circulates rice using the elevator.
6. Supports continuous elevator circulation.
7. Supports interval elevator circulation.
8. Allows circulation interval configuration.
9. Allows circulation run-time configuration.
10. Detects the configured target moisture.
11. Turns the heater OFF after reaching target moisture.
12. Keeps the fan ON during cooling.
13. Detects the configured cooling temperature.
14. Opens the discharge door after cooling.
15. Runs the elevator for the configured discharge duration.
16. Stops the elevator after the discharge timer expires.
17. Closes the discharge door after discharge.
18. Records process data to the SD card.
19. Records faults and system events.
20. Provides batch history.
21. Provides CSV export.
22. Provides a local touchscreen HMI.
23. Provides a local web dashboard.
24. Operates without internet access.
25. Handles sensor and actuator faults safely.
26. Does not automatically restart heating after unexpected power loss.
27. Maintains reliable operation using the hybrid solar power system.