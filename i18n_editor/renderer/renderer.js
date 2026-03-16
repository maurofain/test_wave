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
  cancelTranslation: false,
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
  // dom.translateAllScopesBtn removed: functionality merged into translateAllBtn (Ctrl+click / Ctrl+right-click)
  dom.newKeyLabel = document.getElementById("new-key-label");
  dom.newKeyItText = document.getElementById("new-key-it-text");
  dom.rowsInfo = document.getElementById("rows-info");
  dom.confirmDialog = document.getElementById("confirm-dialog");
  dom.confirmOk = document.getElementById("confirm-ok");
  dom.confirmCancel = document.getElementById("confirm-cancel");
  dom.progressDialog = document.getElementById("translation-progress-dialog");
  dom.progressBar = document.getElementById("progress-bar");
  dom.progressText = document.getElementById("progress-text");
  dom.currentScopeLabel = document.getElementById("current-scope-label");
  dom.currentKeyLabel = document.getElementById("current-key-label");
  dom.currentLangLabel = document.getElementById("current-lang-label");
  dom.currentSourceText = document.getElementById("current-source-text");
  dom.currentTranslatedText = document.getElementById("current-translated-text");
  dom.progressStatsText = document.getElementById("progress-stats-text");
  dom.cancelTranslationBtn = document.getElementById("cancel-translation-btn");
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
      const msg = result.backupFile ? `Salvataggio completato. Backup: ${result.backupFile}` : `Salvataggio completato.`;
      showToast(msg, "success");
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

  // Click: translate current scope; Ctrl+Click: translate all scopes
  dom.translateAllBtn.addEventListener("click", async (event) => {
    if (event.ctrlKey) {
      await translateAllScopes(false);
    } else {
      await translateAllFieldsInCurrentScope(false);
    }
  });

  // Right-click (context menu): confirm retranslate. Ctrl+Right-click => confirm retranslate ALL scopes
  dom.translateAllBtn.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    if (event.ctrlKey) {
      showConfirmDialogAllScopes();
    } else {
      showConfirmDialog();
    }
  });

  // Confirm handlers: confirm action is attached dynamically by showConfirmDialog/showConfirmDialogAllScopes
  dom.confirmCancel.addEventListener("click", () => {
    hideConfirmDialog();
  });

  dom.confirmDialog.addEventListener("click", (event) => {
    if (event.target === dom.confirmDialog) {
      hideConfirmDialog();
    }
  });

  dom.cancelTranslationBtn.addEventListener("click", () => {
    state.cancelTranslation = true;
    dom.cancelTranslationBtn.disabled = true;
    dom.cancelTranslationBtn.textContent = "⏳ Annullamento...";
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

  const idTh = document.createElement("th");
  idTh.textContent = "ID";
  idTh.className = "col-id";
  dom.tableHeadRow.appendChild(idTh);

  const nameTh = document.createElement("th");
  nameTh.textContent = "Nome ID";
  nameTh.className = "col-name";
  dom.tableHeadRow.appendChild(nameTh);

  for (const lang of state.languages) {
    const th = document.createElement("th");
    const langName = state.languageLabels.get(lang) ?? lang.toUpperCase();
    th.textContent = `${langName} (${lang})`;
    th.className = "col-lang";
    dom.tableHeadRow.appendChild(th);
  }

  const actionsTh = document.createElement("th");
  actionsTh.textContent = "Azioni";
  actionsTh.className = "col-actions";
  dom.tableHeadRow.appendChild(actionsTh);
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
    emptyCell.colSpan = 3 + state.languages.length;
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
    keyCell.className = "meta-cell key-cell";
    keyCell.textContent = String(entry.key);
    tr.appendChild(keyCell);

    const nameCell = document.createElement("td");
    nameCell.className = "meta-cell name-cell";
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

    const actionsTd = document.createElement("td");
    actionsTd.className = "meta-cell row-actions actions-cell";

    const clearBtn = document.createElement("button");
    clearBtn.type = "button";
    clearBtn.className = "btn btn-neutral row-action-btn row-clear-btn";
    clearBtn.textContent = "Azzera";
    clearBtn.title = "Azzera tutte le lingue della riga (escluso Italiano)";
    clearBtn.disabled = state.translateAllRunning;
    clearBtn.addEventListener("click", async () => {
      await clearRowTranslations(entry.key, section, tr);
    });

    const copyItBtn = document.createElement("button");
    copyItBtn.type = "button";
    copyItBtn.className = "btn btn-neutral row-action-btn row-copy-it-btn";
    copyItBtn.textContent = "Copia IT";
    copyItBtn.title = "Copia il testo Italiano su tutte le altre lingue della riga";
    copyItBtn.disabled = state.translateAllRunning;
    copyItBtn.addEventListener("click", async () => {
      await copyItalianToRow(entry.key, section, tr);
    });

    const retranslateBtn = document.createElement("button");
    retranslateBtn.type = "button";
    retranslateBtn.className = "btn btn-neutral row-action-btn row-retranslate-btn";
    retranslateBtn.textContent = "Ritraduci";
    retranslateBtn.title = "Ritraduce tutte le lingue della riga da Italiano";
    retranslateBtn.disabled = !state.translatorEnabled || state.translateAllRunning;
    retranslateBtn.addEventListener("click", async () => {
      await retranslateRow(entry.key, section, tr);
    });

    actionsTd.appendChild(clearBtn);
    actionsTd.appendChild(copyItBtn);
    actionsTd.appendChild(retranslateBtn);
    tr.appendChild(actionsTd);

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

async function translateAllScopes(forceRetranslate = false) {
  console.log("translateAllScopes called with forceRetranslate:", forceRetranslate);
  
  if (!state.translatorEnabled) {
    showToast("Traduzione automatica disabilitata", "error");
    return;
  }
  if (state.translateAllRunning) {
    return;
  }

  const scopesToProcess = state.scopes.map(s => String(s.id));
  if (!scopesToProcess.length) {
    showToast("Nessuno scope disponibile", "error");
    return;
  }

  state.translateAllRunning = true;
  const prevLabel = dom.translateAllBtn.textContent;
  // show progress on the single button
  dom.translateAllBtn.disabled = true;

  let totalOk = 0;
  let totalFail = 0;
  let totalSkipped = 0;
  let totalTechnical = 0;

  showProgressDialog();

  try {
    for (let i = 0; i < scopesToProcess.length; i++) {
      if (state.cancelTranslation) {
        console.log("Translation of all scopes cancelled by user");
        break;
      }

      const scopeId = scopesToProcess[i];
      const scopeInfo = state.scopes.find(s => String(s.id) === scopeId);
      const scopeLabel = scopeInfo ? scopeInfo.label : scopeId;
      
      dom.translateAllBtn.textContent = `🌍 ${i + 1}/${scopesToProcess.length}: ${scopeLabel}`;
      
      // Carica lo scope
      await loadScope(scopeId);
      
      // Attendi un momento per permettere il rendering
      await new Promise(resolve => setTimeout(resolve, 100));
      
      // Traduci lo scope corrente (il popup è già aperto e rimane aperto)
      const result = await translateAllFieldsInCurrentScopeWithStats(forceRetranslate);
      totalOk += result.okCount;
      totalFail += result.failCount;
      totalSkipped += result.skippedAlreadyTranslated;
      totalTechnical += result.skippedTechnical;
      
      console.log(`Scope ${scopeLabel} completato: ${result.okCount} tradotti, ${result.failCount} errori`);
    }
  } finally {
    hideProgressDialog();
    state.translateAllRunning = false;
    dom.translateAllBtn.textContent = prevLabel;
    dom.translateAllBtn.disabled = false;
    refreshTranslateButtons();
  }

  // Salva una sola volta alla fine della traduzione di tutti gli scope
  try {
    // create backup only for the final save of the full-scope translation
    const saveResult = await window.editorApi.save({ createBackup: true });
    if (saveResult && saveResult.success) {
      // clear unsaved-changes flag so window can be closed
      setHasChanges(false);
      const msg = saveResult.backupFile ? `Salvataggio completato. Backup: ${saveResult.backupFile}` : `Salvataggio completato.`;
      showToast(msg, "success");
    } else {
      showToast(saveResult?.error ?? "Salvataggio finale fallito", "error");
    }
  } catch (e) {
    console.error('Errore durante salvataggio finale:', e);
    showToast('Errore durante il salvataggio finale', 'error');
  }

  const mode = forceRetranslate ? "Ritraduzione completa" : "Traduzione tutti gli scope";
  if (totalFail > 0) {
    showToast(
      `${mode}: ${totalOk} tradotti, ${totalSkipped} già tradotti, ${totalTechnical} tecnici ignorati, ${totalFail} errori`,
      "error",
      5000
    );
  } else {
    showToast(
      `${mode} completata: ${totalOk} tradotti${!forceRetranslate ? `, ${totalSkipped} già tradotti` : ""}, ${totalTechnical} tecnici ignorati`,
      "success",
      4000
    );
  }
}

async function translateAllFieldsInCurrentScopeWithStats(forceRetranslate = false) {
  const queue = [];
  let skippedAlreadyTranslated = 0;
  let skippedTechnical = 0;
  
  for (const entry of state.currentEntries) {
    const key = entry.key; // preserve original type (string or number) to support lvgl keys
    const section = Number(entry.section ?? 0);
    const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
    const sourceText = String(sourceField?.value ?? "").trim();
    if (!sourceText) {
      continue;
    }
    if (shouldSkipTechnicalTranslation(entry, sourceText)) {
      skippedTechnical += Math.max(state.languages.length - 1, 0);
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
      if (!forceRetranslate && (targetText || existingText)) {
        skippedAlreadyTranslated += 1;
        continue;
      }
      const rowElement = targetField.closest("tr");
      queue.push({ key, section, lang, sourceText, targetField, rowElement });
    }
  }

  let okCount = 0;
  let failCount = 0;

  const scopeInfo = state.scopes.find(s => String(s.id) === String(state.currentScope));
  const scopeLabel = scopeInfo ? scopeInfo.label : state.currentScope;

  for (let i = 0; i < queue.length; i += 1) {
    if (state.cancelTranslation) {
      console.log("Translation cancelled by user in scope", scopeLabel);
      break;
    }

    const item = queue[i];
    const entry = state.currentEntries.find(e => String(e.key) === String(item.key) && Number(e.section ?? 0) === Number(item.section ?? 0));
    const keyLabel = entry?.keyName || `Key ${item.key}`;

    updateProgressDialog({
      current: i + 1,
      total: queue.length,
      scopeLabel,
      keyLabel,
      lang: item.lang,
      sourceText: item.sourceText,
      translatedText: "Traduzione in corso...",
      stats: { ok: okCount, fail: failCount, skipped: skippedAlreadyTranslated }
    });

    try {
      const translated = await window.editorApi.translate({
        text: item.sourceText,
        sourceLang: "it",
        targetLang: item.lang,
      });

      const translatedText = String(translated.text ?? "");
      item.targetField.value = translatedText;
      await window.editorApi.updateTranslation({
        scope: state.currentScope,
        key: item.key,
        section: item.section,
        lang: item.lang,
        text: translatedText,
      });
      
      updateProgressDialog({
        current: i + 1,
        total: queue.length,
        scopeLabel,
        keyLabel,
        lang: item.lang,
        sourceText: item.sourceText,
        translatedText,
        stats: { ok: okCount + 1, fail: failCount, skipped: skippedAlreadyTranslated }
      });
      
      okCount += 1;
    } catch (error) {
      console.error("translateAll error", error);
      failCount += 1;
    }
  }

  return { okCount, failCount, skippedAlreadyTranslated, skippedTechnical };
}

function refreshTranslateButtons() {
  const translateButtons = dom.tableBody.querySelectorAll(".translate-btn");
  for (const button of translateButtons) {
    button.disabled = !state.translatorEnabled || state.translateAllRunning;
  }
  const rowRetranslateButtons = dom.tableBody.querySelectorAll(".row-retranslate-btn");
  for (const button of rowRetranslateButtons) {
    button.disabled = !state.translatorEnabled || state.translateAllRunning;
  }
  const rowClearButtons = dom.tableBody.querySelectorAll(".row-clear-btn");
  for (const button of rowClearButtons) {
    button.disabled = state.translateAllRunning;
  }
  const rowCopyButtons = dom.tableBody.querySelectorAll(".row-copy-it-btn");
  for (const button of rowCopyButtons) {
    button.disabled = state.translateAllRunning;
  }
  if (dom.translateAllBtn) {
    dom.translateAllBtn.disabled =
      !state.translatorEnabled || state.translateAllRunning || !Array.isArray(state.currentEntries) || state.currentEntries.length === 0;
  }
}

function setRowActionButtonsDisabled(rowElement, disabled) {
  if (!rowElement) {
    return;
  }
  const rowButtons = rowElement.querySelectorAll(".row-action-btn");
  for (const btn of rowButtons) {
    btn.disabled = Boolean(disabled);
  }
}

async function clearRowTranslations(key, section, rowElement) {
  setRowActionButtonsDisabled(rowElement, true);
  let updatedCount = 0;
  try {
    for (const lang of state.languages) {
      if (lang === "it") {
        continue;
      }
      const targetField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, lang));
      if (!targetField) {
        continue;
      }
      if (String(targetField.value ?? "").trim() === "") {
        continue;
      }
      targetField.value = "";
      await window.editorApi.updateTranslation({
        scope: state.currentScope,
        key,
        section,
        lang,
        text: "",
      });
      updatedCount += 1;
    }
    if (updatedCount > 0) {
      setHasChanges(true);
      showToast(`Riga ${key}: ${updatedCount} traduzioni azzerate`, "success");
    } else {
      showToast(`Riga ${key}: nessuna traduzione da azzerare`, "success");
    }
  } catch (error) {
    showToast(`Azzera riga fallito: ${error.message}`, "error", 5000);
  } finally {
    refreshTranslateButtons();
  }
}

