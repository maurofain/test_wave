import requests
import re
import sys
import time
import os
import argparse

# Configurazione per il tuo SER9
OLLAMA_URL = "http://127.0.0.1:11434/api/generate"
MODEL = "qwen2.5-coder:7b"

def get_doxygen_comment(func_sig, index, total, filename):
    start_time = time.time()
    display_sig = func_sig.strip().split('\n')[0][:50]
    print(f"   [{index}/{total}] File: {filename} -> Generazione ITA per: {display_sig}...")
    
    prompt = f"""Agisci come un esperto programmatore C. 
Genera il commento Doxygen (/** ... */) in LINGUA ITALIANA per la firma fornita.
REGOLE TASSATIVE:
- Rispondi ESCLUSIVAMENTE in italiano.
- NON usare blocchi di codice Markdown (niente ```).
- Restituisci SOLO il testo del commento.
- Usa i tag: @brief, @param [in/out] e @return.

Firma:
{func_sig}
"""
    try:
        payload = {"model": MODEL, "prompt": prompt, "stream": False, "options": {"temperature": 0.1}}
        response = requests.post(OLLAMA_URL, json=payload, timeout=60)
        res_text = response.json().get('response', '').strip()
        # Pulizia backticks
        return re.sub(r'```[a-z]*\n?', '', res_text).replace('```', '').strip()
    except Exception as e:
        return f"/** Errore Ollama: {e} */"

def has_doxygen_already(content, match_start):
    """
    Controlla se nelle righe immediatamente precedenti all'inizio della 
    funzione esiste già la chiusura di un commento Doxygen */
    """
    # Analizziamo i 500 caratteri precedenti per sicurezza
    lookback_area = content[max(0, match_start-500):match_start].strip()
    # Se l'area termina con */ significa che c'è già un commento (probabilmente Doxygen)
    return lookback_area.endswith("*/")

def process_single_file(filepath, analyze_only=False):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Regex colonna zero (esclude if/while indentati)
    func_pattern = re.compile(r'^(?:static\s+|inline\s+)?[\w\*]+\s+\w+\s*\([^;]*\)\s*\{', re.MULTILINE)
    matches = list(func_pattern.finditer(content))

    if not matches:
        return 0, 0

    processed_count = 0
    skipped_count = 0
    new_content = ""
    last_pos = 0
    fname = os.path.basename(filepath)

    # In modalità analisi, facciamo solo un report veloce
    if analyze_only:
        print(f"\n[FILE] {filepath}")
        for i, m in enumerate(matches, 1):
            sig = m.group(0).replace('{', '').strip()
            line_num = content.count('\n', 0, m.start()) + 1
            if has_doxygen_already(content, m.start()):
                print(f"  {i:2}. [Riga {line_num:4}] [SALTATA - Già commentata] -> {sig}")
                skipped_count += 1
            else:
                print(f"  {i:2}. [Riga {line_num:4}] [DA ELABORARE] -> {sig}")
                processed_count += 1
        return processed_count, skipped_count

    # Elaborazione reale con Ollama
    for match in matches:
        # 1. Copia il codice dall'ultima posizione fino all'inizio di questa funzione
        new_content += content[last_pos:match.start()]
        func_signature = match.group(0).rstrip('{')

        # 2. Controllo se esiste già il commento
        if has_doxygen_already(content, match.start()):
            new_content += match.group(0) # Copia la funzione così com'è
            skipped_count += 1
        else:
            processed_count += 1
            comment = get_doxygen_comment(func_signature, processed_count, len(matches), fname)
            new_content += f"\n{comment}\n{func_signature}{{"
        
        last_pos = match.end()
    
    new_content += content[last_pos:]
    
    # Salvataggio se ci sono state modifiche
    if processed_count > 0:
        bak_path = filepath + ".bak"
        if os.path.exists(bak_path): os.remove(bak_path)
        os.rename(filepath, bak_path)
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
    
    return processed_count, skipped_count

def main():
    parser = argparse.ArgumentParser(description="Doxygen Generator Intelligente ITA")
    parser.add_argument("path", help="File .c o cartella")
    parser.add_argument("-a", "--analyze", action="store_true", help="Analizza funzioni e rileva commenti esistenti")
    args = parser.parse_args()

    target_path = os.path.abspath(args.path)
    files_to_process = []

    if os.path.isdir(target_path):
        for root, _, files in os.walk(target_path):
            for file in files:
                if file.endswith(".c"):
                    files_to_process.append(os.path.join(root, file))
    elif os.path.isfile(target_path) and target_path.endswith(".c"):
        files_to_process.append(target_path)

    if not files_to_process:
        print("Nessun file .c trovato.")
        return

    print(f"Trovati {len(files_to_process)} file. Inizio scansione...\n")
    
    total_p = 0
    total_s = 0
    for f_path in files_to_process:
        p, s = process_single_file(f_path, analyze_only=args.analyze)
        total_p += p
        total_s += s

    print(f"\n--- SINTESI FINALE ---")
    print(f"File analizzati:     {len(files_to_process)}")
    print(f"Funzioni commentate: {total_p}")
    print(f"Funzioni saltate:    {total_s} (già presenti)")

if __name__ == "__main__":
    main()