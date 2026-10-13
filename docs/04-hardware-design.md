# 04 - Hardware Design

## 1. Purpose

This document defines the hardware architecture, electrical interfaces, actuator connections, sensor connections, power distribution, and hardware safety requirements for the 10 kg/batch Solar Hybrid Rice Dryer.

The design uses an ESP32-S3 as the main machine controller.

The system is powered through a hybrid solar power system consisting of:

- 620 W bifacial solar panel
- ECGSOLAX 2000 W 12 V hybrid inverter
- Built-in 100 A MPPT
- 100 Ah battery
- AC input

The hybrid inverter manages the power source and battery system.

The ESP32 controls the rice drying machine only.

---

# 2. System Hardware

## 2.1 Main Components

| Component | Quantity | Function |
|---|---:|---|
| Waveshare ESP32-S3-Touch-LCD-7B | 1 | Main controller and HMI |
| RS485 8-channel Modbus relay | 1 | Actuator control |
| 0-5 V moisture/EC sensor | 1 | Rice moisture and EC measurement |
| RS485 temperature/humidity sensor | 2 | Hot-air and chamber monitoring |
| TinyRTC | 1 | Real-time clock |
| ADS1115 | 1 | 16-bit I2C ADC for moisture and EC |
| SD card | 1 | Data logging |
| 620 W bifacial solar panel | 1 | Solar power source |
| ECGSOLAX hybrid inverter | 1 | Power management |
| 100 Ah battery | 1 | Energy storage |
| Heater | 1 | Rice drying heat source |
| Fan | 1 | Air circulation and cooling |
| Elevator motor | 1 | Rice circulation and discharge |
| Linear actuator | 1 | Discharge door |
| SSR / contactor | 1 | Heater power switching |

---

# 3. Main Controller

The main controller is the Waveshare ESP32-S3-Touch-LCD-7B.

The controller is responsible for:

- Drying process control
- Sensor acquisition
- Actuator control
- Safety monitoring
- HMI
- Data logging
- RTC integration
- Local Wi-Fi
- Web dashboard

The touchscreen provides the primary user interface.

The SD card built into the display system is used for local batch and system data storage.

---

# 4. Controller Interface Architecture

The ESP32-S3 interfaces with the rest of the system through:

```text id="j80z1s"
                 ESP32-S3
                    |
       +------------+-------------+
       |                          |
       v                          v
     RS485                       I2C
       |                          |
       v                          +---- TinyRTC
 Sensors/Relay                    |
                                  +---- ADS1115
                                         |
                                         v
                                    Moisture / EC
```

Additional interfaces:

```text id="g9bq2v"
ESP32-S3
   |
   +---- Touchscreen
   |
   +---- SD Card
   |
   +---- Wi-Fi
```

The final GPIO assignments shall be defined after confirming the exact Waveshare board pinout and reserved interfaces.

---

# 5. Power Architecture

The main power architecture is:

```text id="8gcz7r"
                 620 W Bifacial Panel
                         |
                         v
               ECGSOLAX Hybrid Inverter
                    2000 W / 12 V
                    100 A MPPT
                         |
              +----------+----------+
              |                     |
              v                     v
         100 Ah Battery          AC Input
              |                     |
              +----------+----------+
                         |
                         v
                  Power Distribution
                         |
             +-----------+-----------+
             |                       |
             v                       v
       Control Power             Dryer Loads
             |                       |
             v                       v
     ESP32 / Sensors         Heater / Fan / Motors
```

The inverter is responsible for:

- Solar power conversion
- MPPT
- Battery charging
- AC input
- Power-source management
- Inverter operation

The ESP32 does not directly control these functions.

---

# 6. Power Distribution

The electrical system should be divided into separate power domains.

Recommended structure:

```text id="h1w73h"
Hybrid Inverter
      |
      v
Main Protection / Distribution
      |
      +----------------------+
      |                      |
      v                      v
Control Power           High-Power Loads
      |                      |
      v                      +---- Heater
ESP32 / Sensors              |
                             +---- Fan
                             |
                             +---- Elevator
                             |
                             +---- Door Actuator
```

The exact voltage rails shall be determined by the final component requirements.

A dedicated regulated supply should be used for the ESP32 and sensitive electronics.

---

# 7. Control Electronics Power