async function copyItalianToRow(key, section, rowElement) {
  const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
  const sourceText = String(sourceField?.value ?? "");
  if (sourceText.trim() === "") {
    showToast("Testo italiano vuoto: niente da copiare", "error");
    return;
  }

  setRowActionButtonsDisabled(rowElement, true);
  let updatedCount = 0;

  try {
    for (const lang of state.languages) {
      if (lang === "it") {
        continue;
      }
      const targetField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, lang));
      if (!targetField) {
        continue;
      }
      targetField.value = sourceText;
      await window.editorApi.updateTranslation({
        scope: state.currentScope,
        key,
        section,
        lang,
        text: sourceText,
      });
      updatedCount += 1;
    }
    if (updatedCount > 0) {
      setHasChanges(true);
      showToast(`Riga ${key}: IT copiato su ${updatedCount} lingue`, "success");
    } else {
      showToast(`Riga ${key}: nessuna lingua da aggiornare`, "success");
    }
  } catch (error) {
    showToast(`Copia IT riga fallita: ${error.message}`, "error", 5000);
  } finally {
    refreshTranslateButtons();
  }
}

async function retranslateRow(key, section, rowElement) {
  if (!state.translatorEnabled) {
    showToast("Traduzione automatica disabilitata", "error");
    return;
  }

  const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
  const sourceText = String(sourceField?.value ?? "").trim();
  if (!sourceText) {
    showToast("Testo italiano vuoto: impossibile ritradurre la riga", "error");
    return;
  }

  setRowActionButtonsDisabled(rowElement, true);
  let okCount = 0;
  let failCount = 0;

  try {
    for (const lang of state.languages) {
      if (lang === "it") {
        continue;
      }
      const targetField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, lang));
      if (!targetField) {
        continue;
      }
      try {
        const translated = await window.editorApi.translate({
          text: sourceText,
          sourceLang: "it",
          targetLang: lang,
        });
        targetField.value = String(translated.text ?? "");
        await window.editorApi.updateTranslation({
          scope: state.currentScope,
          key,
          section,
          lang,
          text: targetField.value,
        });
        okCount += 1;
      } catch (error) {
        console.error("retranslateRow error", error);
        failCount += 1;
      }
    }

    if (okCount > 0) {
      setHasChanges(true);
    }
    if (failCount > 0) {
      showToast(`Riga ${key}: ${okCount} ritradotte, ${failCount} errori`, "error", 5000);
      return;
    }
    showToast(`Riga ${key}: ${okCount} traduzioni aggiornate`, "success");
  } finally {
    refreshTranslateButtons();
  }
}

