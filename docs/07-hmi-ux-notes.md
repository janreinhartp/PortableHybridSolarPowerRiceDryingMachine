# HMI UX Notes — Phase 7 Baseline

**Status:** Historical Phase 7 UX debt record. The SCADA redesign described in [08-hmi-design-guideline.md](08-hmi-design-guideline.md) is **implemented** in `firmware/main/ui/`. Keep this file for context only.

**Related code:** `firmware/main/ui/` (theme, widgets, trend_buffer, ui_manager, screens)  
**Related plan:** [MASTERPLAN.md](MASTERPLAN.md) Phase 7

---

## 1. Decision (2026-10-13)

The Phase 7 touchscreen HMI was **functionally working** as a label-heavy baseline: process screens, logging, settings, history, alarms, and manual/commissioning controls wired through the application layer.

Operator feedback at the time: the UI was **not user-friendly**. A major visual and interaction redesign was required.

**Update (2026-10-14):** guideline agreed and landed — see [08-hmi-design-guideline.md](08-hmi-design-guideline.md). Items below describe the **pre-redesign** baseline.

---

## 2. What Existed Then (pre-redesign baseline)

Screens in the Phase 7 baseline (before the 08 SCADA redesign):

```text
Dashboard
Batch Setup
Drying / Cooling / Discharge / Complete
Manual Control
Settings
History
Alarms
System Info
```

Current (post-redesign) screen map: see [08-hmi-design-guideline.md](08-hmi-design-guideline.md) — unified **Run** screen replaces separate Drying / Cooling / Discharge.

Behavior that must be preserved in any redesign:

- Commands go through Process Controller / Safety Manager (no direct relay writes from UI)
- Active process states auto-navigate to the matching run screen
- Manual / Settings / History / System Info stay put until the operator leaves them
- START is gated on precheck readiness
- Fault acknowledge must not clear an active over-temperature condition
- Logger and control stay non-blocking (HMI refresh under `lvgl_port_lock()` from the shared status snapshot)

---

## 3. Known UX Problems (to address in the guideline)

These are observations from the Phase 7 baseline, not a final design.

### Layout and hierarchy

- Dense label-heavy layouts; primary values do not stand out enough for glanceability at 1024×600
- Top navigation exposes many destinations at once (Home / Setup / Run / Manual / Settings / History / Alarms / Info)
- Operator flow and commissioning tools share the same shell without a clear mode split

### Operator flow

- Dashboard vs Batch Setup vs Settings overlap; it is unclear what to do first on a normal batch day
- Abort / ACK INT / fault actions are easy to miss or confuse with maintenance controls
- Process screens are informative but not strongly guided (what matters now vs secondary detail)

### Touch and controls

- Small text and compact buttons; hard to use with gloves or at arm’s length
- Settings use many +/- steppers; slow and error-prone for common values
- Little confirmation for destructive actions (ABORT) beyond banner text

### Visual language

- Dark theme and status colors exist ad hoc; no locked palette, type scale, or spacing system
- Progress (moisture / discharge) is minimal; no strong “remaining time / next step” storytelling
- Alarm presentation is readable but not calm/urgent enough for field use

### Content mixing

- System Info still carries sensor Modbus address commissioning next to diagnostics
- Manual Control mixes everyday maintenance with unsafe-looking heater controls without strong warnings

---

## 4. Design Guideline — Superseded by 08

The guideline exists and is implemented:

```text
docs/08-hmi-design-guideline.md
```

It defines audience split, screen map, information hierarchy, touch targets, typography/color, navigation, copy rules, and architecture non-negotiables. Treat [08-hmi-design-guideline.md](08-hmi-design-guideline.md) as the active HMI source of truth; this file is historical context only.

---

## 5. Redesign Acceptance (pre-redesign goals)

These were the redesign goals from the Phase 7 baseline. They are **complete in firmware** per [08-hmi-design-guideline.md](08-hmi-design-guideline.md). Hardware glanceability / logging soak remains open in MASTERPLAN Phase 7 and Phase 9.

```text
[x] Start a batch from a clear primary action
[x] Read moisture, temperature, and state at a glance during DRYING
[x] Understand the next step during COOLING and DISCHARGING
[x] Find and acknowledge a fault without hunting
[x] Keep Manual / sensor commissioning out of the normal batch path
[x] Use large enough controls for field touch
```
