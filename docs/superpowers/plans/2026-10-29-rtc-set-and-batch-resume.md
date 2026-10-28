# RTC Set Clock + Confirmed Batch Resume Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add System→Set Clock HMI and confirmed Resume/Discard after power-loss using a versioned NVS recovery blob.

**Architecture:** Extend `batch_manager` recovery to magic `0xB7` with elapsed timers + discharge step; add `process_controller_resume_interrupted()` that restores phase after a sensor/safety gate while boot stays IDLE/safe; Dashboard/Batch Setup gain Resume+Discard; new Set Clock screen uses `rtc_manager_set`.

**Tech Stack:** ESP-IDF, NVS, LVGL 8, existing `rtc_manager` / `batch_manager` / `process_controller` / HMI patterns.

## Global Constraints

- Boot never silently restarts heaters or phase actuators.
- Resume only after operator taps Resume; Discard = today’s ACK.
- DRYING resume may re-enable heater after sensor/safety recheck.
- Set Clock gated by `process_controller_manual_control_allowed()`.
- Web stays GET-only (no resume POST).

---

### Task 1: Versioned recovery blob

**Files:**
- Modify: `firmware/main/app/batch_manager/batch_manager.h`
- Modify: `firmware/main/app/batch_manager/batch_manager.cpp`

- [ ] Extend `batch_status_t` interrupted fields with elapsed + discharge_step
- [ ] Replace blob with magic `0xB7` version 1; load legacy `0xB6` as best-effort zeros
- [ ] Extend `batch_manager_save_recovery(...)` to take elapsed + discharge_step
- [ ] Add `batch_manager_resume_from_interrupted(...)` to rehydrate active batch without new seq
- [ ] Commit

### Task 2: process_controller resume + save path

**Files:**
- Modify: `firmware/main/app/process_controller/process_controller.h`
- Modify: `firmware/main/app/process_controller/process_controller.cpp`
- Modify: `firmware/main/system_status.h` (`interrupted_state`)

- [ ] Extend `save_recovery_throttled` to pass timers + discharge step
- [ ] Add `process_controller_resume_interrupted()` with safety gate
- [ ] Fill `status->interrupted_state` in `fill_status`
- [ ] Commit

### Task 3: Dashboard / Batch Setup Resume+Discard

**Files:**
- Modify: `firmware/main/ui/screens/dashboard/dashboard_screen.cpp`
- Modify: `firmware/main/ui/screens/batch_setup/batch_setup_screen.cpp`

- [ ] Replace single ACK with Resume (teal) + Discard; show batch id + phase
- [ ] Commit

### Task 4: Set Clock screen

**Files:**
- Create: `firmware/main/ui/screens/set_clock/set_clock_screen.h|.cpp`
- Modify: `ui_manager`, `system_info_screen`, `CMakeLists.txt`

- [ ] Full Y/M/D/H/M editor + Save via `rtc_manager_set`
- [ ] Commit

### Task 5: Docs

**Files:**
- Modify: `docs/MASTERPLAN.md`, `docs/05-software-design.md`, `data/test-results/phase9-validation-checklist.md`
- Mark design spec Status: Approved/Implemented

- [ ] Update gaps + checklist + recovery policy
- [ ] Commit
