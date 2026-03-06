#!/usr/bin/env python3

from __future__ import annotations

import argparse
import ast
import html
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


CONST_DECL_RE = re.compile(r"const\s+char\s+\*\s*([A-Z0-9_]+)\s*=", re.MULTILINE)

BASE_STYLE = (
    "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}"
    "header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
    ".container{max-width:1000px;margin:20px auto;padding:0 20px}"
)

NAV_HTML = (
    "<nav>"
    "<a href='/'>🏠 Home</a>"
    "<a href='/config'>⚙️ Config</a>"
    "<a href='/test'>🔧 Test</a>"
    "<a href='/files'>📁 File</a>"
    "<a href='/tasks'>📋 Task</a>"
    "<a href='/logs'>📋 Log</a>"
    "<a href='/config/programs'>📊 Programmi</a>"
    "<a href='/ota'>🔄 OTA</a>"
    "</nav>"
)

GLOBAL_FETCH_WRAPPER = (
    "<script>/* Global fetch wrapper: injects Authorization */"
    "(function(){if(window.__auth_wrapped) return; window.__auth_wrapped=true; const _fetch = window.fetch.bind(window);"
    "window.setAuthToken = function(t){ if(t) localStorage.setItem('httpservices_token', t); else localStorage.removeItem('httpservices_token'); };"
    "window.getAuthToken = function(){ return localStorage.getItem('httpservices_token'); };"
    "window.clearAuthToken = function(){ localStorage.removeItem('httpservices_token'); };"
    "window.fetch = function(input, init){ try{ const token = window.getAuthToken(); if(token){ init = init || {}; if(!init.headers){ init.headers = {'Authorization':'Bearer '+token}; } else if(init.headers instanceof Headers){ if(!init.headers.get('Authorization')) init.headers.set('Authorization','Bearer '+token); } else if(Array.isArray(init.headers)){ let has=false; for(const h of init.headers){ if(h[0].toLowerCase()==='authorization'){ has=true; break; } } if(!has) init.headers.push(['Authorization','Bearer '+token]); } else if(typeof init.headers==='object'){ if(!init.headers['Authorization'] && !init.headers['authorization']) init.headers['Authorization'] = 'Bearer '+token; } } }catch(e){} return _fetch(input, init); };"
    "window.goProtectedPath=function(path){window.location.href=path;return false;};"
    "(function(){function tc(){var e=document.getElementById('hdr_clock');if(e)e.textContent=new Date().toTimeString().slice(0,8);}tc();setInterval(tc,1000);})();"
    # Aggiorna dinamicamente l'etichetta partizione nell'header leggendo /status
    "(function(){fetch('/status',{cache:'no-store'}).then(function(r){return r.json();}).then(function(s){var el=document.getElementById('hdr_run_mode');if(el&&s&&s.partition_running)el.textContent=s.partition_running.toUpperCase();}).catch(function(){});})();"
    "})();</script>"
)

CONFIG_READ_ONLY_SCRIPT = (
    "<script>(function(){"
    "var box=document.createElement('div');"
    "box.style='margin:16px 20px;padding:10px 12px;background:#fff3cd;color:#856404;border:1px solid #ffeeba;border-radius:6px;font-weight:bold;';"
    "box.innerText='Modalità APP: configurazione in sola lettura';"
    "var c=document.querySelector('.container');if(c){c.insertBefore(box,c.firstChild);}"
    "var form=document.getElementById('configForm');"
    "if(form){form.addEventListener('submit',function(e){e.preventDefault();alert('Modalità APP: modifica configurazione non consentita.');});"
    "form.querySelectorAll('input,select,textarea,button').forEach(function(el){el.disabled=true;});}"
    "})();</script>"
)