The ESP32-S3 and sensor electronics should receive regulated and stable power.

The control power system should include:

- Appropriate DC/DC converter
- Input fuse
- Reverse-polarity protection where required
- Over-voltage protection
- Appropriate filtering
- Common grounding strategy

The control supply should be isolated from electrical noise as much as practical.

Motor and heater switching should not directly share sensitive analog signal paths.

---

# 8. Heater Power Architecture

The heater is a high-power load.

The ESP32 shall not directly switch the heater's power circuit.

Recommended architecture:

```text id="9mlf8r"
ESP32-S3
    |
    v
RS485 Relay Controller
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

The SSR or contactor shall be selected based on:

- Heater voltage
- Heater current
- Load type
- Switching frequency
- Duty cycle
- Thermal requirements
- Required electrical isolation

The final heater power circuit shall include appropriate:

- Circuit protection
- Fuse or breaker
- Grounding
- Emergency shutdown
- Thermal protection

---

# 9. Heater Thermal Safety

Hardware thermal protection shall be independent of the ESP32 firmware.

Recommended protection layers:

```text id="bxy3ga"
Temperature Sensor
       |
       v
ESP32 Software Control
       |
       v
Heater Control
```

and independently:

```text id="q1u7l6"
Independent Thermal Cutoff
       |
       v
Heater Power Circuit
```

The independent thermal cutoff should remove heater power if an unsafe temperature is reached even when the controller or software fails.

---

# 10. Fan Hardware

The fan provides:

- Heated-air circulation
- Drying airflow
- Cooling airflow

The fan is controlled through the relay controller.

Architecture:

```text id="y4e8nd"
ESP32-S3
    |
    v
RS485 Relay
    |
    v
R3
    |
    v
Fan Control
```

If the fan motor is AC or otherwise requires additional switching hardware, an appropriately rated contactor, relay, or motor-control device shall be used.

The fan circuit should include suitable motor protection.

---

# 11. Elevator Hardware

The elevator is an internal rice circulation mechanism.

It is not a rice-loading mechanism.

The operator manually loads rice into the drying chamber.

The elevator performs two functions:

1. Rice circulation during drying.
2. Rice movement during discharge.

Architecture:

```text id="j8q58o"
ESP32-S3
    |
    v
RS485 Relay Controller
    |
    v
R1
    |
    v
Motor Control
    |
    v
Elevator Motor
    |
    +---- Rice Circulation
    |
    +---- Discharge
```

The motor and gearbox shall be sized for the required rice load and duty cycle.

The design should support both:

- Continuous operation
- Intermittent operation

---

# 12. Elevator Motor Protection

The elevator motor should have appropriate electrical protection.

Depending on the selected motor:

- Fuse or breaker
- Motor overload protection
- Thermal protection
- Appropriate contactor or relay
- Mechanical protection

Software shall also implement a runtime timeout.

Hardware protection and software timeout serve different purposes and should not be treated as substitutes for each other.

---

# 13. Elevator Duty Cycle

The elevator shall support:

### Continuous Mode

The motor remains ON throughout the applicable drying period.

### Interval Mode

The motor operates periodically.

Example:

```text id="x2p0if"
Interval = 2 minutes
Run Time = 10 seconds
```

The motor runs for 10 seconds and then remains OFF for the configured interval before the next circulation cycle.

The final mechanical design must confirm that the motor, gearbox, shaft, bearings, and elevator mechanism can tolerate the selected duty cycle.

---

# 14. Discharge Door Hardware

The discharge door uses a linear actuator.

The actuator requires two directional controls:

```text id="ev6ywy"
R4 -> OPEN
R5 -> CLOSE
```

Recommended architecture:

```text id="xvfg2w"
ESP32-S3
    |
    v
RS485 Relay
    |
    +---- R4 ----> Linear Actuator OPEN
    |
    +---- R5 ----> Linear Actuator CLOSE
