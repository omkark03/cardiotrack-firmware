"""
Fake backend for the chest hub: accepts exactly what the firmware sends, so you can test
Wi-Fi upload, retries and the flash queue before the real backend exists.

Run:   pip install -r requirements.txt
       uvicorn server:app --host 0.0.0.0 --port 8000
Then put  http://<laptop-ip>:8000/ingest  in include/secrets.h (BACKEND_URL).

Request: multipart/form-data
    meta  (form field)  JSON, see core/payload.cpp
    ecg   (file)        int16 little-endian, ecg.sample_rate_hz Hz (raw ADC counts 0..4095)
    imu   (file)        int16 little-endian, imu.sample_rate_hz Hz (|accel| in milli-g)
Header:  X-Device-Key
Answers: 200 stored (also when it is a duplicate, so retries are safe),
         400/422 the record itself is malformed (firmware will not retry),
         401 wrong key (firmware keeps the data and retries).
"""
import json
import os
import struct
import time
from pathlib import Path

from fastapi import FastAPI, File, Form, Header, HTTPException, UploadFile

DEVICE_KEY = os.environ.get("DEVICE_KEY", "dev-key-change-me")
DATA_DIR = Path(os.environ.get("DATA_DIR", Path(__file__).parent / "received"))
DATA_DIR.mkdir(parents=True, exist_ok=True)

app = FastAPI(title="CardioTrack fake backend")


def _int16s(raw: bytes):
    if len(raw) % 2:
        raise ValueError("odd byte count")
    return struct.unpack("<%dh" % (len(raw) // 2), raw)


@app.get("/health")
def health():
    return {"status": "ok", "stored": len(list(DATA_DIR.glob("*.json")))}


@app.post("/ingest")
async def ingest(
    meta: str = Form(...),
    ecg: UploadFile = File(...),
    imu: UploadFile = File(...),
    x_device_key: str = Header(None),
):
    if x_device_key != DEVICE_KEY:
        raise HTTPException(status_code=401, detail="bad device key")

    try:
        m = json.loads(meta)
        n_ecg_expected = int(m["ecg"]["samples"])
        n_imu_expected = int(m["imu"]["samples"])
        key = "%s_%05d_%06d" % (m["device_id"], int(m["boot_count"]), int(m["seq"]))
    except (ValueError, KeyError, TypeError) as e:
        raise HTTPException(status_code=400, detail="bad meta: %s" % e)

    ecg_raw, imu_raw = await ecg.read(), await imu.read()
    try:
        ecg_vals, imu_vals = _int16s(ecg_raw), _int16s(imu_raw)
    except (ValueError, struct.error) as e:
        raise HTTPException(status_code=422, detail="bad waveform: %s" % e)
    if len(ecg_vals) != n_ecg_expected or len(imu_vals) != n_imu_expected:
        raise HTTPException(status_code=422, detail="sample counts do not match meta")

    duplicate = (DATA_DIR / (key + ".json")).exists()
    (DATA_DIR / (key + ".ecg.bin")).write_bytes(ecg_raw)
    (DATA_DIR / (key + ".imu.bin")).write_bytes(imu_raw)
    record = {"received_at": time.time(), "meta": m}
    (DATA_DIR / (key + ".json")).write_text(json.dumps(record))

    print("[ingest] %s  hr=%s(%s)  temp=%s  activity=%s  ecg_ok=%s  synced=%s%s" % (
        key, m["hr"]["bpm"], m["hr"]["signal_quality"], m["skin_temp_c"], m["imu"]["activity_level"],
        m["ecg"]["ok"], m["time_synced"], "  (duplicate)" if duplicate else ""))
    return {"status": "ok", "duplicate": duplicate, "ecg_samples": len(ecg_vals),
            "imu_samples": len(imu_vals), "server_epoch": int(time.time())}


@app.get("/records")
def records(limit: int = 20):
    files = sorted(DATA_DIR.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True)[:limit]
    return [json.loads(p.read_text())["meta"] for p in files]
