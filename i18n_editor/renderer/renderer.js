const state = {
  languages: [],
  languageLabels: new Map(),
  scopes: [],
  currentScope: null,
  searchResults: [],
  searchIndex: 0,
  hasChanges: false,
  translatorEnabled: false,
  translateAllRunning: false,
  currentEntries: [],
  fieldMap: new Map(),
  highlightedField: null,
};

const dom = {};

window.addEventListener("DOMContentLoaded", async () => {
  cacheDom();
  bindEvents();
  restoreTheme();

  try {
    await initialize();
  } catch (error) {
    showToast(`Errore inizializzazione: ${error.message}`, "error", 5000);
  }
});

function cacheDom() {
  dom.scopeSelect = document.getElementById("scope-select");
  dom.autoTranslateToggle = document.getElementById("auto-translate-toggle");
  dom.themeToggle = document.getElementById("theme-toggle");
  dom.scopeMeta = document.getElementById("scope-meta");
  dom.searchInput = document.getElementById("search-input");
  dom.searchBtn = document.getElementById("search-btn");
  dom.prevBtn = document.getElementById("prev-btn");
  dom.nextBtn = document.getElementById("next-btn");
  dom.searchStatus = document.getElementById("search-status");
  dom.saveBtn = document.getElementById("save-btn");
  dom.addKeyBtn = document.getElementById("add-key-btn");
  dom.translateAllBtn = document.getElementById("translate-all-btn");
  dom.newKeyLabel = document.getElementById("new-key-label");
  dom.newKeyItText = document.getElementById("new-key-it-text");
  dom.rowsInfo = document.getElementById("rows-info");
  dom.tableHeadRow = document.getElementById("table-head-row");
  dom.tableBody = document.getElementById("table-body");
  dom.footerInfo = document.getElementById("footer-info");
  dom.toast = document.getElementById("toast");
}

function bindEvents() {
  dom.scopeSelect.addEventListener("change", async (event) => {
    const nextScope = String(event.target.value ?? "").trim();
    if (nextScope) {
      await loadScope(nextScope);
    }
  });

  dom.searchBtn.addEventListener("click", () => runSearch());
  dom.searchInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      runSearch();
    }
  });

  dom.prevBtn.addEventListener("click", () => goToPreviousMatch());
  dom.nextBtn.addEventListener("click", () => goToNextMatch());

  dom.saveBtn.addEventListener("click", async () => {
    const result = await window.editorApi.save();
    if (result.success) {
      setHasChanges(false);
      showToast(`Salvataggio completato. Backup: ${result.backupFile}`, "success");
      return;
    }
    showToast(result.error ?? "Errore durante il salvataggio", "error", 5000);
  });

  dom.addKeyBtn.addEventListener("click", async () => {
    const scope = String(state.currentScope ?? "").trim();
    const label = String(dom.newKeyLabel.value ?? "").trim();
    const italianText = String(dom.newKeyItText.value ?? "").trim();
    if (!scope) {
      showToast("Scope non selezionato", "error");
      return;
    }
    if (!label) {
      showToast("Inserisci una label", "error");
      return;
    }
    if (!italianText) {
      showToast("Inserisci il testo italiano", "error");
      return;
    }
    try {
      const created = await window.editorApi.addKey({ scope, label, italianText });
      await loadScope(scope);
      dom.newKeyLabel.value = "";
      dom.newKeyItText.value = "";
      setHasChanges(true);
      showToast(`Nuova key creata: ${created.keyCode}`, "success");
    } catch (error) {
      showToast(`Creazione key fallita: ${error.message}`, "error", 5000);
    }
  });

  dom.translateAllBtn.addEventListener("click", async () => {
    await translateAllFieldsInCurrentScope();
  });

  dom.autoTranslateToggle.addEventListener("change", async (event) => {
    const status = await window.editorApi.setTranslatorEnabled(event.target.checked);
    state.translatorEnabled = Boolean(status.enabled);
    refreshTranslateButtons();
    showToast(
      state.translatorEnabled ? "Traduzione automatica attivata" : "Traduzione automatica disattivata",
      "success"
    );
  });

  dom.themeToggle.addEventListener("change", (event) => {
    applyTheme(event.target.checked ? "dark" : "light");
  });

  window.addEventListener("beforeunload", (event) => {
    if (!state.hasChanges) {
      return;
    }
    event.preventDefault();
    event.returnValue = "";
  });
}

