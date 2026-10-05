"""Plot the most recently received burst: ECG waveform + accelerometer magnitude.
   python view_latest.py            (needs matplotlib)
"""
import json
import struct
import sys
from pathlib import Path

import matplotlib.pyplot as plt

data = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "received"
files = sorted(data.glob("*.json"), key=lambda p: p.stat().st_mtime)
if not files:
    sys.exit("nothing received yet in %s" % data)
jf = files[-1]
key = jf.name[: -len(".json")]
meta = json.loads(jf.read_text())["meta"]


def load(name):
    raw = (data / (key + name)).read_bytes()
    return struct.unpack("<%dh" % (len(raw) // 2), raw)


ecg, imu = load(".ecg.bin"), load(".imu.bin")
fig, ax = plt.subplots(2, 1, figsize=(11, 6))
t = [i / meta["ecg"]["sample_rate_hz"] for i in range(len(ecg))]
ax[0].plot(t, ecg, lw=0.6)
ax[0].set_title("%s  ECG (raw ADC)  HR=%s bpm (%s)  temp=%s C" % (
    key, meta["hr"]["bpm"], meta["hr"]["signal_quality"], meta["skin_temp_c"]))
ax[0].set_xlabel("s")
t2 = [i / meta["imu"]["sample_rate_hz"] for i in range(len(imu))]
ax[1].plot(t2, imu, lw=0.8)
ax[1].set_title("|accel| (mg)  activity=%s" % meta["imu"]["activity_level"])
ax[1].set_xlabel("s")
plt.tight_layout()
plt.show()
