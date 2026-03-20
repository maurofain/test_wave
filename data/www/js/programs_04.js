let programs = [];
let programNameLanguages = [];
let selectedProgramNameLanguage = 'it';
const loadedProgramNameLanguages = new Set();
const dirtyProgramNameLanguages = new Set();
const programNameTranslationsByLang = Object.create(null);

const RELAY_LAYOUT = {
    totalOutputs: 16,
    localOutputs: 4,
    modbusGroupOutputs: 8,
};

const I18N_DEFAULTS = {
    'programs.js.028': 'Pausa impostata a',
    'programs.js.029': 's per tutti i programmi',
    'programs.js.030': 'Tabella programmi caricata',
    'programs.js.031': 'Errore caricamento: ',
    'programs.js.032': 'Tabella programmi salvata',
    'programs.js.033': 'Errore salvataggio: ',
};

const LANGUAGE_FLAGS = {
    it: '🇮🇹',
    en: '🇬🇧',
    fr: '🇫🇷',
    de: '🇩🇪',
    es: '🇪🇸',
};

function tr(key) {
    const i18n = window.uiI18n;
    if (i18n && typeof i18n.translate === 'function') {
        const translated = i18n.translate(key);
        if (translated !== undefined && translated !== null && translated !== key) {
            return String(translated);
        }
    }
    return I18N_DEFAULTS[key] || key;
}

function trf(key, values) {
    let text = tr(key);
    if (!values || typeof values !== 'object') {
        return text;
    }

    Object.keys(values).forEach((name) => {
        const token = `{${name}}`;
        text = text.split(token).join(String(values[name]));
    });

    return text;
}

function toInt(value, fallback = 0) {
    const num = parseInt(value, 10);
    return Number.isNaN(num) ? fallback : num;
}

