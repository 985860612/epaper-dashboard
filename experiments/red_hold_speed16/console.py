# /// script
# dependencies = ["pyserial>=3.5,<4"]
# ///
"""Explicit one-shot USB commands. Never flashes, provisions, or writes NVS."""
import argparse
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True)
parser.add_argument("--action", choices=("status", "baseline", "try"), default="status")
parser.add_argument("--log", type=Path, required=True)
args = parser.parse_args()
args.log.parent.mkdir(parents=True, exist_ok=True)
with args.log.open("a", buffering=1) as log, serial.Serial(args.port,115200,timeout=0.15,write_timeout=2) as port:
    def record(line):
        log.write(line+"\n")
        print(line,flush=True)
    # Establish identity before sending a display command. A reboot/unarmed
    # response cannot silently trigger BASELINE or retry the experiment.
    port.write(b"STATUS\n");port.flush()
    deadline=time.monotonic()+8
    status=None
    while time.monotonic()<deadline:
        line=port.readline().decode(errors="replace").strip()
        if line:record(line)
        if line.startswith("PROBE_STATUS version=red-native-hold-speed16 "):
            status=line;break
    if status is None:
        raise SystemExit("No matching probe firmware; no display command sent.")
    if args.action=="status":raise SystemExit(0)
    fields=dict(part.split("=",1) for part in status.split() if "=" in part)
    if fields.get("busy")!="0":raise SystemExit("BUSY high; no command sent.")
    if args.action=="try" and any(fields.get(k)!=v for k,v in (("armed","1"),("attempted","0"),("failed","0"))):
        raise SystemExit("Probe is not armed, was reset, or already attempted; no retry sent.")
    if args.action=="baseline" and fields.get("attempted")!="0":
        raise SystemExit("Already attempted; restore production firmware first.")
    record("HOST_SEND "+args.action.upper())
    port.write((args.action.upper()+"\n").encode());port.flush()
    deadline=time.monotonic()+(70 if args.action=="baseline" else 40)
    electrically_valid=False
    while time.monotonic()<deadline:
        line=port.readline().decode(errors="replace").strip()
        if not line:continue
        record(line)
        if line.startswith("PROBE_RESULT "):
            result=dict(part.split("=",1) for part in line.split() if "=" in part)
            electrically_valid=result.get("valid")=="1"
        if line.startswith(("BASELINE_FAILED","REJECT")):
            raise SystemExit("Stopped; inspect log and restore production firmware.")
        if args.action=="baseline" and line.startswith("ARMED="):
            if not line.startswith("ARMED=1 "):
                raise SystemExit("Native LUT did not pass compatibility checks; do not use TRY.")
            raise SystemExit(0)
        if args.action=="try" and line.startswith("PROBE_STOPPED"):
            if not electrically_valid:
                raise SystemExit("BUSY/readback validation failed; restore production; no retry.")
            record("HOST_DONE electrical_timing_only; visual_red_and_isolation_UNCONFIRMED")
            raise SystemExit(0)
    raise SystemExit("Timed out; no automatic retry. Restore production; power off if BUSY stays high.")
