#!/usr/bin/env python3
"""
Script per cercare tutti i file *.c nella cartella corrente,
determinare quante righe hanno, ordinarli in modo decrescente
e creare un report in formato .md nella cartella ./scripts
"""

import os
import glob
import argparse
import re


def collect_if0_disabled_lines(lines):
    """Restituisce gli indici riga (0-based) che ricadono in blocchi #if 0 ... #endif."""
    disabled = set()
    if0_re = re.compile(r'^\s*#\s*if\s*(?:0\b|\(\s*0\s*\))')
    if_re = re.compile(r'^\s*#\s*if(?:def|ndef)?\b')
    endif_re = re.compile(r'^\s*#\s*endif\b')

    index = 0
    total = len(lines)

    while index < total:
        line = lines[index]
        if if0_re.match(line):
            depth = 1
            start = index
            index += 1
            while index < total and depth > 0:
                current = lines[index]
                if if_re.match(current):
                    depth += 1
                elif endif_re.match(current):
                    depth -= 1
                index += 1

            end = min(index, total) - 1
            disabled.update(range(start, end + 1))
            continue

        index += 1

    return disabled

def count_lines(file_path):
    """Conta il numero di righe in un file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return sum(1 for line in f)
    except Exception as e:
        print(f"Errore leggendo {file_path}: {e}")
        return 0


def analyze_functions(file_path):
    """Analizza le funzioni in un file C e restituisce una lista con (nome, riga_inizio, riga_fine, righe)."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Errore leggendo {file_path}: {e}")
        return []

    disabled_lines = collect_if0_disabled_lines(lines)

    signature_re = re.compile(
        r'^\s*(?:static\s+)?(?:inline\s+)?(?:const\s+)?[A-Za-z_][\w\s\*]*?\s+([A-Za-z_]\w*)\s*\([^;]*\)\s*$'
    )
    control_keywords = {'if', 'for', 'while', 'switch', 'return', 'else', 'do'}

    functions = []
    index = 0
    total = len(lines)

    while index < total:
        if index in disabled_lines:
            index += 1
            continue

        line = lines[index]

        if line.strip().startswith('#'):
            index += 1
            continue

        if '(' in line and not line.strip().startswith('//'):
            start = index
            chunk = line.rstrip()
            lookahead = index

            while lookahead + 1 < total and '{' not in chunk and ';' not in chunk:
                if (lookahead + 1) in disabled_lines:
                    break
                lookahead += 1
                chunk += ' ' + lines[lookahead].strip()

            if '{' in chunk and ';' not in chunk.split('{')[0]:
                header = chunk.split('{', 1)[0].strip()
                match = signature_re.match(header)
                if match:
                    name = match.group(1)
                    if name in control_keywords:
                        index += 1
                        continue

                    brace_balance = chunk.count('{') - chunk.count('}')
                    end = lookahead

                    while end + 1 < total and brace_balance > 0:
                        end += 1
                        brace_balance += lines[end].count('{') - lines[end].count('}')

                    functions.append((name, start + 1, end + 1, end - start + 1))
                    index = end + 1
                    continue

        index += 1

    return sorted(functions, key=lambda item: item[3], reverse=True)


def write_functions_report(file_path, functions):
    """Scrive un report markdown con l'analisi delle funzioni di un singolo file."""
    os.makedirs("./scripts", exist_ok=True)
    report_path = "./scripts/c_functions_report.md"

    content = f"# Analisi Funzioni\n\n"
    content += f"File analizzato: `{file_path}`\n\n"
    content += "| Funzione | Inizio | Fine | Righe |\n"
    content += "|----------|--------|------|-------|\n"

    for name, start, end, length in functions:
        content += f"| {name} | {start} | {end} | {length} |\n"

    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(content)

    return report_path

def main():
    parser = argparse.ArgumentParser(
        description="Genera report dei file C e, opzionalmente, analisi funzioni di un file specifico"
    )
    parser.add_argument(
        "file",
        nargs='?',
        help="Percorso file .c da analizzare per funzioni (opzionale)"
    )
    args = parser.parse_args()

    # Trova solo i file .c in main/ e components/
    c_files = glob.glob("main/**/*.c", recursive=True)
    c_files += glob.glob("components/**/*.c", recursive=True)
    
    # Calcola il numero di righe per ogni file
    file_stats = []
    for file_path in c_files:
        line_count = count_lines(file_path)
        file_stats.append((file_path, line_count))
    
    # Ordina i file per numero di righe in modo decrescente
    file_stats.sort(key=lambda x: x[1], reverse=True)
    
    # Crea il contenuto del report
    report_content = "# Report File .c\n\n"
    report_content += "| File | Righe |\n"
    report_content += "|------|-------|\n"
    
    for file_path, line_count in file_stats:
        report_content += f"| {file_path} | {line_count} |\n"
    
    # Scrivi il report nella cartella ./scripts
    os.makedirs("./scripts", exist_ok=True)
    report_path = "./scripts/c_files_report.md"
    
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(report_content)
    
    print(f"Report generato: {report_path}")

    if args.file:
        if not os.path.isfile(args.file):
            print(f"File non trovato: {args.file}")
            return

        if not args.file.endswith('.c'):
            print(f"Il file specificato non è un file .c: {args.file}")
            return

        functions = analyze_functions(args.file)
        functions_report = write_functions_report(args.file, functions)
        print(f"Report funzioni generato: {functions_report}")
        print(f"Funzioni trovate: {len(functions)}")

if __name__ == "__main__":
    main()