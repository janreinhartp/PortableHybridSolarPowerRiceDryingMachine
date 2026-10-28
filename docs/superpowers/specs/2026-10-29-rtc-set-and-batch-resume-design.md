# Design: RTC Set Clock HMI + Confirmed Batch Resume

**Date:** 2026-10-29  
**Status:** Implemented  
**Approach:** Minimal delta (Approach A)  
**Related:** `rtc_manager`, `batch_manager`, `process_controller`, System Info / Dashboard HMI

---

## 1. Goal

Close two as-built gaps before Phase 9 validation:

1. **Set Clock on HMI** — operators can set the DS1307 date/time without a serial tool.
2. **Confirmed batch resume** — after power loss mid-batch, reboot stays safe; Home offers **Resume** (continue same batch/phase, including DRYING with heater after recheck) or **Discard** (today’s ACK).

Non-goals: silent auto-resume, cloud time sync, web-based ACK/resume, persisting PID/circulation internals.

---

## 2. Set Clock

### 2.1 Entry and navigation

- New screen `UI_SCREEN_SET_CLOCK` under System Info (button **Set Clock**, sibling of Commission / Calibrate Moisture).
- Back returns to System Info.
- Mirror existing screen registration in `ui_manager` + `CMakeLists.txt`.

### 2.2 UI

- Title: **Set Clock**
- Editable fields with − / + : **Year**, **Month**, **Day**, **Hour**, **Minute** (second forced to `0` on Save).
- Live preview line: `YYYY-MM-DD HH:MM`
- Buttons: **Save**, **Back**
- Clamp ranges: year 2020–2099; month 1–12; day 1–max for month (simple calendar, no leap-edge UI); hour 0–23; minute 0–59.
- Seed draft from `rtc_manager_get` on build (or zeros + banner if RTC fail).

### 2.3 Behavior

- **Save** calls `rtc_manager_set(&draft)`. On success: banner “Clock Saved”; header updates on next `refresh_rtc`. On failure: banner with error.
- **Gate:** editable only when `process_controller_manual_control_allowed()` is true (IDLE / COMPLETE; not mid-batch). If blocked, Save disabled + note.
- Reuse existing `rtc_manager_set`; no new driver APIs required. `adjust_minutes` / `set_build_time` remain unused by this screen.

---

## 3. Confirmed batch resume

### 3.1 Policy (locked)

- Boot **never** silently restarts heaters or phase actuators.
- Boot always: IDLE + safe outputs; `batch_interrupted = true` if recovery blob present.
- Operator chooses:
  - **Resume** — restore same `batch_id` and saved phase (DRYING / COOLING / DISCHARGING), including heater under PID after a live sensor/safety recheck when phase is DRYING.
  - **Discard** — erase recovery (current ACK semantics); new START allowed.
- START remains refused while interrupted pending.

### 3.2 NVS recovery blob (versioned)

Replace magic `0xB6` single layout with versioned blob (new magic `0xB7`):

| Field | Type | Notes |
|---|---|---|
| `magic` | `uint8` | `0xB7` |
| `version` | `uint8` | `1` |
| `state` | `uint8` | `batch_process_state_t` / dryer phase |
| `batch_id` | `char[16]` | |
| `moisture` | `float` | last sample |
| `chamber_temp_c` | `float` | last sample |
| `drying_elapsed_s` | `uint32` | |
| `cooling_elapsed_s` | `uint32` | |
| `discharge_elapsed_s` | `uint32` | |
| `discharge_step` | `uint8` | process_controller discharge sub-step enum |
| `saved_at_s` | `uint32` | monotonic seconds at save (diagnostic) |

**Load rules:**

- Magic `0xB7` + `version == 1` + size match → interrupted pending; copy all fields into `batch_status_t` interrupted_* (extend struct).
- Legacy magic `0xB6` → treat as interrupted with elapsed timers **0** and discharge step **OPEN_DOOR**; still offer Resume/Discard (best-effort).
- Bad/missing blob → no interrupt.

**Save rules:**

