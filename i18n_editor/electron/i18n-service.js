const fs = require("fs");
const path = require("path");

class I18nService {
  static LANGUAGE_ORDER = {
    it: 0,
    en: 1,
    de: 2,
    es: 3,
    fr: 4,
  };

  static LANGUAGE_NAMES = {
    it: "Italiano",
    en: "English",
    de: "Deutsch",
    es: "Español",
    fr: "Français",
  };

  constructor(dataDir, mapFilePath) {
    this.dataDir = dataDir;
    this.mapFilePath = mapFilePath;
    this.i18nData = {};
    this.languages = [];
    this.entriesByScope = new Map();
    this.scopes = [];
    this.scopeNameMap = new Map();
    this.keyNameMap = new Map();
    this.mapLoaded = false;
  }

  loadIdNameMap() {
    this.scopeNameMap.clear();
    this.keyNameMap.clear();
    this.mapLoaded = false;

    if (!fs.existsSync(this.mapFilePath)) {
      return false;
    }

    try {
      const raw = fs.readFileSync(this.mapFilePath, "utf-8");
      const parsed = JSON.parse(raw);

      const scopes = parsed.scopes ?? {};
      const keys = parsed.keys ?? {};

      for (const [scopeId, scopeName] of Object.entries(scopes)) {
        const parsedId = Number(scopeId);
        if (!Number.isNaN(parsedId)) {
          this.scopeNameMap.set(parsedId, String(scopeName));
        }
      }

      for (const [keyId, keyName] of Object.entries(keys)) {
        const parsedId = Number(keyId);
        if (!Number.isNaN(parsedId)) {
          this.keyNameMap.set(parsedId, String(keyName));
        }
      }

      this.mapLoaded = true;
      return true;
    } catch {
      return false;
    }
  }

  getScopeName(scopeId) {
    return this.scopeNameMap.get(scopeId) ?? "";
  }

  getKeyName(keyId) {
    return this.keyNameMap.get(keyId) ?? "";
  }

  getScopeLabel(scopeId) {
    const scopeName = this.getScopeName(scopeId);
    if (scopeName) {
      return `Scope ${scopeId} · ${scopeName}`;
    }
    return `Scope ${scopeId}`;
  }

  getLanguageName(langCode) {
    return I18nService.LANGUAGE_NAMES[langCode] ?? langCode.toUpperCase();
  }

  loadAllFiles() {
    this.i18nData = {};

    if (!fs.existsSync(this.dataDir)) {
      throw new Error(`Directory non trovata: ${this.dataDir}`);
    }

    const languageFiles = fs
      .readdirSync(this.dataDir)
      .filter((name) => /^i18n_[a-z]{2}\.json$/i.test(name))
      .sort((a, b) => a.localeCompare(b));

    for (const fileName of languageFiles) {
      const match = /^i18n_([a-z]{2})\.json$/i.exec(fileName);
      if (!match) {
        continue;
      }

      const langCode = match[1].toLowerCase();
      const fullPath = path.join(this.dataDir, fileName);

      try {
        const raw = fs.readFileSync(fullPath, "utf-8");
        const parsed = JSON.parse(raw);
        if (Array.isArray(parsed)) {
          this.i18nData[langCode] = parsed;
        }
      } catch (error) {
        console.error(`Errore nel caricamento ${fileName}:`, error.message);
      }
    }

    this.languages = Object.keys(this.i18nData).sort((langA, langB) => {
      const orderA = I18nService.LANGUAGE_ORDER[langA] ?? 999;
      const orderB = I18nService.LANGUAGE_ORDER[langB] ?? 999;
      if (orderA !== orderB) {
        return orderA - orderB;
      }
      return langA.localeCompare(langB);
    });

    if (this.languages.length === 0) {
      return false;
    }

    this.buildScopeIndex();
    return true;
  }