HOME_SYS_TASK_WIDGET = (
    "<div class='card' id='home_sys_task'>"
    "<h2>💻 CPU e Task</h2>"
    "<div style='margin-top:6px'>"
    "<div class='hst-row'><span class='hst-lbl'>Core 0</span><div class='hst-bar'><div class='hst-fill hst-cpu' id='hst_c0_fill'></div></div><span class='hst-val' id='hst_cpu0'>--</span></div>"
    "<div class='hst-row'><span class='hst-lbl'>Core 1</span><div class='hst-bar'><div class='hst-fill hst-cpu' id='hst_c1_fill'></div></div><span class='hst-val' id='hst_cpu1'>--</span></div>"
    "<div class='hst-row'><span class='hst-lbl'>Uptime</span><div class='hst-bar'></div><span class='hst-val' id='hst_uptime'>--</span></div>"
    "</div>"
    "<style>"
    ".hst-row{display:flex;align-items:center;gap:10px;padding:5px 0;border-bottom:1px solid #ecf0f1}"
    ".hst-lbl{width:60px;font-size:12px;font-weight:bold;color:#34495e;flex-shrink:0}"
    ".hst-bar{flex:1;background:#ecf0f1;border-radius:4px;height:13px;overflow:hidden}"
    ".hst-fill{height:100%;background:#3498db;border-radius:4px;width:0%;transition:width .35s}"
    ".hst-cpu{background:#e67e22}"
    ".hst-val{width:130px;font-size:12px;font-family:monospace;color:#555;text-align:right;flex-shrink:0}"
    "</style>"
    "<div style='margin-top:10px'>"
    "<label style='display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;font-weight:bold;color:#2c3e50'>"
    "<span>RunTime Stats</span>"
    "<span style='font-size:12px;color:#7f8c8d'>refresh 2s</span>"
    "</label>"
    "<textarea id='hst_runtime' readonly style='width:100%;min-height:170px;resize:vertical;font-family:monospace;font-size:11px;line-height:1.35;padding:8px;border:1px solid #dfe6e9;border-radius:6px;background:#f8fafc;color:#2d3436'></textarea>"
    "</div>"
    "<script>(function(){"
    "function fmtUptime(s){var n=Math.max(0,parseInt(s||0,10));var d=Math.floor(n/86400);var h=Math.floor((n%86400)/3600);var m=Math.floor((n%3600)/60);var ss=n%60;return (d?d+'g ':'')+h+'h '+m+'m '+ss+'s';}"
    "async function loadSys(){try{var r=await fetch('/api/sysinfo',{cache:'no-store'});if(!r.ok)return;var j=await r.json();"
    "var c0=(j&&j.cpu&&typeof j.cpu.core0_pct==='number')?j.cpu.core0_pct:((j&&typeof j.core0_pct==='number')?j.core0_pct:null);"
    "var c1=(j&&j.cpu&&typeof j.cpu.core1_pct==='number')?j.cpu.core1_pct:((j&&typeof j.core1_pct==='number')?j.core1_pct:null);"
    "var c0ok=(typeof c0==='number'&&isFinite(c0)&&c0>=0);"
    "var c1ok=(typeof c1==='number'&&isFinite(c1)&&c1>=0);"
    "var c0p=c0ok?Math.max(0,Math.min(100,c0)):0;"
    "var c1p=c1ok?Math.max(0,Math.min(100,c1)):0;"
    "document.getElementById('hst_c0_fill').style.width=c0p+'%';"
    "document.getElementById('hst_c1_fill').style.width=c1p+'%';"
    "document.getElementById('hst_cpu0').textContent=(c0ok?(Math.round(c0*10)/10+'%'):'n/d');"
    "document.getElementById('hst_cpu1').textContent=(c1ok?(Math.round(c1*10)/10+'%'):'n/d');"
    "document.getElementById('hst_uptime').textContent=fmtUptime(j&&j.uptime_s);"
    "}catch(e){}}"
    "async function loadRt(){try{var r=await fetch('/api/runtime_stats',{cache:'no-store'});if(!r.ok)return;var t=await r.text();var b=document.getElementById('hst_runtime');if(b)b.value=t||'';}catch(e){}}"
    "loadSys();loadRt();setInterval(loadSys,2000);setInterval(loadRt,2000);"
    "})();</script>"
    "</div>"
)


@dataclass(frozen=True)
class PageSpec:
    filename: str
    title: str
    extra_style_symbol: str
    body_builder: Callable[[dict[str, str], argparse.Namespace], str]
    show_nav: bool = True
    is_emulator_page: bool = False


