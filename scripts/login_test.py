#!/usr/bin/env python3
"""
Login tester for the project — sends POST /api/login using the exact
parameters used in the firmware and logs every step + full response.

Defaults are taken from the codebase:
 - base URL: http://195.231.69.227:5556/
 - endpoint: /api/login
 - serial: AD-34-DFG-333
 - password: c1ef6429c5e0f753ff24a114de6ee7d4 (stored MD5 in device config)
 - Date header: 2026-01-23T13:25:13.218763+01:00 (project hardcoded Date)

Usage:
  python3 scripts/login_test.py
  python3 scripts/login_test.py --url http://host:5556 --serial MY-SERIAL --password <md5>

Logs: verbose console output + file `login_test.log` in project root.
"""

import argparse
import hashlib
import json
import logging
import sys
from datetime import datetime

# Prefer requests for clarity, fallback to urllib
try:
    import requests
    _HAS_REQUESTS = True
except Exception:
    from urllib import request as _urllib_request
    from urllib.error import HTTPError, URLError
    _HAS_REQUESTS = False

# Project defaults (extracted from the workspace)
DEFAULT_BASE_URL = "http://195.231.69.227:5556"
DEFAULT_PATH = "/api/login"
DEFAULT_SERIAL = "AD-34-DFG-333"
DEFAULT_PASSWORD = "c1ef6429c5e0f753ff24a114de6ee7d4"  # stored MD5 in device_config.c
DEFAULT_DATE_HDR = "2026-01-23T13:25:13.218763+01:00"  # hardcoded Date from http_services.c

LOG_FILE = "login_test.log"

# ---------- helpers ----------

def compute_password_from_date_mm_yyyy_dd(date_mm_yyyy_dd: str, serial: str) -> str:
    """Compute MD5(MMYYYYdd + serial) like project doc says."""
    s = f"{date_mm_yyyy_dd}{serial}"
    return hashlib.md5(s.encode("utf-8")).hexdigest()


def mm_yyyy_dd_from_iso(iso_dt: str) -> str:
    """Given ISO date string (e.g. 2026-01-23...), return MMYYYYdd string.
    This mimics format_date_mm_yyyy_dd() from the firmware.
    """
    # parse date part only
    try:
        dt = datetime.fromisoformat(iso_dt.replace("Z", ""))
    except Exception:
        # fallback: use today
        dt = datetime.now()
    return f"{dt.month:02d}{dt.year:04d}{dt.day:02d}"


# ---------- HTTP call ----------

def send_login(base_url: str, path: str, serial: str, password: str, date_hdr: str, timeout: int = 15):
    url = base_url.rstrip("/") + path
    headers = {
        "Content-Type": "application/json",
        "Date": date_hdr,
    }
    body = {"serial": serial, "password": password}
    payload = json.dumps(body)

    logging.debug("Prepared request URL: %s", url)
    logging.debug("Prepared headers: %s", headers)
    logging.debug("Prepared body: %s", payload)

    if _HAS_REQUESTS:
        logging.debug("Using requests library to POST")
        try:
            resp = requests.post(url, headers=headers, data=payload, timeout=timeout, verify=False)
        except Exception as e:
            logging.exception("Request failed")
            return {"error": str(e)}

        result = {
            "status_code": resp.status_code,
            "resp_headers": dict(resp.headers),
            "body_text": resp.text,
        }
        # try parse JSON
        try:
            result["body_json"] = resp.json()
        except Exception:
            result["body_json"] = None
        return result

    # fallback to urllib
    logging.debug("requests not available; using urllib")
    req = _urllib_request.Request(url, data=payload.encode("utf-8"), headers=headers, method="POST")
    try:
        with _urllib_request.urlopen(req, timeout=timeout) as resp:
            resp_body = resp.read().decode("utf-8", errors="replace")
            resp_headers = {k: v for k, v in resp.getheaders()}
            return {"status_code": resp.getcode(), "resp_headers": resp_headers, "body_text": resp_body}
    except HTTPError as e:
        try:
            body = e.read().decode("utf-8", errors="replace")
        except Exception:
            body = None
        return {"status_code": e.code, "error": str(e), "body_text": body}
    except URLError as e:
        return {"error": str(e)}


# ---------- CLI / main ----------

