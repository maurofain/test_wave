const fs = require("fs");
const path = require("path");

class I18nService {
  static LANGUAGE_NAMES = {
    it: "Italiano",
    en: "Inglese",
    fr: "Francese",
    de: "Tedesco",
    es: "Spagnolo",
  };

  static LANGUAGE_ORDER = {
    it: 0,
    en: 1,
    fr: 2,
    de: 3,
    es: 4,
  };

  constructor(dataDir) {
    this.dataDir = dataDir;
    this.catalogPath = path.join(this.dataDir, "i18n_v2.json");
    this.catalog = null;
    this.languages = [];
    this.scopes = [];
    this.entriesByScope = new Map();
  }

  reload() {
    this.catalog = null;
    this.languages = [];
    this.scopes = [];
    this.entriesByScope = new Map();
    this.loadAllFiles();
  }

  loadIdNameMap() {
    return true;
  }

  getScopeLabel(scope) {
    return `Pagina ${scope}`;
  }

  getLanguageName(langCode) {
    return I18nService.LANGUAGE_NAMES[langCode] ?? langCode.toUpperCase();
  }

  ensureCatalog() {
    if (this.catalog) {
      return;
    }
    this.catalog = {
      version: 2,
      base_language: "it",
      languages: ["it", "en", "fr", "de", "es"],
      web: {},
      lvgl: {},
    };
  }

  loadAllFiles() {
    this.ensureCatalog();
    if (!fs.existsSync(this.dataDir)) {
      throw new Error(`Directory non trovata: ${this.dataDir}`);
    }

    if (fs.existsSync(this.catalogPath)) {
      try {
        const raw = fs.readFileSync(this.catalogPath, "utf-8");
        const parsed = JSON.parse(raw);
        if (parsed && typeof parsed === "object") {
          this.catalog = parsed;
        }
      } catch (error) {
        throw new Error(`Errore lettura catalogo v2: ${error.message}`);
      }
    }

    if (!this.catalog.web || typeof this.catalog.web !== "object") {
      this.catalog.web = {};
    }
    if (!this.catalog.lvgl || typeof this.catalog.lvgl !== "object") {
      this.catalog.lvgl = {};
    }
    if (!Array.isArray(this.catalog.languages) || this.catalog.languages.length === 0) {
      this.catalog.languages = ["it", "en", "fr", "de", "es"];
    }

    this.languages = [...new Set(this.catalog.languages.map((x) => String(x).toLowerCase()))].sort((a, b) => {
      const orderA = I18nService.LANGUAGE_ORDER[a] ?? 999;
      const orderB = I18nService.LANGUAGE_ORDER[b] ?? 999;
      if (orderA !== orderB) {
        return orderA - orderB;
      }
      return a.localeCompare(b);
    });
    if (!this.languages.includes("it")) {
      this.languages.unshift("it");
    }

    this.buildScopeIndex();
    return true;
  }

  buildScopeIndex() {
    const web = this.catalog?.web ?? {};
    this.scopes = Object.keys(web).sort((a, b) => a.localeCompare(b));
    if (this.scopes.length === 0) {
      this.scopes = this.discoverWebScopes();
      for (const scope of this.scopes) {
        if (!web[scope]) {
          web[scope] = {};
        }
      }
    }
    this.entriesByScope = new Map();

    // Build entries for web scopes (numeric 001..999 keys)
    for (const scope of this.scopes) {
      const pageEntries = web[scope];
      const rows = [];
      for (const [keyCode, entry] of Object.entries(pageEntries ?? {})) {
        if (!entry || typeof entry !== "object") {
          continue;
        }
        const keyNum = Number(keyCode);
        const textObj = entry.text && typeof entry.text === "object" ? entry.text : {};
        const translations = {};
        for (const lang of this.languages) {
          translations[lang] = String(textObj[lang] ?? "");
        }

        rows.push({
          scope,
          key: Number.isNaN(keyNum) ? keyCode : keyNum,
          keyCode: String(keyCode).padStart(3, "0"),
          section: 0,
          keyName: String(entry.label ?? ""),
          translations,
        });
      }

      rows.sort((a, b) => Number(a.keyCode) - Number(b.keyCode));
      this.entriesByScope.set(scope, rows);
    }

    // Build entries for lvgl (alphanumeric keys)
    const lvglEntries = this.catalog?.lvgl ?? {};
    const lvglKeys = Object.keys(lvglEntries ?? {});
    if (lvglKeys.length > 0) {
      if (!this.scopes.includes("lvgl")) {
        this.scopes.push("lvgl");
      }
      const rows = [];
      for (const [keyName, entry] of Object.entries(lvglEntries ?? {})) {
        if (!entry || typeof entry !== "object") {
          continue;
        }
        const textObj = entry.text && typeof entry.text === "object" ? entry.text : entry;
        const translations = {};
        for (const lang of this.languages) {
          translations[lang] = String(textObj[lang] ?? "");
        }

        rows.push({
          scope: "lvgl",
          key: keyName,
          keyCode: String(keyName),
          section: 0,
          keyName: String(entry.label ?? keyName),
          translations,
        });
      }

      rows.sort((a, b) => a.keyCode.localeCompare(b.keyCode));
      this.entriesByScope.set("lvgl", rows);
    }
  }