def read_app_version(source_path: Path) -> tuple[str, str]:
    """Legge APP_VERSION e APP_DATE da main/app_version.h relativo al sorgente."""
    version_file = source_path.parent.parent.parent / "main" / "app_version.h"
    if not version_file.is_file():
        return "dev", "N/A"
    content = version_file.read_text(encoding="utf-8")
    ver_match = re.search(r'#define\s+APP_VERSION\s+"([^"]+)"', content)
    date_match = re.search(r'#define\s+APP_DATE\s+"([^"]+)"', content)
    version = ver_match.group(1) if ver_match else "dev"
    build_date = date_match.group(1) if date_match else "N/A"
    return version, build_date


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Esporta i file .html partendo dalle costanti embedded WEBPAGE_*.",
    )
    parser.add_argument(
        "--source",
        default="components/web_ui/webpages_embedded.c",
        help="Percorso del file C con le costanti embedded.",
    )
    parser.add_argument(
        "--output",
        default="data/www",
        help="Cartella di destinazione per i file HTML esportati.",
    )
    parser.add_argument(
        "--pages",
        default="",
        help="Lista file separata da virgole (es: index.html,config.html).",
    )
    parser.add_argument("--lang", default="it", help="Lingua usata nello script i18n esportato.")
    parser.add_argument("--app-name", default="APP", help="Valore placeholder mostrato nell'header.")
    parser.add_argument("--version", default="dev", help="Versione placeholder mostrata nell'header.")
    parser.add_argument("--build-date", default="N/A", help="Data build placeholder mostrata nell'header.")
    parser.add_argument("--time-text", default="00:00:00", help="Orario iniziale placeholder nell'header.")
    parser.add_argument("--home-title", default="MH1001 control", help="Titolo usato per index.html.")
    parser.add_argument("--profile-label", default="Factory View", help="Profilo mostrato nella home.")
    parser.add_argument(
        "--show-factory-password-section",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Imposta window.__showFactoryPasswordSection nella pagina config.",
    )
    parser.add_argument(
        "--config-read-only",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Aggiunge lo script read-only della pagina /config.",
    )
    return parser.parse_args()


def skip_ws_and_comments(source: str, index: int) -> int:
    length = len(source)
    i = index
    while i < length:
        if source[i].isspace():
            i += 1
            continue
        if source.startswith("//", i):
            newline = source.find("\n", i + 2)
            if newline == -1:
                return length
            i = newline + 1
            continue
        if source.startswith("/*", i):
            end = source.find("*/", i + 2)
            if end == -1:
                raise ValueError("Commento multilinea non chiuso in webpages_embedded.c")
            i = end + 2
            continue
        break
    return i


def parse_c_string_literal(source: str, index: int) -> tuple[str, int]:
    if source[index] != '"':
        raise ValueError("Atteso inizio stringa C")

    i = index + 1
    escaped = False
    while i < len(source):
        ch = source[i]
        if ch == '"' and not escaped:
            token = source[index : i + 1]
            try:
                value = ast.literal_eval(token)
            except (SyntaxError, ValueError) as exc:
                raise ValueError(f"Stringa C non valida: {token[:40]}...") from exc
            return value, i + 1
        if ch == "\\" and not escaped:
            escaped = True
        else:
            escaped = False
        i += 1

    raise ValueError("Stringa C non chiusa in webpages_embedded.c")


def extract_string_constants(source: str) -> dict[str, str]:
    constants: dict[str, str] = {}
    for match in CONST_DECL_RE.finditer(source):
        symbol = match.group(1)
        i = match.end()
        chunks: list[str] = []

        while True:
            i = skip_ws_and_comments(source, i)
            if i >= len(source):
                raise ValueError(f"Assegnazione non terminata per {symbol}")

            ch = source[i]
            if ch == ";":
                i += 1
                break
            if ch == '"':
                value, i = parse_c_string_literal(source, i)
                chunks.append(value)
                continue

            i += 1

        constants[symbol] = "".join(chunks)

    return constants


def require_symbol(constants: dict[str, str], symbol: str) -> str:
    try:
        return constants[symbol]
    except KeyError as exc:
        raise KeyError(f"Simbolo mancante in webpages_embedded.c: {symbol}") from exc