```

OPEN and CLOSE commands must be electrically and logically interlocked.

---

# 15. Discharge Door Position Feedback

Limit switches are recommended.

Suggested feedback:

```text id="d7x2nq"
Door Open Limit
Door Closed Limit
```

The controller can then verify:

- Door fully open
- Door fully closed
- Door movement
- Door movement timeout

If position feedback is not implemented in the first prototype, a conservative movement timer shall be used.

However, position feedback is strongly recommended for the final machine.

---

# 16. Discharge Sequence

Hardware sequence:

```text id="yn5t5e"
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
```

The elevator shall not start discharge until the door has been commanded open.

Where limit switches are available, the controller should verify that the door is open before starting the elevator.

---

# 17. Temperature/Humidity Sensor Hardware

Two RS485 Modbus temperature/humidity sensors are used.

Suggested locations:

```text id="0g2skc"
Sensor 1
Hot-Air Path
    |
    v
Measures heated air

Sensor 2
Drying Chamber
    |
    v
Measures chamber environment
```

The sensors shall be installed where they are protected from:

- Direct water exposure
- Mechanical damage
- Excessive vibration
- Direct contact with rice
- Unacceptable heat exposure beyond their rated temperature

The sensor locations shall be validated during commissioning.

---

# 18. RS485 Electrical Architecture

The RS485 bus shall connect:

- ESP32-S3
- Hot-air temperature/humidity sensor
- Chamber temperature/humidity sensor
- RS485 relay controller

Conceptually:

```text id="5u6j9z"
                RS485 BUS
                    |
        +-----------+-----------+
        |           |           |
        v           v           v
     Sensor 1    Sensor 2    Relay
     Address 1   Address 2   Address 10
```

All devices must have unique Modbus addresses.

---

# 19. RS485 Wiring

The RS485 network should use:

- Twisted-pair communication cable
- A/B differential signals
- Appropriate termination
- Common reference where required
- Shielding where appropriate

Recommended physical arrangement:

```text id="gyx8rc"
ESP32
  |
  +---- Sensor 1
  |
  +---- Sensor 2
  |
  +---- Relay Controller
```

The final topology should minimize unnecessary cable branching.

Termination resistors shall be installed according to the actual bus topology and cable length.

---

# 20. RS485 Noise Control

The machine contains several potential electrical-noise sources:

- Heater switching
- Fan motor
- Elevator motor
- Linear actuator
- Relay coils
- High-current wiring

The RS485 network should therefore be physically separated from high-power wiring.

Recommended practices:

- Separate signal and power cable routing.
- Avoid long parallel runs with motor cables.
- Use twisted-pair RS485 cable.
- Use shielded cable where appropriate.
- Ground shields according to the system grounding strategy.
- Keep relay and motor wiring away from analog inputs.
- Add appropriate suppression to inductive loads.

---

# 21. Moisture / EC Sensor Interface

The selected moisture/EC sensor provides a 0-5 V analog output.

An ADS1115 reads that output. The ESP32-S3 must not receive the 0-5 V signal on a GPIO.

Recommended architecture:

```text id="e8xqjd"
Moisture / EC Sensor
       |
       | 0-5 V
       v
┌──────────────────┐
│ Signal Interface │
│                  │
│ Protection       │
│ RC Filter        │
│ Scaling if needed│
└────────┬─────────┘
         |
         v
      ADS1115
         |
         | I2C
         v
      ESP32-S3
```

The ADS1115 analog input must stay within VDD + 0.3 V. Two supply choices are acceptable:

- Power the ADS1115 from 5 V so a 0-5 V sensor is inside its absolute maximum, set the programmable gain full-scale range to ±6.144 V, and level-shift SDA and SCL to the 3.3 V ESP32.
- Power the ADS1115 from 3.3 V and scale the sensor output with a divider and protection so the pin stays inside the selected full-scale range.

The exact scaling, gain, and supply shall be confirmed from the ADS1115 datasheet and the moisture sensor datasheet.

---

# 22. Analog Input Protection

The analog input circuit should include:

- Voltage scaling
- Current limiting
- RC filtering
- Over-voltage protection
- Stable ground reference

The design shall prevent the sensor voltage from exceeding the ADS1115 input range, and shall keep 0-5 V off every ESP32 pin.

The analog wiring should be kept away from:

- Heater power wiring
- Motor wiring
- Relay wiring
- High-current battery wiring

---

# 23. Moisture Sensor Calibration

The sensor interface shall support calibration against a trusted reference measurement.

Recommended calibration process:

```text id="9x34j6"
Sensor Reading
      |
      v
Reference Moisture Measurement
      |
      v
