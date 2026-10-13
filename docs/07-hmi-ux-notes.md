# HMI UX Notes — Phase 7 Baseline

**Status:** Deferred. Do not redesign the product HMI until a design guideline exists.

**Related code:** `firmware/main/ui/` (ui_manager + screens)  
**Related plan:** [MASTERPLAN.md](MASTERPLAN.md) Phase 7

---

## 1. Decision (2026-10-13)

The Phase 7 touchscreen HMI is **functionally working**: process screens, logging, settings, history, alarms, and manual/commissioning controls are wired through the application layer.

Operator feedback: the UI is **not user-friendly**. A major visual and interaction redesign is required before the HMI is treated as operator-ready.

**For now:**

- Keep the current HMI as a **bench / commissioning baseline**
- Document UX debt here
- Write a proper **HMI design guideline** when development resumes
- Do **not** spend implementation time on a cosmetic redesign until that guideline is agreed

---

## 2. What Exists Today (baseline)

Screens implemented:

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

## 4. Design Guideline — Placeholder

When HMI work resumes, create:

```text
docs/08-hmi-design-guideline.md   (name may change)
```

That document should define at least:

1. **Audience split** — daily operator vs technician/commissioning
2. **Screen map** — primary path for one batch (Idle → Setup → Run → Complete)
3. **Information hierarchy** — what is large/always visible vs secondary
4. **Touch targets** — minimum sizes, spacing, contrast for the 7" panel
5. **Typography and color** — state colors, fault urgency, disabled states
6. **Navigation model** — fewer top-level tabs; how Manual/Settings are reached
7. **Copy rules** — short verbs, confirmations, fault messaging
8. **Non-negotiables** from firmware architecture (command path, safety authority, lock rules)

Until that guideline exists, treat any HMI PR that is primarily visual as out of scope.

---

## 5. Acceptance for a Future Redesign

A redesign is done when operators can, without coaching:

```text
[ ] Start a batch from a clear primary action
[ ] Read moisture, temperature, and state at a glance during DRYING
[ ] Understand the next step during COOLING and DISCHARGING
[ ] Find and acknowledge a fault without hunting
[ ] Keep Manual / sensor commissioning out of the normal batch path
[ ] Use large enough controls for field touch
```

Functional Phase 7 acceptance (logging + responsiveness) remains separate in MASTERPLAN Phase 7.