  buildScopeIndex() {
    this.scopes = this.extractUniqueScopesFromReference();
    this.entriesByScope = new Map();

    for (const scope of this.scopes) {
      const keyToEntry = new Map();

      for (const lang of this.languages) {
        const langEntries = this.i18nData[lang] ?? [];
        for (const entry of langEntries) {
          if (Number(entry.scope) !== Number(scope)) {
            continue;
          }

          if (entry.key === undefined || entry.key === null) {
            continue;
          }

          const key = Number(entry.key);
          const section = entry.section === undefined || entry.section === null ? 0 : Number(entry.section);
          const entryId = `${key}:${section}`;

          if (!keyToEntry.has(entryId)) {
            keyToEntry.set(entryId, {
              scope,
              key,
              section,
              keyName: this.getKeyName(key),
              translations: {},
            });
          }

          const text = typeof entry.text === "string" ? entry.text : String(entry.text ?? "");
          keyToEntry.get(entryId).translations[lang] = text;
        }
      }

      const sortedEntries = Array.from(keyToEntry.values()).sort((a, b) => {
        if (a.key !== b.key) {
          return a.key - b.key;
        }
        return a.section - b.section;
      });

      this.entriesByScope.set(scope, sortedEntries);
    }
  }

  extractUniqueScopesFromReference() {
    const referenceLang = this.i18nData.it ? "it" : this.languages[0];
    if (!referenceLang) {
      return [];
    }

    const uniqueScopes = new Set();
    const entries = this.i18nData[referenceLang] ?? [];

    for (const entry of entries) {
      if (entry.scope !== undefined && entry.scope !== null) {
        uniqueScopes.add(Number(entry.scope));
      }
    }

    return Array.from(uniqueScopes).sort((a, b) => a - b);
  }

  getScopeData(scope) {
    return this.entriesByScope.get(Number(scope)) ?? [];
  }

  updateTranslation(scope, key, section, lang, text) {
    const langEntries = this.i18nData[lang];
    if (!Array.isArray(langEntries)) {
      return false;
    }

    for (const entry of langEntries) {
      const entrySection = entry.section === undefined || entry.section === null ? 0 : Number(entry.section);
      if (
        Number(entry.scope) === Number(scope) &&
        Number(entry.key) === Number(key) &&
        entrySection === Number(section)
      ) {
        entry.text = text;

        const indexedEntries = this.entriesByScope.get(Number(scope)) ?? [];
        for (const indexedEntry of indexedEntries) {
          if (indexedEntry.key === Number(key) && indexedEntry.section === Number(section)) {
            indexedEntry.translations[lang] = text;
            break;
          }
        }

        return true;
      }
    }

    return false;
  }

  saveAllFiles() {
    const timestamp = this.getTimestamp();
    const backupFilename = `i18n_${timestamp}.json`;
    const backupPath = path.join(this.dataDir, backupFilename);

    try {
      const backupData = {
        timestamp,
        languages: this.languages,
        data: {},
      };

      for (const lang of this.languages) {
        backupData.data[lang] = this.i18nData[lang] ?? [];
      }

      fs.writeFileSync(backupPath, JSON.stringify(backupData, null, 2), "utf-8");

      for (const lang of this.languages) {
        const filePath = path.join(this.dataDir, `i18n_${lang}.json`);
        fs.writeFileSync(filePath, JSON.stringify(this.i18nData[lang] ?? []), "utf-8");
      }

      return {
        success: true,
        backupFile: backupFilename,
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
        for (const lang of this.languages) {
          const text = String(entry.translations[lang] ?? "");
          if (text.toLowerCase().includes(query)) {
            results.push({
              scope,
              key: entry.key,
              section: entry.section,
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
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, "0");
    const day = String(now.getDate()).padStart(2, "0");
    const hours = String(now.getHours()).padStart(2, "0");
    const minutes = String(now.getMinutes()).padStart(2, "0");
    const seconds = String(now.getSeconds()).padStart(2, "0");
    return `${year}${month}${day}_${hours}${minutes}${seconds}`;
  }
}

module.exports = {
  I18nService,
};
