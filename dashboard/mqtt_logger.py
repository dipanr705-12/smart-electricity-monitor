#!/usr/bin/env python3
"""
mqtt_logger.py
──────────────
Subscribes to home/energy/# and logs data to a CSV file.
Also prints live readings and fires desktop alerts when thresholds are breached.

Requirements:
    pip install paho-mqtt plyer
"""

import csv
import json
import os
import signal
import sys
from datetime import datetime
from pathlib import Path

import paho.mqtt.client as mqtt

try:
    from plyer import notification
    NOTIFY = True
except ImportError:
    NOTIFY = False
    print("[warn] 'plyer' not installed – desktop notifications disabled.")

# ── Config ──────────────────────────────────────────
BROKER_HOST  = "localhost"
BROKER_PORT  = 1883
TOPIC_DATA   = "home/energy/data"
TOPIC_ALERT  = "home/energy/alert"
LOG_DIR      = Path(__file__).parent / "logs"
LOG_FILE     = LOG_DIR / f"energy_{datetime.now().strftime('%Y%m%d')}.csv"
# ────────────────────────────────────────────────────

FIELDNAMES = [
    "timestamp", "voltage", "current", "power",
    "energy_kwh", "frequency", "power_factor", "est_cost_inr"
]

LOG_DIR.mkdir(exist_ok=True)
csv_file = open(LOG_FILE, "a", newline="")
writer   = csv.DictWriter(csv_file, fieldnames=FIELDNAMES)
if os.stat(LOG_FILE).st_size == 0:
    writer.writeheader()
print(f"[log] Writing to {LOG_FILE}")


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[mqtt] Connected to {BROKER_HOST}:{BROKER_PORT}")
        client.subscribe([(TOPIC_DATA, 0), (TOPIC_ALERT, 0)])
    else:
        print(f"[mqtt] Connection failed: rc={rc}")


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8", errors="replace")
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        print(f"[?] Bad JSON on {msg.topic}: {payload}")
        return

    if msg.topic == TOPIC_DATA:
        row = {
            "timestamp"     : datetime.now().isoformat(timespec="seconds"),
            "voltage"       : data.get("voltage",      ""),
            "current"       : data.get("current",      ""),
            "power"         : data.get("power",        ""),
            "energy_kwh"    : data.get("energy_kwh",   ""),
            "frequency"     : data.get("frequency",    ""),
            "power_factor"  : data.get("power_factor", ""),
            "est_cost_inr"  : data.get("est_cost_inr", ""),
        }
        writer.writerow(row)
        csv_file.flush()

        print(
            f"[data] {row['timestamp']}  "
            f"V={row['voltage']}V  I={row['current']}A  "
            f"P={row['power']}W  E={row['energy_kwh']}kWh  "
            f"PF={row['power_factor']}  ₹{row['est_cost_inr']}"
        )

    elif msg.topic == TOPIC_ALERT:
        alert_type = data.get("type", "alert")
        alert_msg  = data.get("msg",  payload)
        print(f"\n⚠️  ALERT [{alert_type}]: {alert_msg}\n")
        if NOTIFY:
            notification.notify(
                title   = f"⚡ Energy Alert: {alert_type.upper()}",
                message = alert_msg,
                timeout = 8,
            )


def on_disconnect(client, userdata, rc):
    print(f"[mqtt] Disconnected (rc={rc}). Reconnecting…")


client = mqtt.Client(client_id="energy-logger-py")
client.on_connect    = on_connect
client.on_message    = on_message
client.on_disconnect = on_disconnect

def shutdown(sig, frame):
    print("\n[info] Shutting down logger…")
    csv_file.close()
    client.disconnect()
    sys.exit(0)

signal.signal(signal.SIGINT,  shutdown)
signal.signal(signal.SIGTERM, shutdown)

client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
client.loop_forever()