function shouldSkipTechnicalTranslation(entry, sourceText) {
  const label = String(entry?.keyName ?? "").toLowerCase();
  const text = String(sourceText ?? "").trim();
  if (!text) {
    return true;
  }
  if (label.includes(".js.")) {
    return true;
  }
  if (text.length > 160 || text.includes("\n")) {
    return true;
  }
  if (/\b(function|const|let|var|return|if|else|for|while|switch|case|try|catch|finally|new|class)\b/.test(text)) {
    return true;
  }
  if (/\b(document|window|console|localStorage|sessionStorage|fetch|Promise|setTimeout|setInterval|querySelector|getElementById|addEventListener)\b|=>|===|!==|&&|\|\|/.test(text)) {
    return true;
  }
  if (/<\/?[a-z][^>]*>/i.test(text)) {
    return true;
  }
  if (/[.#]?[A-Za-z0-9_-]+\s*\{[^}]*\}/.test(text)) {
    return true;
  }
  return false;
}

async function translateAllFieldsInCurrentScope(forceRetranslate = false, managePopup = true, manageRunningFlag = true) {
  console.log("translateAllFieldsInCurrentScope called with forceRetranslate:", forceRetranslate, "managePopup:", managePopup, "manageRunningFlag:", manageRunningFlag);
  
  if (!state.translatorEnabled) {
    showToast("Traduzione automatica disabilitata", "error");
    return;
  }
  if (manageRunningFlag && state.translateAllRunning) {
    return;
  }

  const queue = [];
  let skippedAlreadyTranslated = 0;
  let skippedTechnical = 0;
  for (const entry of state.currentEntries) {
    const key = entry.key; // preserve original type (string or number) to support lvgl keys
    const section = Number(entry.section ?? 0);
    const sourceField = state.fieldMap.get(makeFieldKey(state.currentScope, key, section, "it"));
    const sourceText = String(sourceField?.value ?? "").trim();
    if (!sourceText) {
      continue;
    }
    if (shouldSkipTechnicalTranslation(entry, sourceText)) {
      skippedTechnical += Math.max(state.languages.length - 1, 0);
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
      // Se forceRetranslate è true, aggiungi sempre alla queue
      // Se forceRetranslate è false, salta se già tradotto
      if (!forceRetranslate && (targetText || existingText)) {
        skippedAlreadyTranslated += 1;
        continue;
      }
      const rowElement = targetField.closest("tr");
      queue.push({ key, section, lang, sourceText, targetField, rowElement });
    }
  }
  
  console.log("Queue length:", queue.length, "skipped:", skippedAlreadyTranslated, "technical:", skippedTechnical);

  if (!queue.length) {
    showToast(
      skippedAlreadyTranslated > 0 || skippedTechnical > 0
        ? `Nessuna traduzione da fare: ${skippedAlreadyTranslated} già tradotti, ${skippedTechnical} tecnici ignorati`
        : forceRetranslate
        ? "Nessun campo da ritradurre nello scope corrente"
        : "Nessun campo vuoto da tradurre nello scope corrente",
      "success"
    );
    return;
  }

  state.translateAllRunning = true;
  const prevLabel = dom.translateAllBtn.textContent;
  refreshTranslateButtons();

  let okCount = 0;
  let failCount = 0;

  const scopeInfo = state.scopes.find(s => String(s.id) === String(state.currentScope));
  const scopeLabel = scopeInfo ? scopeInfo.label : state.currentScope;

  if (managePopup) {
    showProgressDialog();
  }

  try {
    for (let i = 0; i < queue.length; i += 1) {
      if (state.cancelTranslation) {
        console.log("Translation cancelled by user");
        break;
      }

      const item = queue[i];
      const entry = state.currentEntries.find(e => String(e.key) === String(item.key) && Number(e.section ?? 0) === Number(item.section ?? 0));
      const keyLabel = entry?.keyName || `Key ${item.key}`;

      updateProgressDialog({
        current: i + 1,
        total: queue.length,
        scopeLabel,
        keyLabel,
        lang: item.lang,
        sourceText: item.sourceText,
        translatedText: "Traduzione in corso...",
        stats: { ok: okCount, fail: failCount, skipped: skippedAlreadyTranslated }
      });

      try {
        const translated = await window.editorApi.translate({
          text: item.sourceText,
          sourceLang: "it",
          targetLang: item.lang,
        });

        const translatedText = String(translated.text ?? "");
        item.targetField.value = translatedText;
        await window.editorApi.updateTranslation({
          scope: state.currentScope,
          key: item.key,
          section: item.section,
          lang: item.lang,
          text: translatedText,
        });
        
        updateProgressDialog({
          current: i + 1,
          total: queue.length,
          scopeLabel,
          keyLabel,
          lang: item.lang,
          sourceText: item.sourceText,
          translatedText,
          stats: { ok: okCount + 1, fail: failCount, skipped: skippedAlreadyTranslated }
        });
        
        okCount += 1;
      } catch (error) {
        console.error("translateAll error", error);
        failCount += 1;
        updateProgressDialog({
          current: i + 1,
          total: queue.length,
          scopeLabel,
          keyLabel,
          lang: item.lang,
          sourceText: item.sourceText,
          translatedText: `ERRORE: ${error.message}`,
          stats: { ok: okCount, fail: failCount + 1, skipped: skippedAlreadyTranslated }
        });
      }
    }
  } finally {
    if (managePopup) {
      hideProgressDialog();
    }
    if (manageRunningFlag) {
      state.translateAllRunning = false;
    }
    dom.translateAllBtn.textContent = prevLabel;
    refreshTranslateButtons();
  }

  if (okCount > 0) {
    setHasChanges(true);
  }
  const mode = forceRetranslate ? "Ritraduzione" : "Traduci tutto";
  if (failCount > 0) {
    showToast(
      `${mode}: ${okCount} tradotti, ${skippedAlreadyTranslated} già tradotti, ${skippedTechnical} tecnici ignorati, ${failCount} errori`,
      "error",
      5000
    );
    return;
  }
  showToast(
    `${mode}: ${okCount} tradotti${!forceRetranslate ? `, ${skippedAlreadyTranslated} già tradotti` : ""}, ${skippedTechnical} tecnici ignorati`,
    "success"
  );
}

function makeFieldKey(scope, key, section, lang) {
  const scopePart = String(scope ?? "");
  const keyPart = isNaN(Number(key)) ? String(key) : String(Number(key));
  return `${scopePart}:${keyPart}:${Number(section ?? 0)}:${lang}`;
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

function showConfirmDialog() {
  // Attach a one-time confirm handler for retranslation of current scope
  const handleOk = async () => {
    hideConfirmDialog();
    await translateAllFieldsInCurrentScope(true);
  };
  dom.confirmOk.addEventListener("click", handleOk, { once: true });
  dom.confirmDialog.classList.remove("hidden");
}

function hideConfirmDialog() {
  dom.confirmDialog.classList.add("hidden");
}

function showProgressDialog() {
  state.cancelTranslation = false;
  dom.cancelTranslationBtn.disabled = false;
  dom.cancelTranslationBtn.textContent = "❌ Annulla Traduzione";
  dom.progressBar.style.width = "0%";
  dom.progressText.textContent = "Preparazione...";
  dom.currentScopeLabel.textContent = "-";
  dom.currentKeyLabel.textContent = "-";
  dom.currentLangLabel.textContent = "-";
  dom.currentSourceText.textContent = "-";
  dom.currentTranslatedText.textContent = "-";
  dom.progressStatsText.textContent = "Tradotti: 0 | Errori: 0 | Saltati: 0";
  dom.progressDialog.classList.remove("hidden");
}

function hideProgressDialog() {
  dom.progressDialog.classList.add("hidden");
}

function updateProgressDialog(data) {
  const { current, total, scopeLabel, keyLabel, lang, sourceText, translatedText, stats } = data;
  
  const percentage = total > 0 ? Math.round((current / total) * 100) : 0;
  dom.progressBar.style.width = `${percentage}%`;
  dom.progressText.textContent = `${current} / ${total} (${percentage}%)`;
  
  if (scopeLabel) dom.currentScopeLabel.textContent = scopeLabel;
  if (keyLabel) dom.currentKeyLabel.textContent = keyLabel;
  if (lang) dom.currentLangLabel.textContent = lang.toUpperCase();
  if (sourceText) dom.currentSourceText.textContent = sourceText;
  if (translatedText) dom.currentTranslatedText.textContent = translatedText;
  
  if (stats) {
    dom.progressStatsText.textContent = `Tradotti: ${stats.ok} | Errori: ${stats.fail} | Saltati: ${stats.skipped}`;
  }
}

function showConfirmDialogAllScopes() {
  const dialogContent = dom.confirmDialog.querySelector(".confirm-dialog-content");
  const originalH3 = dialogContent.querySelector("h3").textContent;
  const originalP1 = dialogContent.querySelectorAll("p")[0].innerHTML;
  const originalP2 = dialogContent.querySelectorAll("p")[1].textContent;
  const originalBtnText = dom.confirmOk.textContent;

  dialogContent.querySelector("h3").textContent = "⚠️ Conferma Ritraduzione TUTTI GLI SCOPE";
  dialogContent.querySelectorAll("p")[0].innerHTML = "Vuoi ritradurre <strong>TUTTI</strong> i testi di <strong>TUTTE</strong> le lingue in <strong>TUTTI GLI SCOPE</strong>, sovrascrivendo anche le traduzioni esistenti?";
  dialogContent.querySelectorAll("p")[1].textContent = "Questa operazione può richiedere molto tempo e non può essere annullata.";
  dom.confirmOk.textContent = "Conferma Ritraduzione Completa";

  const handleOk = async () => {
    hideConfirmDialog();
    await translateAllScopes(true);
    restoreDialog();
  };

  const handleCancel = () => {
    hideConfirmDialog();
    restoreDialog();
  };

  const restoreDialog = () => {
    dialogContent.querySelector("h3").textContent = originalH3;
    dialogContent.querySelectorAll("p")[0].innerHTML = originalP1;
    dialogContent.querySelectorAll("p")[1].textContent = originalP2;
    dom.confirmOk.textContent = originalBtnText;
    dom.confirmOk.removeEventListener("click", handleOk);
    dom.confirmCancel.removeEventListener("click", handleCancel);
  };

  dom.confirmOk.addEventListener("click", handleOk, { once: true });
  dom.confirmCancel.addEventListener("click", handleCancel, { once: true });

  showConfirmDialog();
}

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
