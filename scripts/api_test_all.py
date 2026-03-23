#!/usr/bin/env python3
"""
Tester API automatico per firmware test_wave.

- Scopre gli endpoint API dal sorgente (`.uri="/api/..."` + `HTTP_*`).
- Esegue test HTTP verso il device.
- Riporta errori principali (404, timeout, 5xx, connessione).

Modalita predefinita: NON distruttiva.
Per metodi mutanti (POST/PUT/PATCH/DELETE) usa OPTIONS.
Per testare i metodi reali usare `--active-methods`.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from urllib import error, request
from urllib.parse import urlparse

URI_METHOD_RE = re.compile(
    r"\.uri\s*=\s*\"(?P<uri>[^\"]+)\"\s*,\s*\.method\s*=\s*HTTP_(?P<method>[A-Z]+)",
    re.MULTILINE,
)

API_PREFIX = "/api"
MUTATING_METHODS = {"POST", "PUT", "PATCH", "DELETE"}
SKIP_MUTATING_KEYWORDS = {
    "reset",
    "reboot",
    "restart",
    "erase",
    "format",
    "flash",
    "ota",
    "save",
    "backup",
}
FACTORY_ONLY_PREFIXES = (
    "/api/tasks",
    "/api/test/",
)


@dataclass(frozen=True)
class Endpoint:
    uri: str
    declared_method: str
    source_file: str


@dataclass
class TestResult:
    uri: str
    declared_method: str
    tested_method: str
    source_file: str
    status_code: Optional[int]
    ok: bool
    error: str
    detail: str


def is_factory_only_endpoint(uri: str) -> bool:
    return any(uri.startswith(prefix) for prefix in FACTORY_ONLY_PREFIXES)


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    parser = argparse.ArgumentParser(
        description="Testa automaticamente le API del firmware e segnala errori."
    )
    parser.add_argument(
        "--base-url",
        required=True,
        help="Base URL del device, es: http://192.168.2.153",
    )
    parser.add_argument(
        "--source-root",
        default=str(repo_root),
        help="Root sorgente da cui scoprire gli endpoint (default: root repo)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Timeout HTTP in secondi (default: 5)",
    )
    parser.add_argument(
        "--active-methods",
        action="store_true",
        help="Per POST/PUT/PATCH/DELETE invia il metodo reale (potenzialmente mutante).",
    )
    parser.add_argument(
        "--report-json",
        default="",
        help="Percorso output report JSON (default: report in scripts/).",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Stampa anche i test OK.",
    )
    return parser.parse_args()


def discover_endpoints(source_root: Path) -> List[Endpoint]:
    candidates: List[Path] = []
    for rel in ("components", "main"):
        p = source_root / rel
        if p.exists():
            candidates.extend(p.rglob("*.c"))

    endpoints: Dict[Tuple[str, str], Endpoint] = {}
    for file_path in candidates:
        try:
            text = file_path.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue

        for match in URI_METHOD_RE.finditer(text):
            uri = match.group("uri").strip()
            method = match.group("method").strip().upper()

            if not uri.startswith(API_PREFIX):
                continue

            concrete_uri = normalize_uri_for_test(uri)
            key = (concrete_uri, method)
            if key not in endpoints:
                endpoints[key] = Endpoint(
                    uri=concrete_uri,
                    declared_method=method,
                    source_file=str(file_path.relative_to(source_root)),
                )

    return sorted(endpoints.values(), key=lambda e: (e.uri, e.declared_method))


def normalize_uri_for_test(uri: str) -> str:
    if "*" not in uri:
        return uri

    # Per route wildcard (es: /api/test/*) costruiamo una URI concreta di probe.
    base = uri.replace("*", "").rstrip("/")
    return f"{base}/probe"


def choose_test_method(endpoint: Endpoint, active_methods: bool) -> str:
    if endpoint.declared_method in MUTATING_METHODS and not active_methods:
        return "OPTIONS"
    return endpoint.declared_method


def should_skip_active_call(endpoint: Endpoint, tested_method: str, active_methods: bool) -> bool:
    if not active_methods:
        return False
    if tested_method not in MUTATING_METHODS:
        return False
    lower_uri = endpoint.uri.lower()
    return any(keyword in lower_uri for keyword in SKIP_MUTATING_KEYWORDS)


def perform_request(base_url: str, uri: str, method: str, timeout: float) -> Tuple[Optional[int], str, str]:
    url = f"{base_url.rstrip('/')}{uri}"
    headers = {
        "User-Agent": "test_wave_api_tester/1.0",
        "Accept": "application/json, text/plain, */*",
    }

    data = None
    if method in MUTATING_METHODS:
        data = b"{}"
        headers["Content-Type"] = "application/json"

    req = request.Request(url=url, method=method, headers=headers, data=data)

    try:
        with request.urlopen(req, timeout=timeout) as resp:
            status = int(resp.status)
            body = resp.read(300).decode("utf-8", errors="replace")
            return status, "", body
    except error.HTTPError as http_err:
        body = ""
        try:
            body = http_err.read(300).decode("utf-8", errors="replace")
        except Exception:
            body = ""
        return int(http_err.code), "", body
    except error.URLError as url_err:
        return None, f"URLError: {url_err.reason}", ""
    except Exception as exc:
        return None, f"Exception: {exc}", ""


def detect_factory_runtime(base_url: str, timeout: float) -> Optional[bool]:
    status, err, detail = perform_request(base_url, "/status", "GET", timeout)
    if err or status is None or status >= 400:
        return None

    try:
        payload = json.loads(detail) if detail else {}
    except Exception:
        return None

    running = str(payload.get("partition_running", "")).lower()
    if not running:
        return None

    if "factory" in running:
        return True

    if "ota" in running or "app" in running:
        return False

    return None


def classify_result(endpoint: Endpoint, tested_method: str, status: Optional[int], err: str, detail: str) -> TestResult:
    if err:
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=False,
            error="network_error",
            detail=err,
        )

    if status is None:
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=None,
            ok=False,
            error="no_status",
            detail=detail,
        )

    if tested_method == "OPTIONS" and status in (404, 405):
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=True,
            error="",
            detail="OPTIONS non esposto (safe-mode)",
        )

    if status == 404:
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=False,
            error="missing_endpoint",
            detail="404 Non Trovato",
        )

    if status >= 500:
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=False,
            error="server_error",
            detail=detail,
        )

    if status in (401, 403):
        # endpoint presente ma protetto
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=True,
            error="",
            detail="endpoint presente (protetto)",
        )

    if status in (200, 201, 202, 204, 400, 405):
        return TestResult(
            uri=endpoint.uri,
            declared_method=endpoint.declared_method,
            tested_method=tested_method,
            source_file=endpoint.source_file,
            status_code=status,
            ok=True,
            error="",
            detail=detail,
        )

    return TestResult(
        uri=endpoint.uri,
        declared_method=endpoint.declared_method,
        tested_method=tested_method,
        source_file=endpoint.source_file,
        status_code=status,
        ok=False,
        error="unexpected_status",
        detail=detail,
    )


def default_report_path(script_dir: Path) -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return script_dir / f"api_test_report_{stamp}.json"


def normalize_base_url(raw_base_url: str) -> str:
    base = (raw_base_url or "").strip()
    if not base:
        raise ValueError("--base-url vuoto")

    if not re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*://", base):
        base = f"http://{base}"

    parsed = urlparse(base)
    if not parsed.scheme or not parsed.netloc:
        raise ValueError(f"Base URL non valido: {raw_base_url}")

    return base.rstrip("/")


def main() -> int:
    args = parse_args()

    try:
        base_url = normalize_base_url(args.base_url)
    except ValueError as exc:
        print(f"ERRORE: {exc}")
        return 4

    source_root = Path(args.source_root).resolve()
    if not source_root.exists():
        print(f"ERRORE: source root non trovato: {source_root}")
        return 2

    endpoints = discover_endpoints(source_root)
    if not endpoints:
        print("ERRORE: nessun endpoint /api scoperto nel sorgente")
        return 3

    print(f"Endpoint scoperti: {len(endpoints)}")
    print(f"Base URL: {base_url}")
    print(f"Modalita metodi attivi: {'ON' if args.active_methods else 'OFF (safe)'}")

    is_factory_runtime = detect_factory_runtime(base_url, args.timeout)
    if is_factory_runtime is True:
        print("Runtime rilevato: FACTORY")
    elif is_factory_runtime is False:
        print("Runtime rilevato: APP")
    else:
        print("Runtime rilevato: sconosciuto (impossibile leggere /status)")

    results: List[TestResult] = []
    skipped_active: List[TestResult] = []
    skipped_factory_scope: List[TestResult] = []

    for ep in endpoints:
        tested_method = choose_test_method(ep, args.active_methods)

        if is_factory_runtime is False and is_factory_only_endpoint(ep.uri):
            r = TestResult(
                uri=ep.uri,
                declared_method=ep.declared_method,
                tested_method=tested_method,
                source_file=ep.source_file,
                status_code=None,
                ok=True,
                error="",
                detail="SKIPPED endpoint factory-only (runtime APP)",
            )
            skipped_factory_scope.append(r)
            if args.verbose:
                print(f"[SKIP] {tested_method:7s} {ep.uri} (factory-only in APP)")
            continue

        if should_skip_active_call(ep, tested_method, args.active_methods):
            r = TestResult(
                uri=ep.uri,
                declared_method=ep.declared_method,
                tested_method=tested_method,
                source_file=ep.source_file,
                status_code=None,
                ok=True,
                error="",
                detail="SKIPPED active call (endpoint potenzialmente distruttivo)",
            )
            skipped_active.append(r)
            if args.verbose:
                print(f"[SKIP] {tested_method:7s} {ep.uri} ({ep.declared_method})")
            continue

        status, err, detail = perform_request(base_url, ep.uri, tested_method, args.timeout)
        res = classify_result(ep, tested_method, status, err, detail)
        results.append(res)

        if args.verbose or not res.ok:
            tag = "OK" if res.ok else "ERR"
            code = "-" if res.status_code is None else str(res.status_code)
            print(f"[{tag}] {tested_method:7s} {ep.uri:45s} -> {code:>3s} | {res.error or res.detail}")

    errors = [r for r in results if not r.ok]

    report_path = Path(args.report_json).resolve() if args.report_json else default_report_path(Path(__file__).resolve().parent)
    report = {
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "base_url": base_url,
        "source_root": str(source_root),
        "active_methods": bool(args.active_methods),
        "total_discovered": len(endpoints),
        "tested": len(results),
        "skipped_active": len(skipped_active),
        "skipped_factory_scope": len(skipped_factory_scope),
        "errors": len(errors),
        "error_items": [asdict(e) for e in errors],
        "all_items": [asdict(r) for r in results],
        "skipped_items": [asdict(r) for r in skipped_active],
        "skipped_factory_items": [asdict(r) for r in skipped_factory_scope],
    }
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    print("\n==== RIEPILOGO ====")
    print(f"Test eseguiti: {len(results)}")
    print(f"Saltati (safe-mode): {len(skipped_active)}")
    print(f"Saltati (factory-only): {len(skipped_factory_scope)}")
    print(f"Errori: {len(errors)}")
    print(f"Report JSON: {report_path}")

    if errors:
        print("\nErrori principali:")
        for e in errors[:30]:
            code = "-" if e.status_code is None else str(e.status_code)
            print(f"- {e.tested_method} {e.uri} -> {code} [{e.error}] ({e.source_file})")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
