# HMI Design Guideline — Modern SCADA

**Status:** Active and **implemented** in `firmware/main/ui/` (Figma Rice Dryer Design port).  
**Design source:** [Rice Dryer Design](https://www.figma.com/design/NQj2frIOuMvcB1I1deLPbU/Rice-Dryer-Design) (`NQj2frIOuMvcB1I1deLPbU`)  
**Supersedes redesign guidance in:** [07-hmi-ux-notes.md](07-hmi-ux-notes.md)  
**Panel:** Waveshare ESP32-S3 Touch LCD 7B — 1024 × 600, LVGL 8  
**Code:** `firmware/main/ui/` (`theme/`, `widgets/`, `trend_buffer/`, `ui_manager/`, screens)

---

## 1. Purpose

Defines the operator-ready SCADA look and interaction model: warm charcoal industrial theme, high-visibility live values, live trends, clear process screens, and a split between daily operator and technician paths.

Architecture non-negotiables stay unchanged:

- UI commands go only through Process Controller / Safety Manager
- LVGL updates hold `lvgl_port_lock()` briefly
- START is gated on precheck readiness
- Fault ACK must not clear an active over-temperature trip
- Manual / Settings / History / System do not auto-leave when process state changes

---

## 2. Audiences

| Mode | Who | Screens |
|---|---|---|
| Operator | Daily batch runs | Home, Setup, Run, Complete, Alarms |
| Technician | Bench / maintenance | More → Manual, Settings, History, System, Commission |

Operator path must work without opening More.

---

## 3. Screen map (implemented)

Primary batch path:

```text
Home → Batch Setup → Run (Drying / Cooling / Discharging) → Complete
                         ↓
                      Alarms (Fault)
```

| Process state | Screen |
|---|---|
| IDLE | Home |
| PRECHECK | Batch Setup |
| DRYING / COOLING / DISCHARGING | Run (one screen, restyled by state) |
| COMPLETE | Complete |
| FAULT | Alarms |

Sticky (no auto-leave): Manual, Settings, History, System, More, Commission, Calibrate Moisture.

Legacy separate Drying / Cooling / Discharge screens are no longer in the build; Run replaces them.

**Boot splash** (full-screen, not in nav): shows Display / SD / RTC / Moisture / Core / Logger / RS485 / Relay / Wi-Fi bring-up, then auto-advances to Home.

Technician: More → System → **Calibrate Moisture** (3-point piecewise) and **Commission Sensors**.

No Power / Solar / Battery UI — those signals are not implemented in firmware. No EC UI (EC removed).

---

## 4. Shell layout (implemented)

```text
┌──────────────────────────────────────────────────────────────┐
│ Brand · State chip · Batch/phase · Date + 12h clock  (88 px) │
│ Fault strip (48 px, only when active_fault ≠ 0)              │
├──────────────────────────────────────────────────────────────┤
│ Content 1024×448 (or 1024×400 with fault strip)              │
├──────────────────────────────────────────────────────────────┤
│ Home │ Run │ Alarms │ More                         (64 px)   │
└──────────────────────────────────────────────────────────────┘
```

- Header **88 px**: machine brand, ALL-CAPS state chip, batch ID + phase, date + clock
- Content height **448 px** normal; **400 px** when the fault strip is visible
- Bottom nav **64 px**: icon + label; selected tab = solid teal rounded pill
- Phase status tracks process state live; ephemeral banners clear on state change
- Header clock: date line + 12-hour time line

---

## 5. Visual system (implemented)

### Palette (warm charcoal SCADA — Figma)

| Token | Hex | Use |
|---|---|---|
| `bg` | `#171A19` | Screen background |
| `surface` | `#222625` | Cards / tiles |
| `surface_alt` | `#2B302E` | Nested panels, unselected nav |
| `border` | `#3A403D` | Card edges |
| `text` | `#F5F1E8` | Primary text |
| `cream` | `#F2E8CF` | KPI / large values |
| `text_muted` | `#B8B5AC` | Labels / secondary |
| `dim` | `#777C78` | Inactive |
| `accent` / `teal` | `#0F766E` | Primary actions / healthy |
| `teal_soft` | `#CCFBF1` | Soft process accent |
| `amber` | `#EAB308` | Warning / drying heat |
| `orange` | `#D97745` | Discharge / attention |
| `red` | `#DC2626` | Fault / destructive |
| `green` | `#22C55E` | Ready / OK |
| `heater` | `#EAB308` | Heater ON accent |
| `on_primary` | `#FFFFFF` | Text on saturated buttons |
| `fault_bg` / `fault_strip` | `#7F1D1D` | Active fault card / strip |

### Typography and copy

| Role | Font |
|---|---|
| Title | Montserrat ≈26 |
| KPI value | Montserrat ≈32 / 40 where flash allows |
| Body | Montserrat 16 |
| Caption | Montserrat 14 |

- **Title Case** for on-screen labels and buttons (`Start Batch`, `Acknowledge Fault`)
- Prefer full words; avoid unexplained abbreviations (`ACK`)
- State chip uses **ALL CAPS** (`IDLE`, `DRYING`, `FAULT`) to match Figma

### Spacing and touch

- Card radius: **8 px**; card padding: 12–16 px
- Primary button height: **64 px**
- Click debounce: 450 ms on the same control
- Content shell is not scroll-draggable; nested scroll only for long lists

### State colors

| State | Chip color |
|---|---|
| IDLE | muted |
| PRECHECK | teal soft |
| DRYING | amber |
| COOLING | teal soft |
| DISCHARGING | orange |
| COMPLETE | teal |
| FAULT | red |

---

## 6. Gauges and charts (implemented)

### Bar gauges (`lv_bar` — replaces circular meters)

| Screen | Gauge | Notes |
|---|---|---|
| Run | Moisture % | Horizontal bar 0–30 %; value + **Target** + range markers |
| Run | Chamber °C | Horizontal bar 0–80 °C; target = drying or cooling setpoint |

Home uses metric cards (large value + detail), not bar gauges.

### Charts (`lv_chart`)

- Dual series on Run (+ Complete snapshot): moisture + chamber, ~5 minutes at 1 Hz in RAM (`trend_buffer`)
- No SD CSV replay in v1
- Series updated each second; widgets are not recreated every tick

### Actuator status

Heater, Fan, Elevator, Door — LED + status text (`ON` / `OFF` / `RUNNING` / `OPEN` / `CLOSED`).

---

## 7. Screen content (implemented)

### Home (code: `dashboard/`)

Operator label **Home**; screen folder is `firmware/main/ui/screens/dashboard/`.

- Four metric cards: rice moisture, hot-air temp, chamber temp, chamber humidity
- Actuator status row (heater / fan / elevator / door)
- Status note + **Start Batch** (320×64); **Acknowledge Interrupt** when needed

### Batch Setup

- 2-column settings snapshot cards + readiness checklist
- **Edit Settings** / **Start**; **Acknowledge Interrupt** when needed

### Run (code: `run/`)

One screen for DRYING / COOLING / DISCHARGING; restyled by process state.

- Stacked horizontal bar gauges + dual trend chart
- Horizontal actuator status row
- Phase timer card + **Abort Batch** with confirm dialog

### Complete

- Four result KPIs + trend snapshot
- **History** / **New Batch**

### Alarms

- Split layout: active fault card + **Acknowledge Fault** (left); Recent Alarms list (right)
- Fault strip under header while a fault is active

### More → Manual / Settings / History / System / Commission / Calibrate

- More: 2×2 menu tiles
- Manual: per-actuator rows with On/Off (or Open/Close)
- Settings: editable grid including PID gains, window, max hot-air (no hysteresis)
- History: scrollable batch rows
- System: status tiles + **Commission Sensors** + **Calibrate Moisture**
- Commission: live sensor rows + address assign actions
- Calibrate Moisture: 3-point voltage capture + reference %; IDLE/COMPLETE only

---

## 8. Firmware notes

- Theme / widgets: `firmware/main/ui/theme/`, `firmware/main/ui/widgets/`
- Trend RAM buffer: `firmware/main/ui/trend_buffer/`
- Nav / status icons: `LV_SYMBOL_*` and drawn LED circles (no remote Figma asset URLs in firmware)
- `CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION` is **disabled** in `sdkconfig.defaults` to avoid an early boot panic when tiny PSRAM XIP page-alignment gaps are registered with TLSF
- LVGL chart / bar / Montserrat fonts enabled in `sdkconfig.defaults`

---

## 9. Acceptance

```text
[x] Design guideline checked in
[x] Shell matches Figma (88 / 448|400 / 64 + optional fault strip)
[x] Home / Run / Alarms layouts match Figma screenshots with live data wiring
[x] Remaining screens (Setup, Complete, More, Manual, Settings, History, System, Commission) updated
[x] Bottom nav ≤ 4 primary items; Manual/Settings under More
[x] Primary buttons ≥ 64 px; Abort Batch confirms
[x] Commands only via Process Controller
[x] Title Case buttons; ALL-CAPS state chips; header phase + 12-hour clock
[x] No temporary Figma asset URLs in firmware
[ ] Full dry-run glanceability / logging soak on hardware (ongoing bench)
```
