#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from pathlib import Path

FLASH_SIZE_BYTES = 0x1000000  # 16 MB
DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = "460800"
DEFAULT_CHIP = "esp32p4"


def run_cmd(cmd: list[str]) -> int:
    print("[INFO] Esecuzione:", " ".join(cmd))
    result = subprocess.run(cmd)
    return result.returncode


def build_esptool_base(port: str, baud: str) -> list[str]:
    return [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        DEFAULT_CHIP,
        "-p",
        port,
        "-b",
        baud,
        "--before",
        "default_reset",
        "--after",
        "hard_reset",
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Gestione immagine completa flash del device (dump/scrittura totale)."
        )
    )
    parser.add_argument("-g", action="store_true", help="Legge tutta la flash dal device e la salva su file")
    parser.add_argument("-w", action="store_true", help="Scrive tutta la flash sul device leggendo dal file")
    parser.add_argument("-p", default=DEFAULT_PORT, help=f"Porta seriale (default: {DEFAULT_PORT})")
    parser.add_argument("-f", required=True, help="File da leggere/scrivere")
    parser.add_argument(
        "-s",
        "-b",
        dest="baud",
        default=DEFAULT_BAUD,
        help=f"Velocità seriale/baud rate (default: {DEFAULT_BAUD})",
    )

    args = parser.parse_args()

    if args.g == args.w:
        print("[ERRORE] Devi specificare una sola modalità: -g oppure -w")
        return 2

    image_path = Path(args.f)
    base_cmd = build_esptool_base(args.p, args.baud)

    if args.g:
        if image_path.parent and not image_path.parent.exists():
            image_path.parent.mkdir(parents=True, exist_ok=True)

        cmd = base_cmd + [
            "read_flash",
            "0x0",
            hex(FLASH_SIZE_BYTES),
            str(image_path),
        ]
        print(f"[INFO] Dump completo flash ({FLASH_SIZE_BYTES} bytes) su: {image_path}")
        return run_cmd(cmd)

    if not image_path.exists():
        print(f"[ERRORE] File non trovato: {image_path}")
        return 2

    file_size = image_path.stat().st_size
    if file_size != FLASH_SIZE_BYTES:
        print(
            f"[ERRORE] Dimensione file non valida: {file_size} bytes (atteso: {FLASH_SIZE_BYTES})"
        )
        return 2

    cmd = base_cmd + [
        "write_flash",
        "--force",
        "0x0",
        str(image_path),
    ]
    print(f"[INFO] Scrittura completa flash da file: {image_path}")
    return run_cmd(cmd)


if __name__ == "__main__":
    sys.exit(main())
