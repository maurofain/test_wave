# INSTALLAZIONE DETTAGLIATA - i18n Editor Electron

## Requisiti

- Node.js 18 o superiore
- npm 9 o superiore
- Windows, Linux o macOS

Verifica installazione:

```bash
node --version
npm --version
```

## Installazione

### 1. Apri la cartella del progetto

```bash
cd /path/to/MicroHard/test_wave/i18n_editor
```

### 2. Installa dipendenze

```bash
npm install
```

### 3. Avvia l'app

```bash
npm start
```

In alternativa usa gli script:

- Linux/macOS: `./run.sh`
- Windows: `run.bat`

## Configurazione traduzione

La traduzione automatica usa `translator_config.json`.

### Ollama locale

```json
{
  "enabled": true,
  "service": "ollama",
  "ollama": {
    "base_url": "http://localhost:11434",
    "model": "mistral"
  }
}
```

### OpenAI

```json
{
  "enabled": true,
  "service": "openai",
  "openai": {
    "api_key": "sk-...",
    "model": "gpt-3.5-turbo",
    "temperature": 0.3
  }
}
```

## Risoluzione problemi

### `node: command not found`

Installa Node.js da https://nodejs.org

### Errore su `npm install`

Cancella `node_modules` e riprova:

```bash
rm -rf node_modules
npm install
```

### L'app non parte

Avvia da terminale per leggere l'errore:

```bash
npm start
```

### Traduzione non funziona

- verifica `enabled: true`
- verifica endpoint/API key del servizio configurato
- per Ollama, controlla che il server sia attivo
