#!/usr/bin/env python3
"""Flash Bee provisioning: set WiFi, OTA and MQTT settings over the USB serial console.

    python3 tools/provision.py [port]
    uv run --with pyserial tools/provision.py [port]

Asks for the WiFi SSID, WiFi password, OTA password, MQTT host, MQTT user and MQTT password
(Enter skips a field; every password is read with getpass, so it is not echoed). Sends the matching
console lines and prints the board's replies, which never contain a password.
The port is opened without resetting the board (RTS low first, then DTR low;
setting DTR first resets the C3 on macOS).
Nothing is written to disk. Only pyserial is needed.
"""
import getpass
import glob
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing. Run: uv run --with pyserial tools/provision.py")

LINE_MAX = 80          # CON_LINE_MAX in FlashBee.h: longer console lines are discarded


def open_port(path):
    s = serial.Serial(path, 115200, timeout=0.1)
    s.rts = False      # RTS first, then DTR: the other order, or a plain open with DTR set, resets the board
    s.dtr = False
    return s


def read_for(s, seconds):
    end = time.time() + seconds
    buf = b""
    while time.time() < end:
        buf += s.read(4096)
    return buf.decode(errors="replace")


def send(s, line):
    """Send one console line, print the board's reply lines. Returns True if the board answered ok."""
    if len(line) >= LINE_MAX:
        print(f"  skipped: the line is {len(line)} characters, the console takes {LINE_MAX - 1}")
        return False
    s.reset_input_buffer()
    s.write((line + "\n").encode())
    reply = read_for(s, 0.8)
    ok = False
    for ln in reply.splitlines():
        ln = ln.strip()
        if ln.startswith("[CON]") or ln.startswith("[WIFI]") or ln.startswith("[MQTT]") or ln.startswith("[OTA]"):
            print("  " + ln)
        if ln.startswith("[CON] ok"):
            ok = True
    if not ok:
        print("  no 'ok' from the board")
    return ok


def main():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    port = sys.argv[1] if len(sys.argv) > 1 else (ports[0] if ports else None)
    if not port:
        sys.exit("No /dev/cu.usbmodem* port. Plug the board in, or pass the port as an argument.")
    print(f"Port {port}. Enter skips a field. Passwords are not shown.")

    fields = [
        ("WiFi SSID", "wifi ssid", False),
        ("WiFi password", "wifi pass", True),
        ("OTA password", "ota pass", True),
        ("MQTT host (IP or name)", "mqtt host", False),
        ("MQTT user", "mqtt user", False),
        ("MQTT password", "mqtt pass", True),
    ]
    todo = []
    for label, cmd, secret in fields:
        value = getpass.getpass(f"{label}: ") if secret else input(f"{label}: ")
        if value:
            todo.append((cmd, value, secret))
    if not todo:
        print("Nothing to send.")
        return

    s = open_port(port)
    time.sleep(0.3)
    s.reset_input_buffer()
    for cmd, value, secret in todo:
        print(f"> {cmd} {'********' if secret else value}")
        send(s, f"{cmd} {value}")
    print("> checking the result")
    for cmd in ("wifi", "ota", "mqtt"):
        send(s, cmd)
    s.close()
    todo.clear()


if __name__ == "__main__":
    main()
