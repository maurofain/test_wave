const path = require("path");
const { app, BrowserWindow, ipcMain } = require("electron");
const { I18nService } = require("./i18n-service");
const { TranslatorConfigService, TranslatorService } = require("./translator-service");

const editorRoot = path.resolve(__dirname, "..");
const projectRoot = path.resolve(editorRoot, "..");

const dataDir = path.join(projectRoot, "data");
const mapFilePath = path.join(projectRoot, "docs", "i18n", "i18n_it.map.json");
const translatorConfigPath = path.join(editorRoot, "translator_config.json");

const i18nService = new I18nService(dataDir, mapFilePath);
const translatorConfigService = new TranslatorConfigService(translatorConfigPath);
const translatorService = new TranslatorService(translatorConfigService);

let mainWindow;

function ensureDataLoaded() {
  if (i18nService.languages.length > 0) {
    return;
  }

  i18nService.loadIdNameMap();
  const loaded = i18nService.loadAllFiles();
  if (!loaded) {
    throw new Error("Nessun file i18n_xx.json trovato nella cartella data");
  }
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1600,
    height: 950,
    minWidth: 1200,
    minHeight: 700,
    show: false,
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
  });

  mainWindow.loadFile(path.join(editorRoot, "renderer", "index.html"));
}

ipcMain.handle("editor:init", async () => {
  ensureDataLoaded();
  return {
    languages: i18nService.languages,
    languageLabels: i18nService.languages.map((lang) => ({
      code: lang,
      name: i18nService.getLanguageName(lang),
    })),
    scopes: i18nService.scopes.map((scopeId) => ({
      id: scopeId,
      label: i18nService.getScopeLabel(scopeId),
    })),
    currentScope: i18nService.scopes[0] ?? null,
    translator: {
      enabled: translatorConfigService.isEnabled(),
      service: translatorConfigService.getService(),
    },
  };
});

ipcMain.handle("editor:get-scope-data", async (_event, scope) => {
  ensureDataLoaded();
  return {
    scope: Number(scope),
    entries: i18nService.getScopeData(Number(scope)),
  };
});

ipcMain.handle("editor:update-translation", async (_event, payload) => {
  ensureDataLoaded();

  const ok = i18nService.updateTranslation(
    Number(payload.scope),
    Number(payload.key),
    Number(payload.section ?? 0),
    String(payload.lang),
    String(payload.text ?? "")
  );

  if (!ok) {
    throw new Error("Aggiornamento traduzione non riuscito");
  }

  return { ok: true };
});

ipcMain.handle("editor:search", async (_event, searchText) => {
  ensureDataLoaded();
  return {
    results: i18nService.searchAllScopes(searchText),
  };
});

ipcMain.handle("editor:save", async () => {
  ensureDataLoaded();
  return i18nService.saveAllFiles();
});

ipcMain.handle("editor:set-translator-enabled", async (_event, enabled) => {
  translatorConfigService.setEnabled(Boolean(enabled));
  return {
    enabled: translatorConfigService.isEnabled(),
    service: translatorConfigService.getService(),
  };
});

ipcMain.handle("editor:translate", async (_event, payload) => {
  const translated = await translatorService.translate({
    text: String(payload.text ?? ""),
    sourceLang: String(payload.sourceLang ?? "it"),
    targetLang: String(payload.targetLang ?? "en"),
  });

  return {
    text: translated,
  };
});

app.whenReady().then(() => {
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});
