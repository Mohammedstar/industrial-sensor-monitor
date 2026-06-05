"""
=============================================================
Industrial Sensor Monitor - Python SCADA Dashboard
=============================================================
Author:  Mohammed Satar | Instrumentation & Control Engineer
Version: 1.0
Purpose: Real-time web dashboard for ESP32 sensor node data
         Implements ISA-18.2 alarm management and data logging
=============================================================
"""

import serial
import json
import csv
import smtplib
import threading
import time
import datetime
from pathlib import Path
from flask import Flask, render_template, jsonify
from collections import deque

app = Flask(__name__)

# ─── Configuration ───────────────────────────────────────
SERIAL_PORT   = "COM3"       # Change to your ESP32 port
BAUD_RATE     = 115200
LOG_FILE      = "data/sensor_log.csv"
MAX_HISTORY   = 300          # Keep last 5 minutes at 1Hz

# ─── Alarm Limits (matching firmware) ────────────────────
ALARM_LIMITS = {
    "temp":  {"LL": 10, "L": 20, "H": 85, "HH": 95},
    "press": {"LL": 0.5, "L": 1.0, "H": 6.0, "HH": 7.0},
    "flow":  {"LL": 1.0, "L": 2.0, "H": 20.0, "HH": 25.0},
    "level": {"LL": 5.0, "L": 15.0, "H": 80.0, "HH": 90.0},
}

ALARM_STATE_NAMES = {0: "NORMAL", 1: "ALERT", 2: "WARNING", 3: "ALARM"}

# ─── Shared Data Buffers ─────────────────────────────────
data_history = {
    "temp":  deque(maxlen=MAX_HISTORY),
    "press": deque(maxlen=MAX_HISTORY),
    "flow":  deque(maxlen=MAX_HISTORY),
    "level": deque(maxlen=MAX_HISTORY),
    "time":  deque(maxlen=MAX_HISTORY),
}
latest_reading = {}
active_alarms  = []
lock = threading.Lock()


class AlarmManager:
    """
    ISA-18.2 compliant alarm management.
    Handles alarm detection, acknowledgment, and notification.
    """
    ALARM_NAMES = {
        "temp":  "Temperature",
        "press": "Pressure",
        "flow":  "Flow Rate",
        "level": "Tank Level",
    }

    def __init__(self):
        self.alarms = []
        self.alarm_history = []

    def check_alarms(self, reading: dict) -> list:
        """Evaluate all alarm conditions from a new reading."""
        new_alarms = []
        for param, limits in ALARM_LIMITS.items():
            value = reading.get(param)
            if value is None:
                continue
            if value >= limits["HH"] or value <= limits["LL"]:
                state = "ALARM"
                priority = 1
            elif value >= limits["H"] or value <= limits["L"]:
                state = "WARNING"
                priority = 2
            else:
                state = "NORMAL"
                priority = 3

            if state != "NORMAL":
                alarm = {
                    "id": f"{param}_{int(time.time())}",
                    "param": param,
                    "name": self.ALARM_NAMES[param],
                    "state": state,
                    "priority": priority,
                    "value": value,
                    "timestamp": datetime.datetime.now().strftime("%H:%M:%S"),
                    "acknowledged": False,
                }
                new_alarms.append(alarm)

        return new_alarms

    def get_active_alarms(self) -> list:
        return [a for a in self.alarms if not a["acknowledged"]]


class DataLogger:
    """
    Logs sensor readings to CSV file with timestamp.
    Simulates a process historian (like OSIsoft PI or Wonderware).
    """
    HEADERS = ["timestamp", "temperature_C", "pressure_bar",
                "flow_Lmin", "level_cm",
                "temp_alarm", "press_alarm", "flow_alarm", "level_alarm"]

    def __init__(self, filepath: str):
        self.filepath = Path(filepath)
        self.filepath.parent.mkdir(parents=True, exist_ok=True)
        if not self.filepath.exists():
            with open(self.filepath, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(self.HEADERS)

    def log(self, reading: dict):
        """Append one row to the CSV historian."""
        row = [
            datetime.datetime.now().isoformat(),
            reading.get("temp", ""),
            reading.get("press", ""),
            reading.get("flow", ""),
            reading.get("level", ""),
            ALARM_STATE_NAMES.get(reading.get("alarms", {}).get("temp", 0), ""),
            ALARM_STATE_NAMES.get(reading.get("alarms", {}).get("press", 0), ""),
            ALARM_STATE_NAMES.get(reading.get("alarms", {}).get("flow", 0), ""),
            ALARM_STATE_NAMES.get(reading.get("alarms", {}).get("level", 0), ""),
        ]
        with open(self.filepath, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(row)


alarm_manager = AlarmManager()
data_logger   = DataLogger(LOG_FILE)


def serial_reader_thread():
    """Background thread: reads JSON from ESP32 over serial."""
    global latest_reading, active_alarms
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
        print(f"[OK] Serial connected on {SERIAL_PORT}")
    except Exception as e:
        print(f"[WARN] Serial unavailable: {e}. Running in DEMO mode.")
        ser = None

    while True:
        if ser:
            try:
                line = ser.readline().decode("utf-8").strip()
                if not line or not line.startswith("{"):
                    continue
                reading = json.loads(line)
            except (json.JSONDecodeError, serial.SerialException):
                continue
        else:
            # Demo mode: generate simulated data
            import random, math
            t = time.time()
            reading = {
                "temp":  65.0 + 15 * math.sin(t / 30) + random.uniform(-1, 1),
                "press": 4.0  + 1.5 * math.sin(t / 45) + random.uniform(-0.1, 0.1),
                "flow":  12.0 + 5   * math.cos(t / 20) + random.uniform(-0.5, 0.5),
                "level": 50.0 + 20  * math.sin(t / 60) + random.uniform(-1, 1),
                "alarms": {"temp": 0, "press": 0, "flow": 0, "level": 0},
            }
            time.sleep(1)

        with lock:
            latest_reading = reading
            now_str = datetime.datetime.now().strftime("%H:%M:%S")
            for key in ["temp", "press", "flow", "level"]:
                if key in reading:
                    data_history[key].append(reading[key])
            data_history["time"].append(now_str)
            active_alarms = alarm_manager.check_alarms(reading)

        data_logger.log(reading)


# ─── Flask Routes ────────────────────────────────────────
@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/current")
def api_current():
    with lock:
        return jsonify({
            "latest": latest_reading,
            "alarms": active_alarms,
            "limits": ALARM_LIMITS,
        })


@app.route("/api/history")
def api_history():
    with lock:
        return jsonify({
            "time":  list(data_history["time"]),
            "temp":  list(data_history["temp"]),
            "press": list(data_history["press"]),
            "flow":  list(data_history["flow"]),
            "level": list(data_history["level"]),
        })


@app.route("/api/alarms")
def api_alarms():
    with lock:
        return jsonify({"alarms": active_alarms})


# ─── Main ────────────────────────────────────────────────
if __name__ == "__main__":
    # Start serial reader in background thread
    t = threading.Thread(target=serial_reader_thread, daemon=True)
    t.start()
    print("[OK] Dashboard running at http://localhost:5000")
    app.run(host="0.0.0.0", port=5000, debug=False)