async function initialize() {
  const payload = await window.editorApi.init();

  state.languages = payload.languages ?? [];
  state.languageLabels = new Map((payload.languageLabels ?? []).map((item) => [item.code, item.name]));
  state.scopes = payload.scopes ?? [];
  state.currentScope = payload.currentScope;
  state.translatorEnabled = Boolean(payload.translator?.enabled);

  dom.autoTranslateToggle.checked = state.translatorEnabled;

  renderScopeOptions();
  renderTableHeader();
  updateFooter();

  if (state.currentScope !== null && state.currentScope !== undefined) {
    await loadScope(state.currentScope);
  }
}

function renderScopeOptions() {
  dom.scopeSelect.innerHTML = "";
  const optionsFragment = document.createDocumentFragment();

  for (const scopeInfo of state.scopes) {
    const option = document.createElement("option");
    option.value = String(scopeInfo.id);
    option.textContent = scopeInfo.label;
    optionsFragment.appendChild(option);
  }

  dom.scopeSelect.appendChild(optionsFragment);
  if (state.currentScope !== null) {
    dom.scopeSelect.value = String(state.currentScope);
  }
}

function renderTableHeader() {
  dom.tableHeadRow.innerHTML = "";

  const staticColumns = ["ID", "Nome ID"];
  for (const columnTitle of staticColumns) {
    const th = document.createElement("th");
    th.textContent = columnTitle;
    dom.tableHeadRow.appendChild(th);
  }

  for (const lang of state.languages) {
    const th = document.createElement("th");
    const langName = state.languageLabels.get(lang) ?? lang.toUpperCase();
    th.textContent = `${langName} (${lang})`;
    dom.tableHeadRow.appendChild(th);
  }
}

async function loadScope(scopeId) {
  const payload = await window.editorApi.getScopeData(scopeId);
  state.currentScope = String(scopeId);
  dom.scopeSelect.value = String(state.currentScope);
  state.currentEntries = payload.entries ?? [];

  renderTableRows(state.currentEntries);
  updateScopeMeta(state.currentEntries);
  clearFieldHighlight();
  refreshTranslateButtons();
}

function renderTableRows(entries) {
  state.fieldMap.clear();
  dom.tableBody.innerHTML = "";

  if (!entries.length) {
    const emptyRow = document.createElement("tr");
    const emptyCell = document.createElement("td");
    emptyCell.colSpan = 2 + state.languages.length;
    emptyCell.textContent = "Nessun dato disponibile per questo scope";
    emptyCell.className = "empty-cell";
    emptyRow.appendChild(emptyCell);
    dom.tableBody.appendChild(emptyRow);
    dom.rowsInfo.textContent = "Righe caricate: 0";
    return;
  }

  const fragment = document.createDocumentFragment();

  entries.forEach((entry, index) => {
    const tr = document.createElement("tr");
    tr.className = index % 2 === 0 ? "row-even" : "row-odd";

    const keyCell = document.createElement("td");
    keyCell.className = "meta-cell";
    keyCell.textContent = String(entry.key);
    tr.appendChild(keyCell);

    const nameCell = document.createElement("td");
    nameCell.className = "meta-cell";
    const section = Number(entry.section ?? 0);
    const nameText = entry.keyName && entry.keyName.trim() ? entry.keyName.trim() : "-";
    nameCell.textContent = section > 0 ? `${nameText} [s${section}]` : nameText;
    tr.appendChild(nameCell);

    for (const lang of state.languages) {
      const td = document.createElement("td");
      td.className = "edit-cell";

      const editorWrap = document.createElement("div");
      editorWrap.className = "cell-editor";

      const textarea = document.createElement("textarea");
      textarea.value = String(entry.translations?.[lang] ?? "");
      textarea.rows = 2;
      textarea.dataset.scope = String(state.currentScope);
      textarea.dataset.key = String(entry.key);
      textarea.dataset.section = String(section);
      textarea.dataset.lang = lang;

      if (lang === "it") {
        textarea.classList.add("master-lang");
      }

      textarea.addEventListener("input", () => {
        onTextChanged({
          scope: state.currentScope,
          key: Number(entry.key),
          section,
          lang,
          text: textarea.value,
        });
      });

      const fieldKey = makeFieldKey(state.currentScope, entry.key, section, lang);
      state.fieldMap.set(fieldKey, textarea);

      editorWrap.appendChild(textarea);

      if (lang !== "it") {
        const translateBtn = document.createElement("button");
        translateBtn.type = "button";
        translateBtn.className = "translate-btn";
        translateBtn.textContent = "🌐";
        translateBtn.title = `Traduci da Italiano a ${lang}`;
        translateBtn.disabled = !state.translatorEnabled;
        translateBtn.dataset.lang = lang;
        translateBtn.dataset.key = String(entry.key);
        translateBtn.dataset.section = String(section);

        translateBtn.addEventListener("click", async () => {
          await translateField(entry.key, section, lang, textarea, translateBtn);
        });

        editorWrap.appendChild(translateBtn);
      }

      td.appendChild(editorWrap);
      tr.appendChild(td);
    }

    fragment.appendChild(tr);
  });

  dom.tableBody.appendChild(fragment);
  dom.rowsInfo.textContent = `Righe caricate: ${entries.length}`;
}

