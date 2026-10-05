# CardioTrack chest hub firmware — notes

## What the firmware does (every 5 minutes)
wake → (motion wake? note it, go back to sleep) → 60 s burst, radio off (ECG 250 Hz, IMU 25 Hz, MAX30102 IR heart rate, thermistor, battery) → Wi-Fi on → upload → drain up to 4 queued bursts → Wi-Fi off → (upload failed? save burst to flash) → deep sleep until the next burst (timer) or motion (MPU6050 INT).

## Run it
1. VS Code + PlatformIO extension, open this folder.
2. Edit `include/secrets.h` (Wi-Fi, laptop IP, key).
3. Laptop: `cd backend_fake && pip install -r requirements.txt && uvicorn server:app --host 0.0.0.0 --port 8000`
4. Pick an environment (status bar) and upload:

| env | what | when |
|---|---|---|
| `mock_bench` (default) | all sensors fake, 10 s burst / 30 s cycle, no deep sleep | first test, no hardware |
| `mock_full` | all fake, real 60 s / 300 s, real deep sleep. Jumper GPIO27 to 3V3 = fake motion wake | test sleep/wake |
| `stream_real` / `stream_mock` | raw ECG to Serial Plotter | debugging the AD8232 |
| `bb_real_bench` | real drivers on the breadboard. Add `-DUSE_MOCK_PPG`, `-DUSE_MOCK_IMU`, `-DUSE_MOCK_TEMP`, `-DUSE_MOCK_ECG` for any sensor not wired yet | hardware bring-up |
| `pcb_real` | final PCB, real battery divider | last |

`python backend_fake/view_latest.py` plots the last received ECG + motion.

## Tested on the laptop (`cd host_test && make test && make sim`)
- HR algorithm on synthetic PPG (55/72/110 bpm exact, no-skin / noise / too-short handled)
- Payload + multipart builder; the same bytes were POSTed to the real FastAPI server
- The real app/queue/uploader/sleep code against stubbed hardware: Wi-Fi outage, 503s, 400, NTP outage, motion wake, power loss, drift-free schedule, sensors missing

## NOT tested (needs your hardware / a PlatformIO build)
- Compilation against the real ESP32 Arduino core (the sandbox could not download it). Possible small fixes: `Wire.requestFrom` overload in `hal/real/i2c_bus.h`, `esp_sntp.h` names in `services/net.cpp`.
- MAX30102, MPU6050 (incl. wake-on-motion), thermistor, battery drivers: written from datasheets, never run on a chip.
- Whether HR is readable from the chest at the current LED current (`PPG_LED_PA`).
- That the system clock really survives deep sleep on your core version; real current draw; Wi-Fi TX with the supercap.

## Decisions taken (change if you disagree)
- Motion wake only sets a flag `motion_since_last` (one per interval, then motion is disarmed). No extra burst.
- ECG goes as binary int16, not JSON. Upload = multipart (`meta` JSON + `ecg` + `imu`).
- Wi-Fi credentials hardcoded in `secrets.h`. Plain HTTP + `X-Device-Key` header.
- Cycle 60 s per 300 s, wake-to-wake. Cycle ×3 when battery < 2500 mV.
- Queue: max 40 bursts (~33 KB each) in LittleFS, oldest dropped when full. No OTA partition (2 MB for the queue).
- `recovery_day` is NOT sent by the device; the backend derives it from the surgery date + timestamp.
- Battery on breadboard is reported as -1 (unknown). Real divider only with `-DBATTERY_PRESENT`.

## Schema change vs the earlier draft
Added `imu` waveform (|accel| in mg, 25 Hz, 1500 samples per burst, sent as a second binary part). The locked FFT correlation agent needs a motion time series, not just one mean value. Metadata also gained: `seq`, `boot_count`, `epoch`, `time_synced`, quality flags (`ecg.ok`, `hr.signal_quality`, `temp_ok`, `imu.ok`), `motion_since_last`.

## Where things are
```
src/app/app.cpp          the whole cycle / state machine
src/services/            burst capture, flash queue, Wi-Fi+NTP+upload, sleep
src/core/                portable logic: HR algorithm, quality checks, payload (laptop-testable)
src/hal/                 sensor interfaces; mock/ and real/ drivers; factory.cpp picks one
include/                 config.h (all tunables), board_pins.h, secrets.h
backend_fake/            FastAPI server + plot script
host_test/               laptop unit tests + hardware-free simulator
```