- Write on phase entry (force) and throttled (~30 s) during DRYING / COOLING / DISCHARGING, including elapsed + discharge step from process_controller.
- Clear on: complete, abort, Discard, and after successful Resume handoff once the active run owns the blob again (same as today: active run keeps rewriting; pending flag cleared while running).

### 3.3 Process controller

New APIs:

- `process_controller_acknowledge_interrupted()` — unchanged meaning: **Discard**.
- `process_controller_resume_interrupted()` — new:
  1. Fail if no interrupted pending or RTC/sensors/safety not ready for resume gate (see below).
  2. Rehydrate `batch_manager` active batch from interrupted snapshot (`batch_id`, not a new sequence number).
  3. Restore `s_drying_elapsed_s` / cooling / discharge elapsed; reconstruct start timestamps as `now - elapsed`.
  4. Restore `s_discharge_step` if state is DISCHARGING.
  5. Set `s_state` to saved phase (only DRYING, COOLING, DISCHARGING allowed; if saved PRECHECK/FAULT/IDLE/COMPLETE → treat as Discard-equivalent refuse or force Discard path).
  6. Clear `interrupted_pending`; begin writing recovery as active run.
  7. Apply phase outputs via existing enter/tick paths (fan/circulation/heater rules unchanged).

**Resume safety gate (before leaving IDLE):**

- `sensor_manager` snapshot OK for hot-air, chamber, moisture (same spirit as precheck).
- `safety_manager` has no active fault.
- Relay bus OK.
- Then allow DRYING heater under existing PID + hot-air cap on subsequent ticks.

If gate fails: stay IDLE, interrupted still pending, banner explains (e.g. “Resume Blocked — Sensors”).

### 3.4 HMI

**Dashboard (primary):**

- When `batch_interrupted`:
  - Note shows batch id + phase name (expose interrupted state via `system_status` or getters).
  - Buttons: **Resume** (teal) and **Discard** (surface).
  - Start remains disabled via existing gates.
- When not interrupted: hide Resume/Discard; keep current Start / ACK-interrupt-only UI removed in favor of Discard naming (or keep label “Discard Interrupt” for clarity).

**Batch Setup:**

- Same Resume/Discard when interrupted (parity with current dual ACK).
- “No Interrupt” readiness row clears only after Resume or Discard.

**Header:** keep “Interrupted Batch Pending” until resolved.

**Web:** remain read-only; expose `interrupted` (and optionally interrupted state string) — no resume POST in this change.

### 3.5 Status exposure

Extend `system_status_t` (or fill existing unused paths) so HMI can show:

- `batch_interrupted`
- `batch_id` (interrupted id when idle)
- `interrupted_state` (uint8) and/or human phase already available via helper

---

## 4. Docs touch-ups (same change set)

- MASTERPLAN known gaps: remove “RTC not on HMI” and “does not auto-resume”; replace with “confirmed Resume/Discard after power loss”.
- Phase 9 checklist: add Set Clock smoke test; change interrupt step to Resume vs Discard.
- `05-software-design` recovery section: document confirm-resume policy.

---

## 5. Testing

| Test | Expect |
|---|---|
| Set Clock Save in IDLE | Header/System Info show new time after refresh |
| Set Clock mid-DRYING | Save disabled |
| Power-cycle mid-DRYING | Boot IDLE safe; Resume restores DRYING + elapsed; heater may run after sensors OK |
| Discard after interrupt | START allowed; new batch id on next START |
| Legacy `0xB6` blob | Interrupted UI appears; Resume with zero elapsed OK |
| Resume with chamber unplugged | Blocked; still interrupted |
| SoftAP | Still GET-only interrupted flag |

---

## 6. Implementation order

1. Versioned recovery blob + batch_manager interrupted elapsed/step fields  
2. `process_controller_resume_interrupted` + save path includes timers  
3. Dashboard / Batch Setup Resume+Discard UI + status fields  
4. Set Clock screen + System Info entry  
5. Docs / Phase 9 checklist updates  

---

## 7. Open points (none blocking)

None — policy choices locked: confirm-then-resume; DRYING resumes with heater after recheck; full Y/M/D/H/M editor.
