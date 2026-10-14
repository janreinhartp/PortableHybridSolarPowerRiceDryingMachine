# HMI Design Guideline — Modern SCADA

**Status:** Active and **implemented** in `firmware/main/ui/` (light industrial SCADA redesign).  
**Supersedes redesign guidance in:** [07-hmi-ux-notes.md](07-hmi-ux-notes.md)  
**Panel:** Waveshare ESP32-S3 Touch LCD 7B — 1024 × 600, LVGL 8  
**Code:** `firmware/main/ui/` (`theme/`, `widgets/`, `trend_buffer/`, `ui_manager/`, screens)

---

## 1. Purpose

Defines the operator-ready SCADA look and interaction model: light industrial theme, large gauges with Target under live values, live trends, clear process screens, and a split between daily operator and technician paths.

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

Sticky (no auto-leave): Manual, Settings, History, System, More, Commission.

Legacy separate Drying / Cooling / Discharge screens are no longer in the build; Run replaces them.

---

## 4. Shell layout (implemented)

```text
┌──────────────────────────────────────────────────────────────┐
│ State chip │ Batch ID │ Phase status │  Date + 12h clock     │
│            │          │              │  Fault strip          │
├──────────────────────────────────────────────────────────────┤
│                    Content area                              │
├──────────────────────────────────────────────────────────────┤
│   Home    │    Run    │   Alarms   │       More              │
└──────────────────────────────────────────────────────────────┘
```

- Phase status tracks process state live (Ready → Running Precheck → Drying → …)
- Ephemeral operator messages (Start, Save, Acknowledge) appear in the phase slot, then clear on state change
- Header clock: `Mon DD, YYYY  H:MM AM/PM` (12-hour with date)
- Bottom nav has four primary items; More opens Settings / History / Manual / System

---

## 5. Visual system (implemented)

### Palette (light industrial SCADA)

| Token | Hex | Use |
|---|---|---|
| `bg` | `#F1F5F9` | Screen background |
| `surface` | `#FFFFFF` | Cards / tiles |
| `surface_alt` | `#E2E8F0` | Nested panels, nav buttons |
| `border` | `#CBD5E1` | Card edges |
| `text` | `#0F172A` | Primary text |
| `text_muted` | `#475569` | Labels / secondary |
| `accent` | `#0369A1` | Process / interactive |
| `teal` | `#0F766E` | Healthy / complete |
| `amber` | `#B45309` | Warning / drying heat |
| `orange` | `#C2410C` | Discharge / attention |
| `red` | `#B91C1C` | Fault / dangerous |
| `green` | `#15803D` | Ready / OK LED |
| `heater` | `#EA580C` | Heater ON accent |
| `on_primary` | `#FFFFFF` | Text on saturated buttons / state chip |
| `fault_bg` | `#FEE2E2` | Active fault card fill |

### Typography and copy

| Role | Font |
|---|---|
| Title | Montserrat 24 / 28 |
| KPI value | Montserrat 28 / 32 |
| Body | Montserrat 16 / 20 |
| Caption | Montserrat 14 |

- **Title Case** for on-screen labels and buttons (`Start Batch`, `Acknowledge Fault`)
- Prefer full words; avoid unexplained abbreviations (`ACK`)
- State chip uses Title Case (`Idle`, `Drying`, …)

### Spacing and touch

- Card padding: 12–16 px
- Primary button height: ≥ 64 px
- Click debounce: 450 ms on the same control
- Content shell is not scroll-draggable; nested scroll only for long lists (e.g. Recent Alarms)

### State colors

| State | Chip / phase color |
|---|---|
| IDLE | muted |
| PRECHECK | accent |
| DRYING | amber |
| COOLING | accent |
| DISCHARGING | orange |
| COMPLETE | teal |
| FAULT | red |

---

## 6. Gauges and charts (implemented)

### Gauges (`lv_meter`)

| Screen | Gauge | Notes |
|---|---|---|
| Home / Run | Moisture % | 0–30 %; live value + **Target** under the value |
| Home / Run | Chamber °C | 0–80 °C; live value + **Target** (drying or cooling setpoint by phase) |

### Charts (`lv_chart`)

- Dual series on Run (+ Complete snapshot): moisture + chamber, ~5 minutes at 1 Hz in RAM (`trend_buffer`)
- No SD CSV replay in v1
- Series updated each second; widgets are not recreated every tick

### Actuator LEDs

Heater, Fan, Elevator, Door — colored when commanded on.

---

## 7. Screen content (implemented)

### Home

- Large moisture + chamber gauges with Target lines
- KPI tiles: Hot Air, Conductivity; actuator LEDs
- **Start Batch**; **Acknowledge Interrupt** when needed

### Batch Setup

- Settings summary + Readiness checklist
- **Start** / **Edit Settings** / **Acknowledge Interrupt**

### Run

- Mode title (Drying / Cooling / Discharging)
- Moisture + chamber gauges with Target under values
- Dual trend chart
- Actuator LEDs + **one** timer card (phase elapsed, or Remaining while discharging)
- **Abort Batch** with confirm dialog

### Complete

- Result KPIs + trend snapshot
- **New Batch**, **History**

### Alarms

- Active fault card + **Acknowledge Fault**
- **Recent Alarms** list (scrollable)

### More → Manual / Settings / History / System / Commission

- Technician pages; commissioning is nested under System → **Commission Sensors**

---

## 8. Firmware notes

- Theme / widgets: `firmware/main/ui/theme/`, `firmware/main/ui/widgets/`
- Trend RAM buffer: `firmware/main/ui/trend_buffer/`
- `CONFIG_SPIRAM_PRE_CONFIGURE_MEMORY_PROTECTION` is **disabled** in `sdkconfig.defaults` to avoid an early boot panic when tiny PSRAM XIP page-alignment gaps are registered with TLSF
- LVGL chart / meter / Montserrat 14–32 enabled in `sdkconfig.defaults`

---

## 9. Acceptance

```text
[x] Design guideline checked in
[x] Home shows moisture and chamber as large gauges (with Target)
[x] Run shows live dual trend (moisture + chamber)
[x] Process states have clear presentation (Run variants + Complete)
[x] Bottom nav ≤ 4 primary items; Manual/Settings under More
[x] Primary buttons ≥ 64 px; Abort Batch confirms
[x] Commands only via Process Controller
[x] Title Case copy; header phase status + 12-hour date/time
[ ] Full dry-run glanceability / logging soak on hardware (ongoing bench)
```
