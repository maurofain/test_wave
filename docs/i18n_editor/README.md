# i18n Editor - Electron

Applicazione desktop Electron per editare i file `i18n_xx.json` del progetto MicroHard.

## Funzionalità

- Caricamento completo dei file lingua da `../data`
- Scope popolati dalla lingua italiana (`i18n_it.json`)
- Decodifica nomi ID tramite `../docs/i18n/i18n_it.map.json`
- Tabella editabile con colonna per ogni lingua (Italiano sempre per primo)
- Ricerca globale con navigazione risultati usando `<` e `>`
- Pulsante traduzione su ogni campo non italiano
- Salvataggio con backup `i18n_YYYYMMDD_HHMMSS.json` e rigenerazione file lingua
- Tema chiaro/scuro

## Requisiti

- Node.js 18+
- npm 9+
- Windows, Linux o macOS

## Avvio rapido

### Linux/macOS

```bash
cd i18n_editor
./run.sh
```

### Windows

```bat
cd i18n_editor
run.bat
```

### Avvio manuale

```bash
cd i18n_editor
npm install
npm start
```

## Configurazione traduzione automatica

Il file usato è `translator_config.json`.

Esempio Ollama:

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

Esempio OpenAI:

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

## Struttura principale

```
i18n_editor/
├── electron/
│   ├── main.js
│   ├── preload.js
│   ├── i18n-service.js
│   └── translator-service.js
├── renderer/
│   ├── index.html
│   ├── renderer.js
│   └── styles.css
├── translator_config.json
├── package.json
├── run.sh
└── run.bat
```

## Note

- I file Python legacy sono stati archiviati in `i18n_editor/legacy_py`; l'avvio standard resta Electron.
- In caso di errori traduzione, verificare endpoint/configurazione del servizio impostato.