function escapeHtml(text) {
    return String(text || '')
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function showStatus(msg, ok) {
    const status = document.getElementById('status');
    status.textContent = msg;
    status.className = 'status ' + (ok ? 'ok' : 'err');
}

const PROGRAM_RELAY_ITEMS = [
    { label: 'R1', outputId: 6, hover: 'R1 / MH1001 O6' },
    { label: 'R2', outputId: 5, hover: 'R2 / MH1001 O5' },
    { label: 'R3', outputId: 3, hover: 'R3 / MH1001 O3' },
    { label: 'R4', outputId: 4, hover: 'R4 / MH1001 O4' },
    { label: 'R5', outputId: 9, hover: 'R5 / MODBUS01 O1' },
    { label: 'R6', outputId: 10, hover: 'R6 / MODBUS01 O2' },
    { label: 'R7', outputId: 11, hover: 'R7 / MODBUS01 O3' },
    { label: 'R8', outputId: 12, hover: 'R8 / MODBUS01 O4' },
    { label: 'R9', outputId: 13, hover: 'R9 / MODBUS01 O5' },
    { label: 'R10', outputId: 14, hover: 'R10 / MODBUS01 O6' },
    { label: 'R11', outputId: 15, hover: 'R11 / MODBUS01 O7' },
    { label: 'R12', outputId: 16, hover: 'R12 / MODBUS01 O8' },
];

const SERVICE_OUTPUT_ITEMS = [
    { label: 'LED1', outputId: 1, hover: 'LED1 / MH1001 O1' },
    { label: 'LED2', outputId: 2, hover: 'LED2 / MH1001 O2' },
    { label: 'LED3', outputId: 7, hover: 'LED3 / MH1001 O7' },
    { label: 'HEATER', outputId: 8, hover: 'HEATER / MH1001 O8' },
];

function outputIsEnabled(mask, outputId) {
    const bitPos = outputId - 1;
    return (((mask >>> 0) >> bitPos) & 0x01) !== 0;
}

function setOutputEnabled(mask, outputId, enabled) {
    let nextMask = (mask >>> 0);
    const bitMask = (1 << (outputId - 1));
    if (enabled) {
        nextMask |= bitMask;
    } else {
        nextMask &= ~bitMask;
    }
    return (nextMask >>> 0);
}

function buildCheckboxLine(items, rowIndex, mask, cssClass) {
    const checks = items.map((item) => {
        const checked = outputIsEnabled(mask, item.outputId) ? 'checked' : '';
        return `<label class="relay-check" title="${escapeHtml(item.hover)}"><input type="checkbox" ${checked} data-row-idx="${rowIndex}" data-output-id="${item.outputId}" onchange="onProgramOutputToggle(this)"></label>`;
    });

    return `<div class="relay-check-line ${cssClass}">${checks.join('')}</div>`;
}

function buildRelayMaskEditor(mask, rowIndex) {
    const normalizedMask = (toInt(mask, 0) >>> 0);
    return `<div class="relay-check-editor">${buildCheckboxLine(PROGRAM_RELAY_ITEMS, rowIndex, normalizedMask, 'relay-check-line-relays')}${buildCheckboxLine(SERVICE_OUTPUT_ITEMS, rowIndex, normalizedMask, 'relay-check-line-service')}</div>`;
}

function normalizeProgram(program, idx) {
    const entry = program || {};
    const programId = toInt(entry.program_id, idx + 1);

    return {
        program_id: programId,
        name: String(entry.name || ''),
        display_name: String(entry.display_name || entry.name || ''),
        enabled: !!entry.enabled,
        price_units: toInt(entry.price_units, 0),
        duration_sec: toInt(entry.duration_sec, 0),
        pause_max_suspend_sec: toInt(entry.pause_max_suspend_sec, 0),
        relay_mask: (toInt(entry.relay_mask, 0) >>> 0),
    };
}

function getProgramNameStore(language) {
    if (!programNameTranslationsByLang[language]) {
        programNameTranslationsByLang[language] = Object.create(null);
    }
    return programNameTranslationsByLang[language];
}

function getProgramLanguageFallback(program) {
    const normalized = normalizeProgram(program, 0);
    return normalized.display_name || normalized.name || String(normalized.program_id);
}

function getProgramNameForLanguage(program, language) {
    const normalized = normalizeProgram(program, 0);
    const store = getProgramNameStore(language);
    const key = String(normalized.program_id);

    if (Object.prototype.hasOwnProperty.call(store, key)) {
        return String(store[key] || '');
    }

    return getProgramLanguageFallback(normalized);
}

function setProgramNameForLanguage(programId, language, value) {
    const store = getProgramNameStore(language);
    store[String(programId)] = String(value || '');
}

function languageFlag(language) {
    return LANGUAGE_FLAGS[language] || '🏳️';
}

function sortLanguages(languages) {
    return [...languages].sort((left, right) => {
        if (left.code === 'it') return -1;
        if (right.code === 'it') return 1;
        return String(left.code).localeCompare(String(right.code));
    });
}

function renderLanguageToolbar() {
    const toolbar = document.getElementById('programNameToolbar');
    if (!toolbar) {
        return;
    }

    toolbar.innerHTML = programNameLanguages.map((language) => {
        const activeClass = language.code === selectedProgramNameLanguage ? ' active' : '';
        return `<button type="button" class="program-name-lang-btn${activeClass}" title="${escapeHtml(language.label || language.code)}" onclick="selectProgramNameLanguage('${escapeHtml(language.code)}')"><span class="program-name-lang-flag">${languageFlag(language.code)}</span><span class="program-name-lang-code">${escapeHtml(String(language.code).toUpperCase())}</span></button>`;
    }).join('');
}

function rowHtml(program, idx) {
    const p = normalizeProgram(program, idx);
    const displayName = getProgramNameForLanguage(p, selectedProgramNameLanguage);

    return `<tr>
        <td>${p.program_id}</td>
        <td><input type="text" value="${escapeHtml(displayName)}" oninput="onProgramNameInput(${idx}, this.value)"></td>
        <td style="text-align:center"><input type="checkbox" ${p.enabled ? 'checked' : ''} onchange="programs[${idx}].enabled=this.checked"></td>
        <td><input type="number" min="0" max="65535" value="${p.price_units}" onchange="programs[${idx}].price_units=toInt(this.value,0)"></td>
        <td><input type="number" min="0" max="65535" value="${p.duration_sec}" onchange="programs[${idx}].duration_sec=toInt(this.value,0)"></td>
        <td>${buildRelayMaskEditor(p.relay_mask, idx)}</td>
    </tr>`;
}

function render() {
    const body = document.getElementById('programRows');
    body.innerHTML = '';
    programs.forEach((program, idx) => {
        body.insertAdjacentHTML('beforeend', rowHtml(program, idx));
    });
    renderLanguageToolbar();
}

window.onProgramOutputToggle = function onProgramOutputToggle(element) {
    const rowIndex = toInt(element.dataset.rowIdx, -1);
    const outputId = toInt(element.dataset.outputId, 0);

    if (rowIndex < 0 || rowIndex >= programs.length || outputId <= 0) {
        return;
    }

    const current = normalizeProgram(programs[rowIndex], rowIndex);
    const nextMask = setOutputEnabled(current.relay_mask, outputId, !!element.checked);

    programs[rowIndex] = {
        ...current,
        relay_mask: nextMask,
    };
};

window.onProgramNameInput = function onProgramNameInput(rowIndex, value) {
    if (rowIndex < 0 || rowIndex >= programs.length) {
        return;
    }

    const current = normalizeProgram(programs[rowIndex], rowIndex);
    programs[rowIndex] = {
        ...current,
        name: String(value || ''),
        display_name: String(value || ''),
    };

    setProgramNameForLanguage(current.program_id, selectedProgramNameLanguage, value);
    dirtyProgramNameLanguages.add(selectedProgramNameLanguage);
};

async function loadProgramNameLanguages() {
    const response = await fetch('/api/ui/languages', { cache: 'no-store' });
    if (!response.ok) {
        throw new Error('HTTP ' + response.status);
    }

    const payload = await response.json();
    const languages = Array.isArray(payload.languages) ? payload.languages.filter((language) => language && typeof language.code === 'string' && language.code.length === 2) : [];
    programNameLanguages = sortLanguages(languages);

    const hasSelected = programNameLanguages.some((language) => language.code === selectedProgramNameLanguage);
    if (!hasSelected) {
        const italian = programNameLanguages.find((language) => language.code === 'it');
        selectedProgramNameLanguage = italian ? italian.code : ((programNameLanguages[0] && programNameLanguages[0].code) || 'it');
    }
}

function parseProgramNameRecords(records) {
    function parseProgramIdFromKey(recordKey) {
        if (typeof recordKey === 'string') {
            if (recordKey.startsWith('program_name_')) {
                return toInt(recordKey.slice('program_name_'.length), 0);
            }
            if (/^\d+$/.test(recordKey)) {
                const numericKey = toInt(recordKey, 0);
                if (numericKey >= 801 && numericKey <= 899) {
                    return numericKey - 800;
                }
                return numericKey;
            }
            return 0;
        }

        if (typeof recordKey === 'number' && Number.isFinite(recordKey)) {
            const numericKey = toInt(recordKey, 0);
            if (numericKey >= 801 && numericKey <= 899) {
                return numericKey - 800;
            }
            return numericKey;
        }

        return 0;
    }

    const names = Object.create(null);
    if (!Array.isArray(records)) {
        return names;
    }

    records.forEach((record) => {
        if (!record || typeof record.text !== 'string') {
            return;
        }

        const scopeId = toInt(record.scope_id ?? record.scope, 0);
        if (scopeId !== 2) {
            return;
        }

        const programId = parseProgramIdFromKey(record.key);
        if (programId <= 0) {
            return;
        }

        names[String(programId)] = record.text;
    });

    return names;
}

async function loadProgramNamesForLanguage(language, forceReload) {
    if (!forceReload && loadedProgramNameLanguages.has(language)) {
        return;
    }

    const response = await fetch(`/api/ui/texts?lang=${encodeURIComponent(language)}`, { cache: 'no-store' });
    if (!response.ok) {
        throw new Error('HTTP ' + response.status);
    }

    const payload = await response.json();
    programNameTranslationsByLang[language] = parseProgramNameRecords(payload.records);
    loadedProgramNameLanguages.add(language);
}

window.selectProgramNameLanguage = async function selectProgramNameLanguage(language) {
    try {
        await loadProgramNamesForLanguage(language, false);
        selectedProgramNameLanguage = language;
        render();
    } catch (error) {
        showStatus(tr('programs.js.031') + error.message, false);
    }
};

async function loadPrograms() {
    try {
        const [programsResponse] = await Promise.all([
            fetch('/api/programs', { cache: 'no-store' }),
            loadProgramNameLanguages(),
        ]);

        if (!programsResponse.ok) {
            throw new Error('HTTP ' + programsResponse.status);
        }

        const data = await programsResponse.json();
        programs = Array.isArray(data.programs) ? data.programs.map((program, idx) => normalizeProgram(program, idx)) : [];

        const totalOutputs = toInt(data.relay_outputs_count, RELAY_LAYOUT.totalOutputs);
        const localOutputs = toInt(data.relay_local_count, RELAY_LAYOUT.localOutputs);
        const modbusGroupOutputs = toInt(data.relay_modbus_group_count, RELAY_LAYOUT.modbusGroupOutputs);

        RELAY_LAYOUT.totalOutputs = Math.max(1, totalOutputs);
        RELAY_LAYOUT.localOutputs = Math.max(1, Math.min(RELAY_LAYOUT.totalOutputs, localOutputs));
        RELAY_LAYOUT.modbusGroupOutputs = Math.max(1, modbusGroupOutputs);

        await loadProgramNamesForLanguage(selectedProgramNameLanguage, true);
        render();
        showStatus(tr('programs.js.030'), true);
    } catch (error) {
        showStatus(tr('programs.js.031') + error.message, false);
    }
}

function buildProgramNameTranslationsPayload() {
    const payload = Object.create(null);

    dirtyProgramNameLanguages.forEach((language) => {
        const names = Object.create(null);
        programs.forEach((program, idx) => {
            const normalized = normalizeProgram(program, idx);
            names[String(normalized.program_id)] = getProgramNameForLanguage(normalized, language);
        });
        payload[language] = names;
    });

    return payload;
}

async function savePrograms() {
    try {
        const maxBits = Math.max(1, RELAY_LAYOUT.totalOutputs);
        const maxMask = (maxBits >= 31) ? 0x7FFFFFFF : ((1 << maxBits) - 1);

        programs = programs.map((program, idx) => {
            const normalized = normalizeProgram(program, idx);
            normalized.relay_mask = (normalized.relay_mask & maxMask) >>> 0;
            normalized.name = getProgramNameForLanguage(normalized, selectedProgramNameLanguage);
            normalized.display_name = normalized.name;
            return normalized;
        });

        const payloadPrograms = programs.map((program) => ({
            program_id: program.program_id,
            name: program.name,
            enabled: program.enabled,
            price_units: program.price_units,
            duration_sec: program.duration_sec,
            pause_max_suspend_sec: program.pause_max_suspend_sec,
            relay_mask: program.relay_mask,
        }));

        const response = await fetch('/api/programs/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                programs: payloadPrograms,
                program_name_translations: buildProgramNameTranslationsPayload(),
            }),
        });

        if (!response.ok) {
            const text = await response.text();
            throw new Error(text || ('HTTP ' + response.status));
        }

        dirtyProgramNameLanguages.clear();
        showStatus(tr('programs.js.032'), true);
        window.alert(tr('programs.js.032'));
        render();
    } catch (error) {
        showStatus(tr('programs.js.033') + error.message, false);
    }
}

window.loadPrograms = loadPrograms;
window.savePrograms = savePrograms;

window.addEventListener('load', loadPrograms);