function updateScopeMeta(entries) {
  const scopeInfo = state.scopes.find((scope) => String(scope.id) === String(state.currentScope));
  const scopeLabel = scopeInfo ? scopeInfo.label : `Scope ${state.currentScope}`;
  dom.scopeMeta.textContent = `${scopeLabel} · campi: ${entries.length}`;
}

function updateFooter() {
  dom.footerInfo.textContent = `Lingue caricate: ${state.languages.join(", ")} · Scope caricate: ${state.scopes.length}`;
}

function onTextChanged(payload) {
  window.editorApi.updateTranslation(payload).catch((error) => {
    showToast(`Errore update: ${error.message}`, "error", 5000);
  });
  setHasChanges(true);
}

async function runSearch() {
  const query = dom.searchInput.value.trim();
  if (!query) {
    showToast("Inserisci un testo da cercare", "error");
    return;
  }

  const payload = await window.editorApi.search(query);
  state.searchResults = payload.results ?? [];
  state.searchIndex = 0;

  if (!state.searchResults.length) {
    dom.searchStatus.textContent = "Nessun risultato";
    dom.prevBtn.disabled = true;
    dom.nextBtn.disabled = true;
    showToast(`Nessun risultato per: ${query}`, "error");
    return;
  }

  dom.prevBtn.disabled = false;
  dom.nextBtn.disabled = false;

  await showSearchResult(0);
}

async function showSearchResult(index) {
  if (!state.searchResults.length) {
    return;
  }

  const normalized = ((index % state.searchResults.length) + state.searchResults.length) % state.searchResults.length;
  const result = state.searchResults[normalized];

  if (String(result.scope) !== String(state.currentScope)) {
    await loadScope(String(result.scope));
  }

  const fieldKey = makeFieldKey(result.scope, result.key, result.section, result.lang);
  const field = state.fieldMap.get(fieldKey);

  if (field) {
    clearFieldHighlight();
    field.classList.add("search-hit");
    state.highlightedField = field;
    field.focus();
    field.scrollIntoView({ behavior: "smooth", block: "center" });
  }

  state.searchIndex = normalized;
  dom.searchStatus.textContent = `Risultato ${normalized + 1}/${state.searchResults.length}`;
}

function goToPreviousMatch() {
  if (!state.searchResults.length) {
    return;
  }
  showSearchResult(state.searchIndex - 1);
}

function goToNextMatch() {
  if (!state.searchResults.length) {
    return;
  }
  showSearchResult(state.searchIndex + 1);
}

function clearFieldHighlight() {
  if (state.highlightedField) {
    state.highlightedField.classList.remove("search-hit");
    state.highlightedField = null;
  }
}

function setHasChanges(hasChanges) {
  state.hasChanges = hasChanges;
  document.title = `${state.hasChanges ? "⚫ " : ""}i18n Editor (Electron)`;
}

async function translateField(key, section, targetLang, targetTextarea, button) {
  if (!state.translatorEnabled) {
    showToast("Traduzione automatica disabilitata", "error");
    return;
  }

  const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
  const sourceText = String(sourceField?.value ?? "").trim();

  if (!sourceText) {
    showToast("Testo italiano vuoto: impossibile tradurre", "error");
    return;
  }

  button.disabled = true;
  const previousLabel = button.textContent;
  button.textContent = "…";

  try {
    const translated = await window.editorApi.translate({
      text: sourceText,
      sourceLang: "it",
      targetLang,
    });

    targetTextarea.value = String(translated.text ?? "");

    await window.editorApi.updateTranslation({
      scope: state.currentScope,
      key,
      section,
      lang: targetLang,
      text: targetTextarea.value,
    });

    setHasChanges(true);
    showToast(`Traduzione completata (${targetLang})`, "success");
  } catch (error) {
    showToast(`Traduzione fallita: ${error.message}`, "error", 5000);
  } finally {
    button.textContent = previousLabel;
    button.disabled = !state.translatorEnabled;
  }
}