def main(argv):
    ap = argparse.ArgumentParser(description="Perform /api/login using project defaults and log everything.")
    ap.add_argument("--url", default=DEFAULT_BASE_URL, help="Base server URL (default from device_config)")
    ap.add_argument("--path", default=DEFAULT_PATH, help="Login path (default: /api/login)")
    ap.add_argument("--serial", default=DEFAULT_SERIAL, help="Device serial to send")
    ap.add_argument("--password", default=DEFAULT_PASSWORD, help="Password (MD5) to send")
    ap.add_argument("--date", default=DEFAULT_DATE_HDR, help="Date header to send (use project's default)")
    ap.add_argument("--compute-pwd-from-date", action="store_true", help="Compute MD5(MMYYYYdd+serial) from --date and show comparison")
    ap.add_argument("--show-json-only", action="store_true", help="Only pretty-print JSON response to stdout")
    ap.add_argument("--proxy-url", default=None, help="Optional: device proxy URL (e.g. http://192.168.1.50:80) to compare forwarded response)")
    args = ap.parse_args(argv)

    # configure logging
    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)
    fmt = logging.Formatter("%(asctime)s %(levelname)-5s %(message)s")

    ch = logging.StreamHandler(sys.stdout)
    ch.setLevel(logging.INFO)
    ch.setFormatter(fmt)
    root_logger.addHandler(ch)

    fh = logging.FileHandler(LOG_FILE, mode="w", encoding="utf-8")
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(fmt)
    root_logger.addHandler(fh)

    logging.info("Login tester starting — using project defaults where not overridden")

    if args.compute_pwd_from_date:
        mmdd = mm_yyyy_dd_from_iso(args.date)
        computed = compute_password_from_date_mm_yyyy_dd(mmdd, args.serial)
        logging.info("Computed MD5(MMYYYYdd+serial) using date '%s' => mm_yyyy_dd='%s' => %s", args.date, mmdd, computed)
        if computed != args.password:
            logging.warning("Provided password (from device config) DOES NOT MATCH computed value. Project stores password directly in config.")
        else:
            logging.info("Provided password matches computed MD5 value.")

    # Build request preview (same values used by send_login)
    req_url = args.url.rstrip('/') + args.path
    req_headers = {"Content-Type": "application/json", "Date": args.date}
    req_body = json.dumps({"serial": args.serial, "password": args.password})

    logging.info("--- REQUEST (to be sent) ---")
    logging.info("URL: %s", req_url)
    logging.info("Method: POST")
    logging.info("Headers: %s", req_headers)
    logging.info("Body: %s", req_body)
    logging.info("Authorization header (request): %s", "<none>")

    # perform request
    res = send_login(args.url, args.path, args.serial, args.password, args.date)

    # Log response details (show headers in console too)
    if isinstance(res, dict) and res.get("error"):
        logging.error("Request error: %s", res.get("error"))
        if res.get("body_text"):
            logging.debug("Response body (error): %s", res.get("body_text"))
        return 1

    # optional: compare the server response with device proxy response (if provided)
    if args.proxy_url:
        logging.info("Comparing server response with device proxy at %s%s", args.proxy_url.rstrip('/'), args.path)
        proxy_res = send_login(args.proxy_url, args.path, args.serial, args.password, args.date)
        if isinstance(proxy_res, dict) and proxy_res.get('error'):
            logging.error("Proxy request error: %s", proxy_res.get('error'))
        else:
            proxy_status = proxy_res.get('status_code')
            proxy_body = proxy_res.get('body_text')
            logging.info("server status=%s proxy status=%s", res.get('status_code'), proxy_status)
            if res.get('body_text') == proxy_body:
                logging.info("Server and proxy bodies are IDENTICAL (len=%d)", len(proxy_body or ""))
            else:
                logging.warning("Server and proxy bodies DIFFER: server_len=%d proxy_len=%d", len(res.get('body_text') or ""), len(proxy_body or ""))
                with open('server_body.json', 'w', encoding='utf-8') as f: f.write(res.get('body_text') or '')
                with open('proxy_body.json', 'w', encoding='utf-8') as f: f.write(proxy_body or '')
                logging.info("Wrote server_body.json and proxy_body.json for manual diffing")

    status = res.get("status_code")
    logging.info("HTTP status: %s", status)
    logging.info("Response headers: %s", res.get("resp_headers"))

    body_text = res.get("body_text")
    if body_text is None:
        logging.info("No response body")
        return 0

    # log full body (but cap to 8k in console) and always write full body to log file
    MAX_CONSOLE = 8192
    if len(body_text) > MAX_CONSOLE:
        logging.info("Response body (truncated in console, full in %s): %s... (total %d bytes)", LOG_FILE, body_text[:MAX_CONSOLE].replace('\n', '\\n'), len(body_text))
        logging.debug("Full response body: %s", body_text)
    else:
        logging.info("Response body: %s", body_text)

    # pretty print JSON if possible and surface access_token
    try:
        js = json.loads(body_text)
        logging.info("Response JSON (pretty):\n%s", json.dumps(js, indent=2, ensure_ascii=False))
        if args.show_json_only:
            print(json.dumps(js, indent=2, ensure_ascii=False))

        # expose access_token clearly in console output
        token = js.get("access_token") if isinstance(js, dict) else None
        if token:
            logging.info("access_token (from response body): %s", token)
            logging.info("Use in header: Authorization: Bearer %s", token)
    except Exception:
        logging.debug("Response body is not JSON")

    logging.info("Done — detailed log written to %s", LOG_FILE)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
