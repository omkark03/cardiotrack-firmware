# CardioTrack firmware

Firmware for **CardioTrack (SB08)**: a chest-worn wearable hub for recovery-trajectory-aware monitoring after cardiac surgery.
Final-year B.Tech (ENTC) project, PCCOE Pune. Team: Omkar Kumbhar, Ketki Gaikwad, Bhumit Hambire. Guide: Dr. A. S. Gaadhe.

The device wakes every 5 minutes, records a 60 s burst (ECG, motion, heart rate, skin temperature, battery), uploads it over Wi-Fi to a backend, and goes back to deep sleep. A queue in flash keeps bursts safe during Wi-Fi/server outages.

> **Status (Oct 2026): firmware complete and verified end-to-end on a real ESP32 with *mock* sensors. Real sensor drivers are written but not yet compiled/tested on hardware.**

---

## Current state

| Area | State |
|---|---|
| Burst pipeline (ECG 250 Hz, IMU 25 Hz, HR, temp, battery) | Done, runs with mock sensors on the ESP32 |
| Wi-Fi + HTTP multipart upload with device key | Done, verified against the fake backend (HTTP 200) |
| Offline queue in flash (LittleFS, 40 records, oldest-first drain, drop-oldest when full) | Done, verified on hardware (outage -> queued -> flushed) |
| Deep sleep (timer) + motion wake (MPU6050 INT on GPIO27) | Done, verified on hardware: 300 s wake-to-wake spacing, motion wake does not shift the schedule |
| Time: NTP at cold boot (2 tries), resync every 12 cycles, late-sync timestamp correction | Done, simulator-tested; the last change is not yet flashed |
| Real drivers: AD8232 ECG, MAX30102 (IR-only HR), MPU6050, NTC thermistor, battery ADC | Written, **not yet compiled on ESP32, not tested on hardware** |
| Fake backend (FastAPI) + plot viewer | Done |
| Host tests + hardware-free simulator (virtual time, stubbed Wi-Fi/flash/sleep) | Done, all passing |
| Real backend, AI/trajectory models, dashboard | Not in this repo yet |
| PCB port, battery/charging, enclosure | Not started |

### Not verified yet
- Real sensor drivers on hardware (compile risk: `Wire.requestFrom` overload; MPU6050 wake-on-motion; MAX30102 on the chest; thermistor calibration; battery divider)
- System clock surviving deep sleep (assumed; the schedule logic tolerates a reset by re-syncing)
- Current draw / coin cell + supercap behaviour during Wi-Fi TX

---

## How a cycle works

```
wake -> motion wake before due?  yes: set motion flag, sleep again (motion disarmed)
     -> 60 s burst, radio OFF  (ECG 250 Hz, IMU 25 Hz, MAX30102 IR, thermistor, battery)
     -> Wi-Fi on -> NTP if needed -> upload fresh burst -> drain up to 4 queued bursts -> Wi-Fi off
     -> upload failed? save burst to flash queue
     -> deep sleep until next due (timer) or motion (ext0, GPIO27)
```

Cycle is 300 s wake-to-wake (x3 if the battery is below 2500 mV). Cold boot syncs time before the first burst. If both NTP tries fail, the first burst is captured unsynced and its timestamp is corrected after a later sync (`time_estimated: true`).

## Upload format

`POST /ingest`, `multipart/form-data`, header `X-Device-Key`:

| Part | Content |
|---|---|
| `meta` | JSON: device id, fw, `seq`, `boot_count`, `timestamp`, `epoch`, `time_synced`, ECG/IMU/HR/temp/battery summaries and quality flags, `motion_since_last` |
| `ecg` | int16 little-endian, 250 Hz, 15000 samples |
| `imu` | int16 little-endian, acceleration magnitude in mg, 25 Hz, 1500 samples |

Server result policy: 2xx = accepted; 400/413/422 = rejected for good (dropped); anything else = retry later (queued). The backend is idempotent per `(device, boot_count, seq)`. `recovery_day` is not sent by the device; the backend derives it from the surgery date.

---

## Quick start

