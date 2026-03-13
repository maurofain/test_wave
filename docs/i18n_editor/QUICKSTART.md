# QUICKSTART - i18n Editor Electron

## 1) Avvio immediato

### Linux/macOS

```bash
cd /path/to/test_wave/i18n_editor
chmod +x run.sh
./run.sh
```

### Windows

```bat
cd C:\path\to\test_wave\i18n_editor
run.bat
```

Gli script installano le dipendenze npm se mancanti e avviano Electron.

## 2) Uso base

1. Seleziona uno scope dal menu in alto.
2. Modifica le traduzioni nelle celle.
3. Cerca testo globale e naviga con `<` e `>`.
4. Premi `💾 Salva`.

Al salvataggio viene creato un backup `i18n_YYYYMMDD_HHMMSS.json` in `../data`.

## 3) Traduzione automatica

Configura `translator_config.json` e imposta:

```json
{
  "enabled": true,
  "service": "ollama"
}
```

oppure:

```json
{
  "enabled": true,
  "service": "openai"
}
```

Poi abilita la checkbox `Traduzione automatica` nell'app e usa il pulsante `🌐` nelle colonne non italiane.

## 4) Troubleshooting rapido

- `node: command not found`: installa Node.js 18+
- app non parte: esegui `npm install` manualmente in `i18n_editor`
- traduzione fallita: verifica endpoint/API key in `translator_config.json`