def build_head(
    constants: dict[str, str],
    title: str,
    extra_style: str,
    show_nav: bool,
    is_emulator_page: bool,
    args: argparse.Namespace,
) -> str:
    if is_emulator_page:
        emu_button = require_symbol(constants, "WEBPAGE_COMMON_EMU_BUTTON_FMT_HOME") % "Home"
    else:
        emu_button = require_symbol(constants, "WEBPAGE_COMMON_EMU_BUTTON_FMT_EMU") % "Emulatore"

    nav_style = require_symbol(constants, "WEBPAGE_COMMON_STYLE_NAV") if show_nav else ""
    nav_html = NAV_HTML if show_nav else ""
    i18n_script = require_symbol(constants, "WEBPAGE_COMMON_I18N_SCRIPT_FMT") % (args.lang, "{}")

    return (
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>"
        + title
        + "</title><style>"
        + BASE_STYLE
        + nav_style
        + " "
        + extra_style
        + "</style></head><body>"
        + "<header>"
        + "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>"
        + html.escape(title)
        + " [<span id='hdr_run_mode'>"
        + html.escape(args.app_name)
        + "</span>] - <span id='hdr_clock'>"
        + html.escape(args.time_text)
        + "</span></h1>"
        + emu_button
        + "</div>"
        + "<div style='text-align:right;font-size:12px;opacity:0.8;'>v"
        + html.escape(args.version)
        + " ("
        + html.escape(args.build_date)
        + ")</div>"
        + "</header>"
        + nav_html
        + GLOBAL_FETCH_WRAPPER
        + i18n_script
    )


def body_from_symbol(symbol: str) -> Callable[[dict[str, str], argparse.Namespace], str]:
    def _builder(constants: dict[str, str], _args: argparse.Namespace) -> str:
        return require_symbol(constants, symbol)

    return _builder


def build_home_body(constants: dict[str, str], args: argparse.Namespace) -> str:
    return (
        "<div class='container'><div class='grid'>"
        "<a href='/config' class='btn-link btn-config'><span class='icon'>⚙️</span><span>Configurazione</span></a>"
        "<a href='/stats' class='btn-link'><span class='icon'>📈</span><span>Statistiche</span></a>"
        "<a href='/files' class='btn-link'><span class='icon'>📁</span><span>File Manager</span></a>"
        + require_symbol(constants, "WEBPAGE_HOME_LINK_TEST")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_TASKS")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_HTTPSERVICES")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_EMULATOR")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_MANTAINER")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_PROGRAMS")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_LOGS")
        + "<a href='/ota' class='btn-link btn-ota'><span class='icon'>🔄</span><span>Update OTA</span></a>"
        + "</div>"
        + "<div class='card'><h2>ℹ️ Informazioni</h2>"
        + "<p><strong>Profilo:</strong> "
        + html.escape(args.profile_label)
        + "</p>"
        + "<p>Benvenuti nell'interfaccia di configurazione e test.</p>"
        + "<div style='margin-top:20px; border-top:1px solid #eee; padding-top:15px;'>"
        + "<h3 style='margin:0 0 10px 0;color:#2c3e50;'>Reboot</h3>"
        + "<div style='display:flex;flex-wrap:wrap;gap:8px;'>"
        + "<a href='#' onclick=\"return window.goProtectedPath('/reboot/factory');\" class='btn-reboot' style='background:#c0392b;'>FACTORY</a>"
        + "<a href='/reboot/app_last' class='btn-reboot' style='background:#27ae60;'>APP LAST</a>"
        + "<a href='/reboot/ota0' class='btn-reboot' style='background:#2980b9;'>OTA0</a>"
        + "<a href='/reboot/ota1' class='btn-reboot' style='background:#8e44ad;'>OTA1</a>"
        + "<div style='margin-left:auto;display:flex;flex-wrap:wrap;gap:8px;'>"
        + "<a href='/api' class='btn-reboot' style='background:#3498db;'>API</a>"
        + require_symbol(constants, "WEBPAGE_HOME_LINK_REBOOT_FACTORY_CRASH")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_REBOOT_APP_RESTORE")
        + require_symbol(constants, "WEBPAGE_HOME_LINK_PROMOTE_FACTORY")
        + "</div></div></div></div>"
        + require_symbol(constants, "WEBPAGE_HOME_SERVICE_STATUS_WIDGET")
        + HOME_SYS_TASK_WIDGET
        + "</div></body></html>"
    )


def build_config_body(constants: dict[str, str], args: argparse.Namespace) -> str:
    show_flag = "true" if args.show_factory_password_section else "false"
    injected_script = f"<script>window.__showFactoryPasswordSection={show_flag};</script>"
    read_only_script = CONFIG_READ_ONLY_SCRIPT if args.config_read_only else ""
    return injected_script + require_symbol(constants, "WEBPAGE_CONFIG_BODY") + read_only_script