  discoverWebScopes() {
    try {
      const wwwPath = path.join(this.dataDir, "www");
      if (!fs.existsSync(wwwPath)) {
        return ["index"];
      }
      const scopes = fs
        .readdirSync(wwwPath)
        .filter((name) => /\.(html?|HTML?)$/.test(name))
        .map((name) => name.replace(/\.[^.]+$/, ""))
        .sort((a, b) => a.localeCompare(b));
      return scopes.length ? scopes : ["index"];
    } catch {
      return ["index"];
    }
  }

  getScopeData(scope) {
    return this.entriesByScope.get(String(scope)) ?? [];
  }

  updateTranslation(scope, key, _section, lang, text) {
    const s = String(scope);
    // Special handling for lvgl keys (alphanumeric keys at top-level)
    if (s === "lvgl") {
      const k = String(key);
      if (!this.catalog?.lvgl?.[k]) {
        return false;
      }
      const entry = this.catalog.lvgl[k];
      // support both formats: either entry.text = { it:..., en:... } or entry = { it:..., en:... }
      if (entry.text && typeof entry.text === "object") {
        entry.text[String(lang)] = String(text ?? "");
      } else if (typeof entry === "object") {
        entry[String(lang)] = String(text ?? "");
      } else {
        // unexpected format
        this.catalog.lvgl[k] = { text: { [String(lang)]: String(text ?? "") } };
      }
      this.buildScopeIndex();
      return true;
    }

    // Default: web scopes with numeric 3-digit keys
    const k = String(Number(key)).padStart(3, "0");
    if (!this.catalog?.web?.[s]?.[k]) {
      return false;
    }
    const entry = this.catalog.web[s][k];
    if (!entry.text || typeof entry.text !== "object") {
      entry.text = {};
    }
    entry.text[String(lang)] = String(text ?? "");
    this.buildScopeIndex();
    return true;
  }

  addKey(scope, label, italianText) {
    const s = String(scope ?? "").trim();
    if (!s) {
      throw new Error("Scope non valido");
    }
    if (!/^[a-z0-9]+(\.[a-z0-9_]+){2,}$/.test(String(label ?? ""))) {
      throw new Error("Label non valida (formato richiesto: page.section.name)");
    }
    const it = String(italianText ?? "").trim();
    if (!it) {
      throw new Error("Il testo italiano è obbligatorio");
    }

    if (!this.catalog.web[s] || typeof this.catalog.web[s] !== "object") {
      this.catalog.web[s] = {};
    }
    const page = this.catalog.web[s];

    for (let i = 1; i <= 999; i++) {
      const code = String(i).padStart(3, "0");
      if (!page[code]) {
        page[code] = {
          label: String(label),
          text: { it },
        };
        this.buildScopeIndex();
        return { scope: s, keyCode: code, key: i };
      }
    }
    throw new Error(`Scope ${s} pieno: nessuna key libera tra 001..999`);
  }

  saveAllFiles(createBackup = false) {
    const timestamp = this.getTimestamp();
    const backupFilename = `i18n_${timestamp}.json`;

    // Save backup in ../docs/i18n relative to dataDir when requested
    const backupDir = path.join(this.dataDir, "..", "docs", "i18n");
    const backupPath = path.join(backupDir, backupFilename);

    try {
      this.catalog.languages = this.languages;

      if (createBackup) {
        try {
          if (!fs.existsSync(backupDir)) {
            fs.mkdirSync(backupDir, { recursive: true });
          }
          // write backup (previous version) to docs/i18n
          fs.writeFileSync(backupPath, JSON.stringify(this.catalog, null, 2), "utf-8");
        } catch (e) {
          // ignore backup errors but continue to save current catalog
          console.error('Unable to write backup to docs/i18n:', e.message);
        }
      }

      // write current catalog to original location
      fs.writeFileSync(this.catalogPath, JSON.stringify(this.catalog, null, 2), "utf-8");

      return {
        success: true,
        backupFile: createBackup ? backupPath : null,
      };
    } catch (error) {
      return {
        success: false,
        error: `Errore nel salvataggio: ${error.message}`,
      };
    }
  }

  searchAllScopes(searchText) {
    const query = String(searchText ?? "").trim().toLowerCase();
    if (!query) {
      return [];
    }

    const results = [];
    for (const scope of this.scopes) {
      const entries = this.getScopeData(scope);
      for (const entry of entries) {
        const label = String(entry.keyName ?? "");
        if (label.toLowerCase().includes(query)) {
          results.push({
            scope,
            key: entry.key,
            section: 0,
            lang: "label",
            text: label,
          });
        }
        for (const lang of this.languages) {
          const text = String(entry.translations?.[lang] ?? "");
          if (text.toLowerCase().includes(query)) {
            results.push({
              scope,
              key: entry.key,
              section: 0,
              lang,
              text,
            });
          }
        }
      }
    }
    return results;
  }

  getTimestamp() {
    const now = new Date();
    const pad = (value) => String(value).padStart(2, "0");
    return `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }
}

module.exports = { I18nService };