Calibration Curve
      |
      v
Firmware Calibration Parameters
      |
      v
Moisture %
```

Calibration parameters shall be stored in non-volatile memory.

The calibration method shall be finalized after actual sensor testing.

---

# 24. Moisture Sensor Installation

The moisture sensor must be installed so that it provides a representative measurement of the rice.

The mechanical design should consider:

- Sensor placement
- Rice contact
- Rice flow
- Temperature exposure
- Cleaning
- Sensor replacement
- Cable protection

The exact mounting position shall be determined during prototype testing.

---

# 25. EC Measurement

If the selected sensor provides EC measurement, the EC signal shall be connected according to the sensor's actual datasheet.

EC shall be treated primarily as a monitoring parameter.

It shall not automatically determine drying completion unless validated experimentally.

The primary drying completion parameter remains rice moisture.

---

# 26. RTC Hardware

The TinyRTC provides real-time clock functionality.

Architecture:

```text id="g07yr4"
ESP32-S3
    |
    | I2C
    v
 TinyRTC
```

The RTC should have an appropriate backup battery installed.

The controller shall periodically read the RTC for timestamps.

---

# 27. ADS1115

The ADS1115 is the analog-to-digital converter for the moisture and EC sensor.

It is a 16-bit I2C ADC with four single-ended inputs. Moisture and EC use separate inputs when the sensor provides both signals.

```text id="3q9gh2"
ESP32-S3
    |
    | I2C
    v
 ADS1115
    |
    +---- Moisture
    |
    +---- EC
```

The ADS1115 shares the controller I2C bus with the TinyRTC. Its address is set by the ADDR pin. The default address is 0x48, and it must not collide with the TinyRTC, the GT911 touch controller, or the board I/O expander.

The firmware selects the input channel and programmable gain, then converts the raw code to voltage before calibration.

---

# 28. SD Card Hardware

The Waveshare ESP32-S3-Touch-LCD-7B provides SD card functionality.

The SD card shall store:

- Batch data
- Process logs
- Fault logs
- System logs
- Export files

The SD card should be considered non-critical to immediate actuator safety.

If the SD card fails:

- The drying controller must continue to enforce safety.
- Heater control must continue safely.
- The system should report the logging failure.
- The fault should be recorded in RAM where possible.

---

# 29. Relay Controller

The system uses an 8-channel RS485 Modbus RTU relay controller.

Initial mapping:

| Relay | Function | Type |
|---|---|---|
| R1 | Elevator | Motor control |
| R2 | Heater | SSR/contactor control |
| R3 | Fan | Motor/load control |
| R4 | Door OPEN | Linear actuator |
| R5 | Door CLOSE | Linear actuator |
| R6 | Spare | TBD |
| R7 | Spare | TBD |
| R8 | Spare | TBD |

The relay controller shall use a unique Modbus address.

Initial example:

```text id="v9gc2q"
Relay Address = 10
```

The final address shall be configurable if supported by the relay hardware.

---

# 30. Relay Output Safety

Relay outputs shall not be assumed to be suitable for every final load.

Each output must be evaluated based on:

- Voltage
- Current
- Inrush current
- Load type
- Switching frequency
- Inductive characteristics

For motors and high-power loads, additional contactors, SSRs, motor drivers, or suppression components may be required.

---

# 31. Emergency Stop

The machine should have a physical emergency-stop mechanism.

The emergency stop should remove hazardous energy independently of normal software control where practical.

At minimum, the emergency stop should disable the heater.

Depending on the final mechanical and electrical design, it may also remove power from:

- Elevator motor
- Fan
- Linear actuator

The exact emergency-stop circuit shall be finalized during electrical safety design.

---

# 32. Hardware Safety Layers

Safety should use multiple layers.

```text id="m3x7is"
Layer 1
Software Limits
       |
       v
Layer 2
Relay / Driver Control
       |
       v
Layer 3
Electrical Protection
       |
       v
Layer 4
Independent Thermal Protection
       |
       v
