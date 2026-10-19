# ESP32-S3 GPIO Map — Waveshare ESP32-S3-Touch-LCD-7B

Locked pin assignments for the Hybrid Solar Power Rice Dryer firmware.

Board: Waveshare ESP32-S3-Touch-LCD-7B (1024 × 600 RGB, GT911, octal PSRAM).
Firmware reference: `firmware/main/board/gpio_map.h`

---

## 1. Locked I/O decisions (not on ESP32 GPIO)

| Topic | Decision |
|---|---|
| Door travel limits | **Not wired to the ESP32.** The linear actuator provides internal end-of-travel limit switches. Firmware only commands door OPEN/CLOSE relays (R4/R5) and must never energize both at once. |
| Emergency stop | **Pure electrical.** The E-stop drops the main contactor on the electrical panel and removes power from high-power loads. It is **not** an ESP32 digital input and is **not** handled in software. |

After an E-stop or power loss, the controller must still boot into a safe commanded state (heater/fan/elevator OFF) and require operator confirmation before restarting a batch.

---

## 2. I2C address map (shared bus)

Bus: **I2C0** — SDA `GPIO8`, SCL `GPIO9`, 400 kHz (RTC device uses 100 kHz).

| Address | Device | Notes |
|--------:|--------|--------|
| `0x5D` / `0x14` | GT911 touch | Address depends on INT strap at reset |
| `0x24` | IO expander | Backlight, resets, SD select |
| `0x68` | TinyRTC (DS1307) | Must not collide |
| `0x48` | ADS1115 | Phase 4 — ADDR pin default; **AIN2** moisture / **AIN3** EC (as wired) |

Do not add a second I2C controller.

---

## 3. Pins used by the board (do not reassign)

| GPIO | Function |
|-----:|----------|
| 0 | LCD RGB G3 |
| 1 | LCD RGB R3 |
| 2 | LCD RGB R4 |
| 3 | LCD RGB VSYNC |
| 4 | GT911 INT |
| 5 | LCD RGB DE |
| 6 | Demo LED (unused by product firmware; leave free or LED only) |
| 7 | LCD RGB PCLK |
| 8 | I2C SDA |
| 9 | I2C SCL |
| 10 | LCD RGB B7 |
| 11 | SDMMC CMD |
| 12 | SDMMC CLK |
| 13 | SDMMC D0 |
| 14 | LCD RGB B3 |
| 17 | LCD RGB B6 |
| 18 | LCD RGB B5 |
| 21 | LCD RGB G7 |
| 38 | LCD RGB B4 |
| 39 | LCD RGB G2 |
| 40 | LCD RGB R7 |
| 41 | LCD RGB R6 |
| 42 | LCD RGB R5 |
| 45 | LCD RGB G4 |
| 46 | LCD RGB HSYNC |
| 47 | LCD RGB G6 |
| 48 | LCD RGB G5 |

---

## 4. Reserved system pins (do not use for control I/O)

| GPIO | Function |
|-----:|----------|
| 19 | USB D- (native USB / JTAG) |
| 20 | USB D+ (native USB / JTAG) |
| 33–37 | Octal PSRAM |
| 43 | UART0 TX (CH343 console / flash) |
| 44 | UART0 RX (CH343 console / flash) |

---

## 5. Product control pins — onboard RS485

The Waveshare board already has an **SP3485** transceiver with **automatic direction control**.
Use the HY2.0 RS485 A/B header. Do **not** drive a separate DE/RE GPIO.

| GPIO | Direction | Function | Phase |
|-----:|-----------|----------|------:|
| 16 | Out | UART1 TX → transceiver DI | 3 |
| 15 | In | UART1 RX ← transceiver RO | 3 |

Schematic net names `RS485_TXD` / `RS485_RXD` are from the transceiver side and look swapped vs ESP UART names — that is expected (Waveshare FAQ).

UART for Modbus/RS485: **UART1** at **19200 8N1**.

| Modbus addr | Device |
|------------:|--------|
| 1 | **XY-MD03 hot-air** T/H (no DIP — set via Modbus reg `0x0101`) |
| 2 | **XY-MD03 chamber** T/H (no DIP — set via Modbus reg `0x0101`) |
| 10 | **N4D8B08 relay** (move DIP here before commissioning sensors) |

### XY-MD03 T/H probes (no DIP switches)

| Topic | Value |
|--------|--------|
| Wires | Red VCC (5–28 V), Black GND, Yellow A, White B |
| Baud | **19200 8N1** (product); factory default is often **9600** |
| Baud register | Holding **0x0102** (FC06 write). Common codes: **0 = 9600**, **1 = 14400**, **2 = 19200** |
| Read T/H | FC04 start **0x0001** count 2 → temp×10, humidity×10 |
| Set address | FC06 write holding **0x0101** = new ID (1–247) |

Factory address is usually **1**. Commission **one probe at a time** (relay should already be at address **10**).

### Eletechsup N4D8B08 relay (locked for this project)