def page_specs() -> list[PageSpec]:
    return [
        PageSpec(
            filename="index.html",
            title="MH1001 control",
            extra_style_symbol="WEBPAGE_HOME_EXTRA_STYLE",
            body_builder=build_home_body,
            show_nav=True,
            is_emulator_page=False,
        ),
        PageSpec(
            filename="config.html",
            title="Configurazione Device",
            extra_style_symbol="WEBPAGE_CONFIG_EXTRA_STYLE",
            body_builder=build_config_body,
            show_nav=True,
            is_emulator_page=False,
        ),
        PageSpec(
            filename="ota.html",
            title="Aggiornamento OTA",
            extra_style_symbol="WEBPAGE_OTA_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_OTA_BODY"),
        ),
        PageSpec(
            filename="stats.html",
            title="Statistiche Device",
            extra_style_symbol="WEBPAGE_STATS_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_STATS_BODY"),
        ),
        PageSpec(
            filename="tasks.html",
            title="Editor Tasks",
            extra_style_symbol="WEBPAGE_TASKS_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_TASKS_BODY"),
        ),
        PageSpec(
            filename="httpservices.html",
            title="HTTP Services",
            extra_style_symbol="WEBPAGE_HTTPSERVICES_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_HTTPSERVICES_BODY"),
        ),
        PageSpec(
            filename="api.html",
            title="API Endpoints",
            extra_style_symbol="WEBPAGE_API_INDEX_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_API_INDEX_BODY"),
        ),
        PageSpec(
            filename="files.html",
            title="File Manager",
            extra_style_symbol="WEBPAGE_FILES_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_FILES_BODY"),
        ),
        PageSpec(
            filename="logs.html",
            title="Log Remoto",
            extra_style_symbol="WEBPAGE_LOGS_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_LOGS_BODY"),
        ),
        PageSpec(
            filename="test.html",
            title="Test Hardware",
            extra_style_symbol="WEBPAGE_TEST_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_TEST_BODY"),
        ),
        PageSpec(
            filename="programs.html",
            title="Tabella Programmi",
            extra_style_symbol="WEBPAGE_PROGRAMS_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_PROGRAMS_BODY"),
        ),
        PageSpec(
            filename="emulator.html",
            title="Emulator",
            extra_style_symbol="WEBPAGE_EMULATOR_EXTRA_STYLE",
            body_builder=body_from_symbol("WEBPAGE_EMULATOR_BODY"),
            show_nav=False,
            is_emulator_page=True,
        ),
    ]


def parse_selected_pages(raw_pages: str) -> set[str]:
    if not raw_pages.strip():
        return set()
    return {part.strip() for part in raw_pages.split(",") if part.strip()}


def main() -> int:
    args = parse_args()

    source_path = Path(args.source)
    if not source_path.is_file():
        print(f"Errore: file sorgente non trovato: {source_path}", file=sys.stderr)
        return 1

    # Popola version e build_date da app_version.h se non passati esplicitamente
    if args.version == "dev" or args.build_date == "N/A":
        auto_version, auto_date = read_app_version(source_path)
        if args.version == "dev":
            args.version = auto_version
        if args.build_date == "N/A":
            args.build_date = auto_date

    content = source_path.read_text(encoding="utf-8")
    constants = extract_string_constants(content)

    export_specs = page_specs()
    selected_pages = parse_selected_pages(args.pages)

    known_filenames = {spec.filename for spec in export_specs}
    unknown = selected_pages - known_filenames
    if unknown:
        print("Errore: pagine non riconosciute:", ", ".join(sorted(unknown)), file=sys.stderr)
        print("Pagine valide:", ", ".join(sorted(known_filenames)), file=sys.stderr)
        return 2

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    exported_count = 0
    for spec in export_specs:
        if selected_pages and spec.filename not in selected_pages:
            continue

        title = args.home_title if spec.filename == "index.html" else spec.title
        extra_style = require_symbol(constants, spec.extra_style_symbol)
        head = build_head(
            constants=constants,
            title=title,
            extra_style=extra_style,
            show_nav=spec.show_nav,
            is_emulator_page=spec.is_emulator_page,
            args=args,
        )
        body = spec.body_builder(constants, args)
        html_page = head + body

        file_path = output_dir / spec.filename
        file_path.write_text(html_page, encoding="utf-8")
        exported_count += 1
        print(f"[OK] {file_path}")

    if exported_count == 0:
        print("Nessun file esportato (controllare --pages).", file=sys.stderr)
        return 3

    print(f"Completato: esportati {exported_count} file HTML in {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
