#!/usr/bin/env python3
"""
One-shot uploader that can build if needed, then sends firmware.bin and spiffs.bin
to the device, waiting for the ESP32 to reboot between stages.

Defaults are set for your current setup:
- Host: http://192.168.2.223
- Build env: esp32-s3-devkitc-1
- Artifacts under .pio/build/<env>/:
    firmware.bin
    spiffs.bin

Usage (recommended):
    python scripts/http_upload.py

Overrides:
    --host http://other-ip
    --env <pio-env-name>
    --firmware-path <file>
    --spiffs-path <file>
    --no-firmware / --no-spiffs to skip either stage
    --skip-build to skip automatic rebuild checks
"""

import argparse
import http.client
import mimetypes
import os
import sys
import urllib.parse
import uuid
import time
import socket
import subprocess


DEFAULT_HOST = "http://192.168.2.223"
DEFAULT_ENV = "esp32-s3-devkitc-1"
DEFAULT_BUILD_ROOT = r"C:\Users\lando\LED_Matrices\.pio\build"


def build_multipart_body(fields, files, boundary: str) -> bytes:
    """
    Build a multipart/form-data body.
    fields: list of (name, value) pairs
    files: list of (fieldname, filename, content_bytes)
    """
    lines = []
    boundary_bytes = boundary.encode()
    for name, value in fields:
        lines.append(b"--" + boundary_bytes)
        header = f'Content-Disposition: form-data; name="{name}"'.encode()
        lines.append(header)
        lines.append(b"")
        lines.append(str(value).encode())

    for field_name, filename, content in files:
        lines.append(b"--" + boundary_bytes)
        disposition = f'Content-Disposition: form-data; name="{field_name}"; filename="{os.path.basename(filename)}"'
        lines.append(disposition.encode())
        content_type = mimetypes.guess_type(filename)[0] or "application/octet-stream"
        lines.append(f"Content-Type: {content_type}".encode())
        lines.append(b"")
        lines.append(content)

    lines.append(b"--" + boundary_bytes + b"--")
    lines.append(b"")
    return b"\r\n".join(lines)


def post_multipart(url: str, file_path: str, field_name: str = "file", timeout: int = 120) -> None:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme not in ("http", "https"):
        raise ValueError(f"Unsupported scheme in URL: {url}")

    conn_cls = http.client.HTTPSConnection if parsed.scheme == "https" else http.client.HTTPConnection
    host = parsed.hostname
    port = parsed.port or (443 if parsed.scheme == "https" else 80)
    path = parsed.path or "/"

    with open(file_path, "rb") as f:
        content = f.read()

    boundary = uuid.uuid4().hex
    body = build_multipart_body([], [(field_name, file_path, content)], boundary)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body)),
    }

    conn = conn_cls(host, port, timeout=timeout)
    conn.request("POST", path, body=body, headers=headers)
    resp = conn.getresponse()
    resp_body = resp.read().decode(errors="ignore")
    conn.close()

    if resp.status >= 300:
        raise RuntimeError(f"Upload failed: {resp.status} {resp.reason} :: {resp_body}")

    print(f"[OK] {file_path} -> {url} :: {resp.status} {resp.reason} :: {resp_body}")


def wait_for_boot(host: str, port: int = 80, path: str = "/", timeout: int = 60) -> None:
    """
    Wait until the device responds over HTTP after a reboot.
    """
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            conn = http.client.HTTPConnection(host, port, timeout=5)
            conn.request("GET", path)
            resp = conn.getresponse()
            resp.read()
            conn.close()
            if resp.status < 500:
                print(f"[READY] Device responded with {resp.status} {resp.reason}")
                return
        except (ConnectionRefusedError, socket.timeout, OSError):
            time.sleep(1.5)
        except Exception as e:
            print(f"[WAIT] Unexpected error while waiting: {e}")
            time.sleep(1.5)
    raise TimeoutError(f"Device at {host}:{port} did not come back within {timeout}s")


