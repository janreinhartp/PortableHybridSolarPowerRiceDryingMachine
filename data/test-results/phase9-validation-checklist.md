# Phase 9 — Validation Checklist

Record results here as each step is completed on hardware. Keep heater power disconnected until Phase 5 safety tests pass.

Firmware baseline: Phase 8 (local Wi-Fi SoftAP + read-only web + product HMI).

| Field | Value |
|---|---|
| Date | |
| Operator | |
| Firmware build / commit | |
| SoftAP SSID | `RiceDryer-____` |
| Reference moisture meter | |

---

## 1. Dry run (forced / simulated sensors)

Precondition: RS485 sensors, relay board, RTC, SD, HMI, and web dashboard reachable.

```text
[ ] Boot reaches IDLE without critical fault
[ ] System Info shows Display / Touch / LVGL / SD / RTC / RS485 / Relay / Wi-Fi Ok
[ ] Phone or laptop joins SoftAP; http://192.168.4.1 loads
[ ] GET /api/status and /api/sensors update live
[ ] Manual Control: fan, elevator, door open/close (heater OFF)
[ ] System → Set Clock: change time, Save; header clock updates
[ ] START → PRECHECK → DRYING with heater still disconnected or mock-safe
[ ] Abort returns to safe IDLE (actuators OFF)
[ ] Fault inject: disconnect chamber or hot-air probe → matching F00x + Alarms screen
[ ] ACK clears sticky fault only when condition is gone
[ ] Power-cycle mid-run: boot IDLE safe; Home shows Resume + Discard
[ ] Discard clears interrupt; new START allowed
[ ] (Separate run) Resume restores same batch/phase after sensors OK
[ ] CSV appears under /sdcard/batches after a short run (or export link works)
```

Notes:

```text


```

---

## 2. Empty chamber heat test

Precondition: Phase 5 safety tests passed; thermal protection and E-stop verified electrically.

```text
[ ] Empty chamber loaded (no rice)
[ ] Target moisture set high enough that cooling is reached by temperature path, or run short max-time
[ ] Heater hysteresis tracks chamber temperature
[ ] Over-temp (hot-air) trip forces heater OFF, fan ON (F001)
[ ] Cooling completes when chamber ≤ cooling target
[ ] Discharge: door open → elevator for discharge time → door close → COMPLETE
[ ] Batch summary logged; history lists the batch
[ ] Power-loss mid-run: restart shows interrupted batch; heater stays OFF until Resume or Discard
```

Notes:

```text


```

---

## 3. Small rice batch

```text
[ ] Load small rice mass (well under 10 kg)
[ ] Moisture sensor calibrated vs reference meter (record offset/scale in NVS)
[ ] Full automatic batch completes to target moisture
[ ] Final moisture vs reference within agreed tolerance
[ ] Logs clean (no unexpected F00x); CSV export reviewed
```

| Metric | Firmware | Reference meter |
|---|---:|---:|
| Initial moisture % | | |
| Final moisture % | | |
| Max chamber °C | | — |
| Batch ID | | — |

Notes:

```text


```

---

## 4. 10 kg validation batch

```text
[ ] Load 10 kg rice
[ ] Full automatic batch completes
[ ] Target moisture met (record below)
[ ] Safety fault injection still passes after the run
[ ] Power-loss recovery stays safe
[ ] SD logs and web CSV export complete
```

| Metric | Value |
|---|---|
| Batch ID | |
| Initial moisture % | |
| Final moisture % | |
| Target moisture % | |
| Max / avg chamber °C | |
| Drying / cooling / discharge time | |
| Faults | |

Notes:

```text


```

---

## Production readiness (MASTERPLAN)

```text
[ ] Full automatic batch completes
[ ] Moisture sensor calibrated against a reference meter
[ ] Safety fault injection passes
[ ] Power-loss recovery stays safe
[ ] 10 kg batch meets target moisture and logs cleanly
```

Sign-off:

```text
Operator: _______________    Date: _______________
Reviewer: _______________    Date: _______________
```
