#!/usr/bin/env python3
"""
Script per cercare tutti i file *.c nella cartella corrente,
determinare quante righe hanno, ordinarli in modo decrescente
e creare un report in formato .md nella cartella ./scripts
"""

import os
import glob

def count_lines(file_path):
    """Conta il numero di righe in un file"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return sum(1 for line in f)
    except Exception as e:
        print(f"Errore leggendo {file_path}: {e}")
        return 0

def main():
    # Trova tutti i file .c nella cartella corrente e nelle sottocartelle
    c_files = glob.glob("**/*.c", recursive=True)
    
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

if __name__ == "__main__":
    main()