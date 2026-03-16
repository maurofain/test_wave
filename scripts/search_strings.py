import re
import os
import csv

# --- CONFIGURAZIONE ---
TARGET_EXTENSIONS = ('.c', '.h', '.js', '.html')
OUTPUT_FILE = "stringhe_da_tradurre.csv"

STR_REGEX = re.compile(r'(["\'])(?:(?=(\\?))\2.)*?\1')
HTML_TEXT_REGEX = re.compile(r'>([^<>{}\n\t]+)<')
PLACEHOLDER_REGEX = re.compile(r'^\{\{\w+\}\}$')

# Regex per identificare costanti C (es: CMD_START_TEST, PIN_32, ERROR_1)
CONSTANT_REGEX = re.compile(r'^[A-Z0-9_]+$')

EXCLUDE_PATTERNS = [
    re.compile(r'.*\.(h|c|js|html|png|jpg|gif|css|json|bin|ld)$', re.IGNORECASE),
    re.compile(r'^[a-zA-Z0-9_\-\.]+$'),
    re.compile(r'^# [a-fA-F0-9]{3,6}$'),
    re.compile(r'^[/%].*'),
]

LOG_MACROS = ('LOGI', 'LOGW', 'LOGE', 'LOGV', 'LOGD', 'printf', 'console.log', 'console.error')

def strip_comments(text, extension):
    if extension in ('.c', '.h', '.js'):
        text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.DOTALL)
        text = re.sub(r'//.*', '', text)
    if extension == '.html':
        text = re.sub(r'', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.DOTALL)
    return text

def is_functional_string(s):
    s = s.strip()
    if not s or len(s) < 2:
        return True
    # Esclude placeholder tipo {{1}}
    if PLACEHOLDER_REGEX.match(s):
        return True
    # Esclude costanti TUTTO_MAIUSCOLO
    if CONSTANT_REGEX.match(s):
        return True
    # Esclude file, percorsi e stringhe tecniche senza spazi
    for ptrn in EXCLUDE_PATTERNS:
        if ptrn.match(s):
            return True
    return False

def extract_findings(file_path):
    findings = []
    ext = os.path.splitext(file_path)[1].lower()
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            raw_content = f.read()
        clean_content = strip_comments(raw_content, ext)
        lines = clean_content.splitlines()
        for line_num, line in enumerate(lines, 1):
            clean_line = line.strip()
            if not clean_line or any(m in clean_line for m in LOG_MACROS):
                continue
            for match in STR_REGEX.finditer(line):
                content = match.group(0).strip('"\'').strip()
                if not is_functional_string(content):
                    findings.append([line_num, content])
            if ext == '.html':
                for match in HTML_TEXT_REGEX.finditer(line):
                    content = match.group(1).strip()
                    if not is_functional_string(content):
                        findings.append([line_num, content])
    except Exception as e:
        print(f"Errore su {file_path}: {e}")
    return findings

def main():
    search_path = input("Percorso cartella [default .]: ").strip() or "."
    if not os.path.isdir(search_path):
        print("Percorso non valido.")
        return
    all_results = []
    for root, _, files in os.walk(search_path):
        if any(d in root for d in ('build', '.git', 'managed_components')):
            continue
        for file in files:
            if file.endswith(TARGET_EXTENSIONS):
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, search_path)
                res = extract_findings(full_path)
                for r in res:
                    all_results.append([rel_path, r[0], r[1]])
    with open(OUTPUT_FILE, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f, delimiter=';')
        writer.writerow(['File', 'Riga', 'Stringa'])
        writer.writerows(all_results)
    print(f"\n✅ Completato! Trovate {len(all_results)} stringhe in '{OUTPUT_FILE}'.")

if __name__ == "__main__":
    main()