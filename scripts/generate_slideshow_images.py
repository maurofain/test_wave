#!/usr/bin/env python3
"""
Script per generare immagini pubblicitarie partendo dalle immagini in docs/images.
Le immagini vengono ritagliate e ridimensionate a 692x904 pixel (dimensioni pubblicitarie).
Proporzione consigliata: ≈ 3:4
"""

import os
import sys
from pathlib import Path
from PIL import Image, ImageOps

# Configurazione
SOURCE_DIR = Path("/home/mauro/1P/MicroHard/test_wave/docs/images")
TARGET_DIR = Path("/home/mauro/1P/MicroHard/test_wave/data")
TARGET_WIDTH = 692
TARGET_HEIGHT = 904
QUALITY = 85  # Qualità JPEG (0-100)

def fit_and_crop(image, target_width, target_height):
    """
    Ritaglia e ridimensiona un'immagine per riempire esattamente le dimensioni target.
    Mantiene le proporzioni e ritaglia l'eccesso.
    """
    # Calcola il rapporto di aspect
    img_ratio = image.width / image.height
    target_ratio = target_width / target_height
    
    if img_ratio > target_ratio:
        # L'immagine è più larga del target: ritaglia i lati
        new_height = target_height
        new_width = int(new_height * img_ratio)
        image = image.resize((new_width, new_height), Image.Resampling.LANCZOS)
        # Ritaglia al centro
        left = (new_width - target_width) // 2
        image = image.crop((left, 0, left + target_width, target_height))
    else:
        # L'immagine è più alta del target: ritaglia sopra e sotto
        new_width = target_width
        new_height = int(new_width / img_ratio)
        image = image.resize((new_width, new_height), Image.Resampling.LANCZOS)
        # Ritaglia al centro
        top = (new_height - target_height) // 2
        image = image.crop((0, top, target_width, top + target_height))
    
    return image

def process_image(source_path, target_path):
    """
    Processa una singola immagine.
    """
    try:
        with Image.open(source_path) as img:
            # Converti in RGB se necessario (per JPEG)
            if img.mode != 'RGB':
                img = img.convert('RGB')
            
            # Applica fit and crop
            processed_img = fit_and_crop(img, TARGET_WIDTH, TARGET_HEIGHT)
            
            # Salva l'immagine
            processed_img.save(target_path, 'JPEG', quality=QUALITY, optimize=True)
            
            print(f"✅ Processato: {source_path.name} -> {target_path.name}")
            print(f"   Dimensioni originali: {img.width}x{img.height}")
            print(f"   Dimensioni finali: {TARGET_WIDTH}x{TARGET_HEIGHT}")
            return True
            
    except Exception as e:
        print(f"❌ Errore processando {source_path.name}: {e}")
        return False

def main():
    """Funzione principale."""
    print("🖼️  Generatore immagini pubblicitarie")
    print(f"   Directory sorgente: {SOURCE_DIR}")
    print(f"   Directory target: {TARGET_DIR}")
    print(f"   Dimensioni target: {TARGET_WIDTH}x{TARGET_HEIGHT} (proporzione ≈ 3:4)")
    print()
    
    # Verifica che le directory esistano
    if not SOURCE_DIR.exists():
        print(f"❌ Errore: Directory sorgente non trovata: {SOURCE_DIR}")
        sys.exit(1)
    
    if not TARGET_DIR.exists():
        print(f"❌ Errore: Directory target non trovata: {TARGET_DIR}")
        sys.exit(1)
    
    # Trova tutte le immagini nella directory sorgente
    image_extensions = {'.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.webp'}
    source_files = []
    
    for ext in image_extensions:
        source_files.extend(SOURCE_DIR.glob(f"*{ext}"))
        source_files.extend(SOURCE_DIR.glob(f"*{ext.upper()}"))
    
    if not source_files:
        print(f"⚠️  Nessuna immagine trovata in {SOURCE_DIR}")
        return
    
    print(f"📁 Trovate {len(source_files)} immagini da processare:")
    for f in sorted(source_files):
        print(f"   - {f.name}")
    print()
    
    # Processa ogni immagine
    success_count = 0
    for source_file in sorted(source_files):
        # Genera il nome del file target (imgXX.jpg)
        # Estrae il numero dal nome del file (es: image01.jpg -> 01)
        stem = source_file.stem
        number = stem.replace('image', '').replace('img', '').zfill(2)
        target_name = f"img{number}.jpg"
        target_path = TARGET_DIR / target_name
        
        if process_image(source_file, target_path):
            success_count += 1
        print()
    
    print(f"✨ Completato! Processate {success_count}/{len(source_files)} immagini con successo.")
    
    # Mostra le dimensioni dei file generati
    print("\n📊 Dimensioni file generati:")
    for f in sorted(TARGET_DIR.glob("img*.jpg")):
        size_kb = f.stat().st_size / 1024
        print(f"   {f.name}: {size_kb:.1f} KB")

if __name__ == "__main__":
    main()