Requirements: VS Code + PlatformIO, an ESP32 dev board (esp32dev), Python 3.10+.

1. **Secrets:** copy `include/secrets.example.h` to `include/secrets.h` and fill in Wi-Fi name/password, `BACKEND_URL` (your laptop's IP) and `DEVICE_KEY`. `secrets.h` is git-ignored.
2. **Backend (laptop, same Wi-Fi):**
   ```
   cd backend_fake
   pip install -r requirements.txt
   set DEVICE_KEY=dev-key-change-me        (use the same key as secrets.h)
   uvicorn server:app --host 0.0.0.0 --port 8000
   ```
   Check `http://localhost:8000/records`. Bursts are stored in `backend_fake/received/`. `python view_latest.py` plots the latest one.
3. **Firmware:** pick an environment and upload (`pio run -e <env> -t upload`), then open the serial monitor at 115200.

### Environments (`platformio.ini`)

| Env | Sensors | Sleep | Timing | Use |
|---|---|---|---|---|
| `mock_bench` (default) | mock | fake (delay) | 10 s burst / 30 s cycle | fast pipeline checks |
| `mock_full` | mock | real deep sleep | 60 s / 300 s | real behaviour test |
| `bb_real_bench` | real drivers | fake | 10 s / 30 s | breadboard bring-up (build-only until sensors are wired) |
| `pcb_real` | real drivers | real | 60 s / 300 s | target PCB (`BOARD_PCB_V1`, battery divider on) |
| `stream_mock` / `stream_real` | mock / real ECG | none | raw ECG at 250 Hz | Serial Plotter ECG check |

Per-sensor mock flags (`USE_MOCK_ECG/PPG/IMU/TEMP`) allow mixing real and mock sensors during bring-up.

## Tests (no hardware needed)

```
cd host_test
make test        # core algorithms (HR, quality, payload, time patch)
make sim         # runs the real app code against stubbed hardware: bench + sleep + no-sensor scenarios
```
Scenarios cover Wi-Fi outage, server 503/400, lost NTP, motion wake, coin-cell swap (power loss), and missing sensors.

---

## Hardware (breadboard prototype)

ESP32 + AD8232 (ECG) + MAX30102 (IR-only heart rate) + MPU6050 (motion, wake-on-motion) + NTC thermistor, in a single chest hub.

| Signal | Pin | Status |
|---|---|---|
| ECG output | GPIO34 | confirmed |
| Thermistor ADC | GPIO35 | proposed |
| Battery ADC | GPIO36 | reserved (only with `-DBATTERY_PRESENT`) |
| I2C SDA / SCL | GPIO21 / GPIO22 | proposed |
| MPU6050 INT | GPIO27 | proposed (RTC-capable pin, ext0 wake) |

Assumptions to confirm: 10k NTC, B = 3950, wired 3V3 -> 10k -> node -> NTC -> GND (calibrate against a reference thermometer).

## Repository layout

```
platformio.ini        environments
include/              config.h (all tunables), board_pins.h, secrets.example.h
src/main.cpp          entry point
src/app/              device state machine (cycle, schedule, time sync)
src/services/         burst capture, queue (LittleFS), net (Wi-Fi/NTP/upload), sleep manager
src/core/             portable algorithms: hr_algo, quality, payload (host-testable)
src/hal/              sensor interfaces + mock/ and real/ drivers + factory
backend_fake/         FastAPI stand-in for the real backend, plot viewer
host_test/            unit tests + hardware-free simulator + stubs
FIRMWARE_NOTES.md     design decisions and detailed notes
```

## Roadmap

1. Compile `bb_real_bench`, fix any driver build errors
2. Bring up real sensors one at a time: thermistor -> MPU6050 (incl. wake-on-motion) -> MAX30102 -> AD8232
3. Combined breadboard run, calibrate thresholds, measure current
4. Port to PCB (`pcb_real`), battery + charging
5. Backend and AI side: recovery-trajectory model, escalation logic, ECG/IMU correlation agent, clinician dashboard
6. Report, demo, review

Open design questions: 2 vs 3 ECG electrodes, charging method, thermistor part/B-value, final pin assignment.
