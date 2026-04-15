#!/usr/bin/env python3
import argparse
import hashlib
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime

DEFAULT_URL = "http://195.231.69.227"
DEFAULT_PORT = 5556
DEFAULT_BARCODE = "00011486026039906"


def md5_password(serial: str, when: datetime) -> str:
    date_part = f"{when.month:02d}{when.year:04d}{when.day:02d}"
    seed = f"{serial}{date_part}"
    return hashlib.md5(seed.encode("utf-8")).hexdigest()


def build_url(base_url: str, port: int) -> str:
    parsed = urllib.parse.urlparse(base_url)
    if parsed.scheme and parsed.netloc:
        return base_url if parsed.port else f"{base_url}:{port}"
    if base_url.startswith("//"):
        return f"http:{base_url}:{port}"
    return f"http://{base_url}:{port}"


def do_post(url: str, payload: dict, token: str | None = None) -> tuple[int, str]:
    body = json.dumps(payload).encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"

    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=15) as response:
            resp_body = response.read().decode("utf-8", errors="replace")
            return response.status, resp_body
    except urllib.error.HTTPError as exc:
        resp_body = exc.read().decode("utf-8", errors="replace")
        return exc.code, resp_body
    except urllib.error.URLError as exc:
        raise RuntimeError(f"HTTP request failed: {exc}") from exc


def parse_json(body: str) -> dict:
    try:
        return json.loads(body)
    except json.JSONDecodeError:
        raise RuntimeError(f"Invalid JSON response: {body}")


def run_tests(base_url: str, port: int, serial: str | None, token: str | None, barcode: str) -> int:
    base_url = build_url(base_url, port)
    print(f"Server base URL: {base_url}")

    auth_token = token
    if not auth_token:
        if not serial:
            raise ValueError("Seriale mandatory for login when no token is provided")
        login_url = urllib.parse.urljoin(base_url, "/api/login")
        password = md5_password(serial, datetime.utcnow())
        print(f"Login: serial={serial} password={password}")
        status, body = do_post(login_url, {"serial": serial, "password": password})
        print(f"[LOGIN] {status}\n{body}\n")
        if status < 200 or status >= 300:
            raise RuntimeError(f"Login failed with HTTP status {status}")
        data = parse_json(body)
        auth_token = data.get("access_token")
        if not auth_token:
            raise RuntimeError(f"Login response missing access_token: {body}")
        print(f"Token ricevuto: {auth_token[:32]}... (len={len(auth_token)})")

    print("\n[GETCUSTOMERS] chiamata con token")
    getcustomers_url = urllib.parse.urljoin(base_url, "/api/getcustomers")
    status, body = do_post(getcustomers_url, {"Code": barcode, "Telephone": ""}, auth_token)
    print(f"[GETCUSTOMERS] {status}\n{body}\n")

    print("[KEEPALIVE] chiamata con token")
    keepalive_url = urllib.parse.urljoin(base_url, "/api/keepalive")
    status, body = do_post(keepalive_url, {"status": "ping"}, auth_token)
    print(f"[KEEPALIVE] {status}\n{body}\n")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Test HTTP services remoto con login e token bearer")
    parser.add_argument("-u", "--url", default=DEFAULT_URL, help=f"Base URL del server remoto (default: {DEFAULT_URL})")
    parser.add_argument("-p", "--port", default=DEFAULT_PORT, type=int, help=f"Porta del server remoto (default: {DEFAULT_PORT})")
    parser.add_argument("-s", "--serial", default=None, help="Seriale usato per il login (necessario se non è passato un token)")
    parser.add_argument("-t", "--token", default=None, help="Bearer token già disponibile (skippa il login)")
    parser.add_argument("-c", "--code", default=DEFAULT_BARCODE, help=f"Barcode cliente per /api/getcustomers (default: {DEFAULT_BARCODE})")
    args = parser.parse_args()

    try:
        return run_tests(args.url, args.port, args.serial, args.token, args.code)
    except Exception as exc:
        print(f"Errore: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