function refreshTranslateButtons() {
  const translateButtons = dom.tableBody.querySelectorAll(".translate-btn");
  for (const button of translateButtons) {
    button.disabled = !state.translatorEnabled || state.translateAllRunning;
  }
  if (dom.translateAllBtn) {
    dom.translateAllBtn.disabled =
      !state.translatorEnabled || state.translateAllRunning || !Array.isArray(state.currentEntries) || state.currentEntries.length === 0;
  }
}

async function translateAllFieldsInCurrentScope() {
  if (!state.translatorEnabled) {
    showToast("Traduzione automatica disabilitata", "error");
    return;
  }
  if (state.translateAllRunning) {
    return;
  }

  const queue = [];
  let skippedAlreadyTranslated = 0;
  for (const entry of state.currentEntries) {
    const key = Number(entry.key);
    const section = Number(entry.section ?? 0);
    const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
    const sourceText = String(sourceField?.value ?? "").trim();
    if (!sourceText) {
      continue;
    }

    for (const lang of state.languages) {
      if (lang === "it") {
        continue;
      }
      const targetField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, lang));
      const targetText = String(targetField?.value ?? "").trim();
      const existingText = String(entry.translations?.[lang] ?? "").trim();
      if (!targetField) {
        continue;
      }
      if (targetText || existingText) {
        skippedAlreadyTranslated += 1;
        continue;
      }
      queue.push({ key, section, lang, sourceText, targetField });
    }
  }

  if (!queue.length) {
    showToast(
      skippedAlreadyTranslated > 0
        ? `Nessuna traduzione da fare: ${skippedAlreadyTranslated} campi già tradotti`
        : "Nessun campo vuoto da tradurre nello scope corrente",
      "success"
    );
    return;
  }

  state.translateAllRunning = true;
  const prevLabel = dom.translateAllBtn.textContent;
  dom.translateAllBtn.textContent = `… 0/${queue.length}`;
  refreshTranslateButtons();

  let okCount = 0;
  let failCount = 0;

  try {
    for (let i = 0; i < queue.length; i += 1) {
      const item = queue[i];
      dom.translateAllBtn.textContent = `… ${i + 1}/${queue.length}`;
      try {
        const translated = await window.editorApi.translate({
          text: item.sourceText,
          sourceLang: "it",
          targetLang: item.lang,
        });

        item.targetField.value = String(translated.text ?? "");
        await window.editorApi.updateTranslation({
          scope: state.currentScope,
          key: item.key,
          section: item.section,
          lang: item.lang,
          text: item.targetField.value,
        });
        okCount += 1;
      } catch (error) {
        console.error("translateAll error", error);
        failCount += 1;
      }
    }
  } finally {
    state.translateAllRunning = false;
    dom.translateAllBtn.textContent = prevLabel;
    refreshTranslateButtons();
  }

  if (okCount > 0) {
    setHasChanges(true);
  }
  if (failCount > 0) {
    showToast(
      `Traduci tutto: ${okCount} tradotti, ${skippedAlreadyTranslated} ignorati, ${failCount} errori`,
      "error",
      5000
    );
    return;
  }
  showToast(`Traduci tutto: ${okCount} tradotti, ${skippedAlreadyTranslated} ignorati`, "success");
}

function makeFieldKey(scope, key, section, lang) {
  const scopePart = String(scope ?? "");
  return `${scopePart}:${Number(key)}:${Number(section ?? 0)}:${lang}`;
}

function restoreTheme() {
  const savedTheme = localStorage.getItem("i18n-editor-theme") ?? "light";
  applyTheme(savedTheme);
}

function applyTheme(themeMode) {
  const nextTheme = themeMode === "dark" ? "dark" : "light";
  document.body.classList.toggle("theme-dark", nextTheme === "dark");
  document.body.classList.toggle("theme-light", nextTheme !== "dark");
  dom.themeToggle.checked = nextTheme === "dark";
  localStorage.setItem("i18n-editor-theme", nextTheme);
}

let toastTimer = null;

function showToast(message, type = "success", timeout = 2500) {
  dom.toast.textContent = message;
  dom.toast.classList.remove("hidden", "success", "error");
  dom.toast.classList.add(type === "error" ? "error" : "success");

  if (toastTimer) {
    clearTimeout(toastTimer);
  }

  toastTimer = setTimeout(() => {
    dom.toast.classList.add("hidden");
  }, timeout);
}
