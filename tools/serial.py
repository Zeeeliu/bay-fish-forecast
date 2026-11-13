#!/usr/bin/env python3
"""Talk to the Tab5 over USB serial (PRD §10.8). Needs pyserial.

  python3 tools/serial.py listen [seconds]            print device output
  python3 tools/serial.py status | report | fetch | alerts | reboot | debug on|off
  python3 tools/serial.py wifi_set "<ssid>" "<password>"
Port: $FISH_PORT or the first /dev/cu.usbmodem*.
"""
import glob, json, os, sys, time
import serial


def port():
    p = os.environ.get("FISH_PORT")
    for _ in range(50):
        c = [p] if p else sorted(glob.glob("/dev/cu.usbmodem*"))
        if c and os.path.exists(c[0]):
            return c[0]
        time.sleep(0.2)
    sys.exit("no /dev/cu.usbmodem* port (is the Tab5 connected with a data cable?)")


def open_port():
    for _ in range(25):
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = port(), 115200, 0.2
            # leave DTR/RTS at their defaults (both asserted): dropping DTR while RTS is high resets the ESP32-P4
            s.open()
            return s
        except (serial.SerialException, OSError):
            time.sleep(0.2)
    sys.exit("could not open the serial port (close any other serial monitor)")


def read_for(s, seconds, stop_on_json=False):
    end, buf = time.time() + seconds, b""
    while time.time() < end:
        try:
            chunk = s.read(4096)
        except (serial.SerialException, OSError):
            s.close(); time.sleep(0.5); s = open_port(); continue
        buf += chunk
        if chunk:
            text = chunk.decode("utf-8", "replace")
            sys.stdout.write(text); sys.stdout.flush()
            if stop_on_json and any(l.startswith("{") and l.endswith("}") for l in buf.decode("utf-8", "replace").splitlines()):
                return


def shot(s, path):
    import base64
    s.write(b'{"cmd":"shot"}\n')
    buf, end = b"", time.time() + 60
    while time.time() < end and b"-----END-----" not in buf:
        buf += s.read(65536)
    text = buf.decode("utf-8", "replace")
    if "-----PNG" not in text or "-----END-----" not in text:
        sys.exit("no screenshot received")
    body = text.split("-----PNG", 1)[1].split("\n", 1)[1].split("-----END-----")[0]
    data = base64.b64decode("".join(l.strip() for l in body.splitlines() if l.strip() and not l.startswith("[")))
    open(path, "wb").write(data)
    print(f"saved {path} ({len(data)} bytes)")


def main():
    a = sys.argv[1:] or ["listen"]
    s = open_port()
    if a[0] == "listen":
        read_for(s, float(a[1]) if len(a) > 1 else 15)
        return
    cmd = {"cmd": a[0]}
    if a[0] == "shot":
        return shot(s, a[1] if len(a) > 1 else "screen.png")
    if a[0] == "settime":
        cmd["epoch"] = int(a[1]) if len(a) > 1 else int(time.time())
    if a[0] == "tap":
        cmd.update(x=int(a[1]), y=int(a[2]), hold=len(a) > 3 and a[3] == "hold")
    if a[0] == "wifi_set":
        cmd.update(ssid=a[1], password=a[2] if len(a) > 2 else "")
    elif a[0] == "debug":
        cmd["on"] = len(a) > 1 and a[1] == "on"
    data = (json.dumps(cmd, ensure_ascii=False) + "\n").encode()
    while data:
        n = s.write(data); data = data[n:]
    read_for(s, 8 if a[0] != "reboot" else 3, stop_on_json=a[0] != "reboot")


if __name__ == "__main__":
    main()