Layer 5
Emergency Stop
```

No single software function should be the only protection against a dangerous condition.

---

# 33. Grounding and Wiring

The machine shall use a defined grounding strategy.

Recommended principles:

- Proper protective earth connection for applicable AC equipment.
- Separate low-voltage signal routing from high-power wiring.
- Avoid uncontrolled ground loops.
- Use appropriate cable sizes.
- Use ferrules and properly secured terminals.
- Label all field wiring.
- Provide strain relief.
- Protect cables from heat and moving mechanisms.

The final grounding architecture shall be reviewed based on the actual power system and enclosure design.

---

# 34. Electrical Noise Management

Potential noise sources include:

- Heater switching
- Relay coils
- DC motors
- AC motors
- Linear actuators
- Inverter switching
- Battery currents

Recommended countermeasures:

- Physical separation
- Twisted-pair signal wiring
- Shielded communication cables where appropriate
- RC filtering for analog inputs
- Flyback suppression for DC inductive loads where applicable
- Snubbers or appropriate suppression for AC inductive loads
- Proper grounding
- Short analog signal paths
- Separate power rails for sensitive electronics

---

# 35. Mechanical Hardware Architecture

The drying chamber should provide:

- 10 kg rice capacity
- Controlled airflow
- Internal rice circulation
- Heating area
- Temperature/humidity measurement
- Moisture measurement
- Discharge outlet

Conceptual arrangement:

```text id="y3f5j0"
                  HOT AIR
                    |
                    v
             ┌──────────────┐
             │    Heater    │
             └──────┬───────┘
                    |
                    v
             ┌──────────────┐
             │    FAN /     │
             │ AIRFLOW PATH │
             └──────┬───────┘
                    |
                    v
       ┌──────────────────────────┐
       │      DRYING CHAMBER      │
       │                          │
       │   Rice                   │
       │                          │
       │   ↑                  ↓   │
       │   │   CIRCULATION    │   │
       │   └── ELEVATOR ─────┘   │
       │                          │
       └────────────┬─────────────┘
                    |
                    v
              DISCHARGE DOOR
                    |
                    v
              DISCHARGE PATH
```

The exact airflow and elevator geometry shall be finalized during mechanical design.

---

# 36. Sensor Placement

Recommended sensor placement:

```text
Hot-Air Sensor
    |
    v
Near heated-air path

Chamber Sensor
    |
    v
Inside or near the drying chamber

Moisture Sensor
    |
    v
Representative rice measurement location
```

Sensor placement must avoid:

- Direct heater radiation
- Direct mechanical impact
- Excessive vibration
- Unrepresentative airflow
- Direct contact with moving mechanisms

---

# 37. Hardware State at Startup

The hardware shall initialize to a safe condition.

Expected startup state:

```text id="q3p0ey"
Heater = OFF
Fan = OFF
Elevator = OFF
Door = Safe / CLOSED
```

The controller shall verify relay communication before enabling process control.

---

# 38. Hardware State During DRYING

```text id="ukf3g6"
Fan       = ON

Heater    = Hysteresis Controlled

Elevator  =
    Continuous
    OR
    Interval

Door      = CLOSED
```

The door shall remain closed during normal drying.

---

# 39. Hardware State During COOLING

```text id="9gc6we"
Heater    = OFF
Fan       = ON
Elevator  = OFF*
Door      = CLOSED
```

`*` The initial design does not require elevator operation during cooling.

This behavior can be changed later if testing shows that continued circulation improves cooling performance.

---

# 40. Hardware State During DISCHARGING

```text id="9s8s8v"
Heater    = OFF
Fan       = OFF
Door      = OPEN
Elevator  = ON
```

The elevator runs until the configured discharge timer expires.

Then:

```text id="0ofq4m"
Elevator = OFF
Door     = CLOSED
```

---

# 41. Hardware State During FAULT

For critical faults:

```text id="y9r5l0"
Heater = OFF
```

Other actuator states depend on the specific fault.

The safety design shall define safe behavior for:

- Motor fault
- Door fault
- Sensor fault
- Over-temperature
- Communication failure
- Power recovery

The system shall avoid creating a secondary hazard while attempting to recover from a fault.

---

# 42. Recommended Protection Components

The final electrical design should evaluate the need for:

- Main circuit breaker
- DC fuse
- AC fuse
- Branch circuit protection
- Heater fuse/breaker
- Motor protection
- DC/DC input fuse
- Emergency stop
- Thermal cutoff
- Surge protection
- Reverse-polarity protection
- Over-voltage protection
- Ground fault protection where applicable

Component ratings must be selected based on the actual measured or rated load currents.

---

# 43. Wiring Documentation

The final implementation shall include:

- Wiring diagram
- Terminal map
- Relay map
- RS485 wiring diagram
- Power distribution diagram
- Grounding diagram
- Fuse/breaker schedule
- Connector pinout
- ESP32 GPIO assignment
- Sensor address list
- Cable identification

All field connections should be labeled.

---

# 44. Hardware Validation

Before full system operation, each hardware subsystem shall be tested independently.

Recommended sequence:

```text id="9rc8qo"
1. Power System
        |
        v