def latest_mtime(paths) -> float:
    newest = 0.0
    for path in paths:
        if not os.path.exists(path):
            continue
        if os.path.isfile(path):
            newest = max(newest, os.path.getmtime(path))
        else:
            for root, _, files in os.walk(path):
                for fname in files:
                    fpath = os.path.join(root, fname)
                    try:
                        newest = max(newest, os.path.getmtime(fpath))
                    except OSError:
                        continue
    return newest


def needs_rebuild(artifact: str, inputs) -> bool:
    if not artifact or not os.path.isfile(artifact):
        return True
    artifact_mtime = os.path.getmtime(artifact)
    sources_mtime = latest_mtime(inputs)
    return sources_mtime > artifact_mtime


def run_pio(cmd):
    print(f"[BUILD] {' '.join(cmd)}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(result.stdout)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(cmd)}")


def main() -> int:
    parser = argparse.ArgumentParser(description="HTTP uploader for firmware and SPIFFS images.")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Device base URL (default: %(default)s)")
    parser.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO environment name (default: %(default)s)")
    parser.add_argument("--firmware-path", help="Path to firmware.bin (default: derived from env)")
    parser.add_argument("--spiffs-path", help="Path to spiffs.bin (default: derived from env)")
    parser.add_argument("--no-firmware", action="store_true", help="Skip firmware upload")
    parser.add_argument("--no-spiffs", action="store_true", help="Skip SPIFFS upload")
    parser.add_argument("--wait-timeout", type=int, default=60, help="Seconds to wait for device to reboot (default: %(default)s)")
    parser.add_argument("--http-timeout", type=int, default=120, help="HTTP request timeout seconds (default: %(default)s)")
    parser.add_argument("--skip-build", action="store_true", help="Skip automatic build checks")
    args = parser.parse_args()

    base = args.host.rstrip("/")
    parsed = urllib.parse.urlparse(base)
    host_only = parsed.hostname
    port = parsed.port or 80

    do_fw = not args.no_firmware
    do_fs = not args.no_spiffs

    # Resolve artifact paths
    env_root = os.path.join(DEFAULT_BUILD_ROOT, args.env)
    firmware_path = args.firmware_path or os.path.join(env_root, "firmware.bin")
    spiffs_path = args.spiffs_path or os.path.join(env_root, "spiffs.bin")

    # Build if needed
    if not args.skip_build:
        if do_fw and needs_rebuild(firmware_path, ["src", "include", "lib", "platformio.ini"]):
            try:
                run_pio(["pio", "run", "-e", args.env])
            except RuntimeError as exc:
                print(exc)
                return 1
        if do_fs and needs_rebuild(spiffs_path, ["data", "platformio.ini"]):
            try:
                run_pio(["pio", "run", "-t", "buildfs", "-e", args.env])
            except RuntimeError as exc:
                print(exc)
                return 1

    if do_fw:
        if not firmware_path or not os.path.isfile(firmware_path):
            print(f"Missing firmware.bin (expected at {firmware_path}). Build first or pass --firmware-path.")
            return 1
        print(f"[STEP] Uploading firmware -> {base}/update")
        try:
            post_multipart(f"{base}/update", firmware_path, timeout=args.http_timeout)
        except TimeoutError:
            print("Firmware upload timed out. Try increasing --http-timeout.")
            return 1
        print("[STEP] Waiting for device reboot after firmware...")
        wait_for_boot(host_only, port=port, timeout=args.wait_timeout)

    if do_fs:
        if not spiffs_path or not os.path.isfile(spiffs_path):
            print(f"Missing spiffs.bin (expected at {spiffs_path}). Build first or pass --spiffs-path.")
            return 1
        print(f"[STEP] Uploading SPIFFS -> {base}/updatefs")
        try:
            post_multipart(f"{base}/updatefs", spiffs_path, timeout=args.http_timeout)
        except TimeoutError:
            print("SPIFFS upload timed out. Try increasing --http-timeout.")
            return 1
        print("[STEP] Waiting for device reboot after SPIFFS...")
        wait_for_boot(host_only, port=port, timeout=args.wait_timeout)

    print("Uploads complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
