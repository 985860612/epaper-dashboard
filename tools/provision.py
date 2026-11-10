# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5,<4"]
# ///
"""Import existing QWeather credentials and private Wi-Fi settings via physical USB.

Never prints secret values or compiles them into firmware. Uses only the new
epaperdash NVS namespace; preserves all other namespaces and partitions.
"""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import time

import serial


def private_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    os.fchmod(fd, 0o600)
    with os.fdopen(fd, "w") as out:
        json.dump(data, out, ensure_ascii=False, indent=2)
        out.write("\n")


def read_env(path):
    result = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^([A-Z_]+)\s*=\s*(.*?)\s*$", line)
        if match:
            result[match[1]] = match[2].strip("\"'")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--source", type=Path, required=True, help="Existing QWeather .env")
    parser.add_argument("--wifi", type=Path, required=True, help="Private JSON with ssid and wifiPassword")
    parser.add_argument("--access", type=Path, default=Path(__file__).resolve().parents[1] / "local/device-access.json")
    args = parser.parse_args()
    env = read_env(args.source)
    wifi = json.loads(args.wifi.read_text())
    access = json.loads(args.access.read_text()) if args.access.exists() else {}
    password = access.get("password") or secrets.token_hex(12)
    private_json(args.access, {"username": "admin", "password": password, "ip": access.get("ip", "")})
    config = {
        "ssid": wifi["ssid"], "wifiPassword": wifi["wifiPassword"],
        "apiHost": env["QWEATHER_API_HOST"].removeprefix("https://").rstrip("/"),
        "apiKey": env["QWEATHER_API_KEY"], "adminPassword": password,
        "location": "101010400", "city": "北京顺义", "stocks": ["sh600900"],
    }
    with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=8) as conn:
        conn.reset_input_buffer()
        # Wait until the application loop is servicing USB before sending credentials.
        startup_deadline = time.monotonic()+40
        next_ping = 0
        started = False
        while time.monotonic()<startup_deadline:
            if time.monotonic()>=next_ping:
                conn.write(b'{"command":"status"}\n')
                next_ping=time.monotonic()+1
            line=conn.readline()
            try:
                if json.loads(line).get("event")=="status":
                    started=True
                    break
            except (ValueError,UnicodeError):
                pass
        if not started:
            raise SystemExit("Firmware did not acknowledge USB status; no credentials sent.")
        conn.write((json.dumps({"command": "provision", "config": config}, ensure_ascii=False)+"\n").encode())
        deadline = time.monotonic()+110
        provisioned = False
        next_status = 0
        buffer = b""
        while time.monotonic() < deadline:
            if time.monotonic() >= next_status:
                conn.write(b'{"command":"status"}\n')
                next_status = time.monotonic()+5
            buffer += conn.read(conn.in_waiting or 1)
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                try:
                    data = json.loads(line.decode().strip())
                except (ValueError, UnicodeError):
                    continue
                if data.get("event") == "provisioned":
                    provisioned = True
                    print("Credentials saved to device NVS; values omitted.", flush=True)
                if data.get("event") == "provision_failed":
                    raise SystemExit("Device rejected configuration: "+data.get("error", "unknown"))
                if provisioned and data.get("event") == "status" and data.get("ip"):
                    private_json(args.access, {"username": "admin", "password": password, "ip": data["ip"], "url": "http://"+data["ip"]+"/"})
                    print(json.dumps({k: data.get(k) for k in ["version", "ip", "clockReady", "lastNtpSync", "phase", "freeHeap"]}), flush=True)
                    print("Private login file:", args.access, flush=True)
                    if data.get("clockReady") and data.get("lastNtpSync"):
                        return
        raise SystemExit("Wi-Fi/NTP verification timed out; saved settings remain on device. Check AP setup and private login file.")


if __name__ == "__main__":
    main()
