# ESP32-S3 GPIO Map — Waveshare ESP32-S3-Touch-LCD-7B

Locked pin assignments for the Hybrid Solar Power Rice Dryer firmware.

Board: Waveshare ESP32-S3-Touch-LCD-7B (1024 × 600 RGB, GT911, octal PSRAM).
Firmware reference: `firmware/main/board/gpio_map.h`

---

## 1. I2C address map (shared bus)

Bus: **I2C0** — SDA `GPIO8`, SCL `GPIO9`, 400 kHz.

| Address | Device | Notes |
|--------:|--------|--------|
| `0x5D` / `0x14` | GT911 touch | Address depends on INT strap at reset |
| `0x24` | IO expander | Backlight, resets, SD select |
| `0x68` | TinyRTC (DS1307) | Phase 2 — must not collide |
| `0x48` | ADS1115 (planned) | Phase 4 — ADDR pin default |

Do not add a second I2C controller.

---

## 2. Pins used by the board (do not reassign)

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

## 3. Reserved system pins (do not use for control I/O)

| GPIO | Function |
|-----:|----------|
| 19 | USB D- (native USB / JTAG) |
| 20 | USB D+ (native USB / JTAG) |
| 33–37 | Octal PSRAM |
| 43 | UART0 TX (CH343 console / flash) |
| 44 | UART0 RX (CH343 console / flash) |

---

## 4. Product control pins (Phase 2 reservation)

These pins are reserved for later phases. Firmware must not drive them for unrelated functions.

| GPIO | Direction | Function | Phase |
|-----:|-----------|----------|------:|
| 15 | Out | RS485 UART1 TX | 3 |
| 16 | In | RS485 UART1 RX | 3 |
| 26 | Out | RS485 DE/RE (high = transmit) | 3 |
| 27 | In | Door OPEN limit (active low, internal pull-up) | 4+ |
| 28 | In | Door CLOSE limit (active low, internal pull-up) | 4+ |
| 29 | In | Emergency stop (active low, internal pull-up) | 5 |

UART for Modbus/RS485: **UART1** at 9600 8N1 (confirmed when sensors are wired).

---

## 5. Still free (spare)

| GPIO | Notes |
|-----:|-------|
| 30 | Spare digital I/O |
| 31 | Spare digital I/O |
| 32 | Spare digital I/O |

Confirm availability on the physical header before wiring.

---

## 6. Actuators not on ESP32 GPIO

Heater, fan, elevator, and door open/close are controlled through the **RS485 Modbus relay board** (address 10), not direct ESP32 GPIO.

| Relay | Function |
|------:|----------|
| R1 | Elevator |
| R2 | Heater |
| R3 | Fan |
| R4 | Door OPEN |
| R5 | Door CLOSE |
| R6–R8 | Spare |

---

## 7. Change control

Update this file and `firmware/main/board/gpio_map.h` together. Do not start RS485 or ADS1115 work until this map matches the wired hardware.
