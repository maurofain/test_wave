const fs = require("fs");

const DEFAULT_CONFIG = {
  enabled: false,
  service: "ollama",
  openai: {
    api_key: "",
    model: "gpt-3.5-turbo",
    temperature: 0.3,
  },
  google: {
    api_key: "",
    project_id: "",
  },
  deepl: {
    api_key: "",
    api_type: "free",
  },
  ollama: {
    base_url: "http://localhost:11434",
    model: "mistral",
  },
};

function deepMerge(base, override) {
  const output = { ...base };
  for (const [key, value] of Object.entries(override ?? {})) {
    if (
      value &&
      typeof value === "object" &&
      !Array.isArray(value) &&
      output[key] &&
      typeof output[key] === "object" &&
      !Array.isArray(output[key])
    ) {
      output[key] = deepMerge(output[key], value);
    } else {
      output[key] = value;
    }
  }
  return output;
}

class TranslatorConfigService {
  constructor(configFilePath) {
    this.configFilePath = configFilePath;
    this.config = deepMerge(DEFAULT_CONFIG, {});
    this.loadConfig();
  }

  loadConfig() {
    if (!fs.existsSync(this.configFilePath)) {
      this.saveConfig();
      return;
    }

    try {
      const raw = fs.readFileSync(this.configFilePath, "utf-8");
      const parsed = JSON.parse(raw);
      this.config = deepMerge(DEFAULT_CONFIG, parsed);
    } catch {
      this.config = deepMerge(DEFAULT_CONFIG, {});
    }
  }

  saveConfig() {
    fs.writeFileSync(this.configFilePath, JSON.stringify(this.config, null, 2), "utf-8");
  }

  getConfig() {
    return this.config;
  }

  isEnabled() {
    return Boolean(this.config.enabled);
  }

  setEnabled(enabled) {
    this.config.enabled = Boolean(enabled);
    this.saveConfig();
  }

  getService() {
    return String(this.config.service ?? "ollama").toLowerCase();
  }
}

class TranslatorService {
  constructor(configService) {
    this.configService = configService;
  }

  async translate({ text, sourceLang, targetLang }) {
    const sourceText = String(text ?? "").trim();
    if (!sourceText) {
      return "";
    }

    if (!this.configService.isEnabled()) {
      throw new Error("Traduzione automatica disabilitata in translator_config.json");
    }

    const service = this.configService.getService();
    if (service === "ollama") {
      return this.translateWithOllama(sourceText, sourceLang, targetLang);
    }
    if (service === "openai") {
      return this.translateWithOpenAI(sourceText, sourceLang, targetLang);
    }

    throw new Error(`Servizio di traduzione non supportato: ${service}`);
  }

  buildPrompt(text, sourceLang, targetLang) {
    const languageNames = {
      it: "Italian",
      en: "English",
      de: "German",
      es: "Spanish",
      fr: "French",
    };

    const sourceName = languageNames[sourceLang] ?? sourceLang;
    const targetName = languageNames[targetLang] ?? targetLang;

    return [
      `Traduci il seguente testo da ${sourceName} a ${targetName}.`,
      "Rispondi solo con la traduzione, senza note aggiuntive.",
      "",
      `Testo: ${text}`,
    ].join("\n");
  }

  async translateWithOllama(text, sourceLang, targetLang) {
    const config = this.configService.getConfig().ollama ?? {};
    const baseUrl = String(config.base_url ?? "http://localhost:11434").replace(/\/$/, "");
    const model = String(config.model ?? "mistral");
    const prompt = this.buildPrompt(text, sourceLang, targetLang);

    const response = await fetch(`${baseUrl}/api/generate`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        model,
        prompt,
        stream: false,
      }),
    });

    if (!response.ok) {
      throw new Error(`Ollama ha risposto con codice ${response.status}`);
    }

    const payload = await response.json();
    const translated = String(payload.response ?? "").trim();
    if (!translated) {
      throw new Error("Risposta Ollama vuota");
    }

    return translated;
  }

  async translateWithOpenAI(text, sourceLang, targetLang) {
    const config = this.configService.getConfig().openai ?? {};
    const apiKey = String(config.api_key ?? "").trim();
    const model = String(config.model ?? "gpt-3.5-turbo");
    const temperature = Number(config.temperature ?? 0.3);
    const prompt = this.buildPrompt(text, sourceLang, targetLang);

    if (!apiKey) {
      throw new Error("OpenAI API key non configurata in translator_config.json");
    }

    const response = await fetch("https://api.openai.com/v1/chat/completions", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${apiKey}`,
      },
      body: JSON.stringify({
        model,
        temperature,
        messages: [
          {
            role: "user",
            content: prompt,
          },
        ],
      }),
    });

    if (!response.ok) {
      const errorText = await response.text();
      throw new Error(`OpenAI errore ${response.status}: ${errorText}`);
    }

    const payload = await response.json();
    const translated = String(payload.choices?.[0]?.message?.content ?? "").trim();
    if (!translated) {
      throw new Error("Risposta OpenAI vuota");
    }

    return translated;
  }
}

module.exports = {
  TranslatorConfigService,
  TranslatorService,
};