2. ESP32 / HMI
        |
        v
3. RS485 Communication
        |
        v
4. Temperature Sensors
        |
        v
5. Moisture / EC Interface
        |
        v
6. Relay Controller
        |
        v
7. Fan
        |
        v
8. Elevator
        |
        v
9. Discharge Door
        |
        v
10. Heater Control
        |
        v
11. Safety Systems
        |
        v
12. Complete Drying Test
```

---

# 45. Hardware Acceptance Criteria

The hardware design shall be considered ready for integrated testing when:

1. ESP32 power is stable.
2. Touchscreen operates reliably.
3. SD card operates reliably.
4. RTC provides valid time.
5. RS485 communication is stable.
6. Both temperature/humidity sensors respond correctly.
7. The ADS1115 reads moisture and EC inside its allowed input range, and 0-5 V never reaches an ESP32 pin.
8. Moisture readings can be calibrated.
9. Relay controller responds correctly.
10. Elevator motor operates safely.
11. Fan operates safely.
12. Discharge actuator operates in both directions.
13. Door OPEN/CLOSE interlock works.
14. Heater control operates through the appropriate SSR/contactor.
15. Independent thermal protection is functional.
16. Emergency stop disables hazardous operation as designed.
17. Motor protection is functional.
18. Electrical noise does not cause unacceptable sensor or RS485 failures.
19. Wiring is properly labeled and secured.
20. The complete machine can operate without exposing the ESP32 or low-voltage electronics to unsafe voltage levels.

---

# 46. Hardware Design Principles

The hardware design shall follow these principles:

### Safety First

High-power and hazardous loads require independent electrical protection.

### Electrical Separation

Keep low-level signals away from high-current and motor wiring.

### Modular Design

Sensors, relay control, power conversion, and control electronics should be replaceable independently.

### Serviceability

Provide access to:

- Controller
- Relay board
- Fuses
- Power supplies
- Sensors
- Motor connections
- Heater control components

### Expandability

Unused relay channels and controller interfaces should remain available for future functions.

### No Direct 0-5 V to ESP32

All 0-5 V sensor outputs connect to the ADS1115, with supply, gain, and protection chosen so the signal stays inside the ADS1115 ratings. They do not connect to an ESP32 GPIO.

### No Direct High-Power Heater Switching

The ESP32/relay system should control an appropriately rated heater switching stage.

---

# 47. Hardware Summary

The final hardware architecture is:

```text
                    SOLAR / AC POWER
                           |
                           v
                ECGSOLAX HYBRID INVERTER
                           |
                           v
                    POWER DISTRIBUTION
                           |
             +-------------+-------------+
             |                           |
             v                           v
       CONTROL SYSTEM              DRYER SYSTEM
             |                           |
             v                           |
        ESP32-S3                        |
             |                           |
     +-------+-------+                   |
     |               |                   |
     v               v                   |
   RS485            I2C                  |
     |               |                   |
     |               +--> TinyRTC        |
     |               |                   |
     |               +--> ADS1115        |
     |                     |             |
     |                     +--> Moisture / EC
     |                                   |
     +--> Temp/RH Sensor 1               |
     +--> Temp/RH Sensor 2               |
     +--> RS485 Relay                    |
                    |                    |
          +---------+---------+----------+
          |         |         |
          v         v         v
       Heater      Fan     Elevator
          |                   |
          |                   |
          +------> Chamber <--+
                         |
                         v
                  Discharge Door
```

The architecture provides a clear separation between power management, control electronics, sensing, actuation, and mechanical drying functions.

The design is intentionally modular so the system can be expanded without changing the core ESP32 control architecture.