| Topic | Requirement |
|--------|-------------|
| Power | **12 V DC** on relay board — RS485 alone does not power the module |
| Bus | **A↔A, B↔B**, **GND↔GND** between Waveshare RS485 header and relay |
| Baud | **19200 8N1** — set with the board’s **baud DIP** (see steps below), then power-cycle |
| Address | DIP **A0–A5** raw binary (A0=1, A1=2, A2=4, A3=8). Product target **10** = **A1+A3 ON**. (A0+A3 = 9 — wrong for product ID 10.) |
| Protocol | FC **0x06** write / FC **0x03** read holding regs; channels **0x0001–0x0008**; Close all = reg `0x0000` value `0x0800` |
| Open / Close | Value `0x0100` open, `0x0200` close (matches N4D8B08 command PDF) |

Relay channels: R1=1 … R5=5. R4/R5 must never both be ON.

---

## 5.1 RS485 baud commissioning → 19200 (do this once)

Product firmware talks at **19200 8N1**. Every device on the bus must match. Do **relay first**, then each probe, then flash/run the ESP at 19200.

**Order matters:** if the ESP is already flashed for 19200 but a device is still at 9600, that device will look “dead” until you change it (or temporarily run a 9600 tool).

### A. N4D8B08 relay — baud DIP

1. Power **off** the relay board (12 V).
2. Confirm address DIPs for product ID **10**: **A1 ON**, **A3 ON**, A0/A2/A4/A5 OFF.
3. Set the **baud-rate DIP pair** (often labeled **BR0 / BR1**, **B0 / B1**, or printed on the silk) to **19200**.

   Typical Eletechsup encoding (verify against your board silk / PDF — silkscreens differ):

   | BR1 | BR0 | Baud |
   |:---:|:---:|------|
   | OFF | OFF | 9600 (factory) |
   | OFF | ON | **19200** ← product |
   | ON | OFF | 38400 |
   | ON | ON | 115200 |

4. Power **on** the relay (12 V). Leave A/B/GND connected to the Waveshare RS485 header.
5. After the ESP is at 19200, confirm Manual Control can toggle fan/elevator (heater still OFF until safety checks).

### B. XY-MD03 hot-air probe — baud register (addr → 1)

Probes have **no baud DIP**. Change baud over Modbus while the ESP (or a USB-RS485 dongle) is still talking at the probe’s **current** baud (usually 9600).

1. Disconnect the chamber probe and the relay from the bus if they collide, or leave the relay at addr 10 (it will not answer at 9600 once set to 19200 — that is OK while you are still on 9600 for this step).
2. Connect **only one** XY-MD03 (factory ID usually **1**): VCC, GND, A, B.
3. With the host at **9600**, write holding register **0x0102** = **2** (19200) using FC06:
   - Firmware helper: `th_sensor_set_baud(1, 2)` (or whatever address the lone probe answers on).
   - Or any Modbus tool: slave = current ID, reg = `0x0102`, value = `2`.
4. **Power-cycle** the probe (remove VCC, wait 2 s, reconnect).
5. Assign address if needed: HMI **Commission Sensors → Assign Hot Air (1)**, then power-cycle again.
6. From here on, that probe only answers at **19200**.

### C. XY-MD03 chamber probe — baud register (addr → 2)

1. Disconnect the hot-air probe (already at 19200) so it does not confuse a 9600 host, **or** use a host already at 19200 if this second probe was already changed.
2. Connect **only** the second factory probe (usually still ID **1** @ **9600**).
3. At **9600**, write **0x0102** = **2**, then **power-cycle**.
4. HMI **Commission Sensors → Assign Chamber (2)**, power-cycle again.
5. Reconnect both probes + relay. Bus is now all **19200**: hot-air **@1**, chamber **@2**, relay **@10**.

### D. ESP32 firmware

1. Flash/run product firmware (`RS485_BAUD` = **19200** in `firmware/components/rs485/rs485.c`).
2. Boot → System Info / Commission: hot-air, chamber, and relay should show live.
3. If one device is silent: it is almost always still at **9600** or the wrong address — repeat that device’s steps only.

**Do not** use 115200 on this shared bus for the thesis baseline.

---

## 6. Free / spare GPIOs

| GPIO | Notes |
|-----:|-------|
| 6 | Board GPIO header (often the only clearly free digital I/O) |
| 26 | Not used (was wrongly claimed as DE/RE — onboard RS485 is auto-DE) |
| 27 | Spare if present on header |
| 28 | Spare if present on header |
| 29 | Spare if present on header |
| 30 | Spare if present on header |
| 31 | Spare if present on header |
| 32 | Spare if present on header |

Confirm availability on the physical header before wiring. Door limits and E-stop are not ESP32 inputs.

---

## 7. Actuators not on ESP32 GPIO

Heater, fan, elevator, and door open/close are controlled through the **RS485 Modbus relay board** (address 10), not direct ESP32 GPIO.

| Relay | Function |
|------:|----------|
| R1 | Elevator |
| R2 | Heater |
| R3 | Fan |
| R4 | Door OPEN |
| R5 | Door CLOSE |
| R6–R8 | Spare |

Door motion stops at the actuator’s internal limits. Software must still:
- Never assert R4 and R5 together
- Use configured run/timeouts where needed for process sequencing
- Leave the door in a safe commanded relay state after faults (see master plan)

---

## 8. Change control

Update this file and `firmware/main/board/gpio_map.h` together. RS485/Modbus firmware follows this map. Phase 4 ADS1115 work uses I2C address `0x48` on the same bus and must keep this map matched to the wired hardware.
