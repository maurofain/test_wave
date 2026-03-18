let programs = [];

const RELAY_LAYOUT = {
    totalOutputs: 12,
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
    'programs.js.037': 'Relay 1-{end} (MH1001)',
    'programs.js.038': 'Relay {start}-{end} (Waveshare Modbus {idx})',
    'programs.js.039': 'Ogni editbox: 4 bit (0/1), completamento automatico con trailing zeroes.',
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

function sanitizeRelaySegment(raw, fill = false) {
    let value = String(raw || '').replace(/[^01]/g, '');
    if (value.length > 4) value = value.slice(0, 4);
    if (fill) value = value.padEnd(4, '0');
    return value;
}

function relayBit(mask, relayNumber) {
    return (((mask >>> 0) >> (relayNumber - 1)) & 0x01) ? '1' : '0';
}

function maskSegmentFromRange(mask, startRelay, validBits) {
    let bits = '';
    for (let i = 0; i < validBits; i += 1) {
        bits += relayBit(mask, startRelay + i);
    }
    return bits.padEnd(4, '0');
}

function applySegmentToMask(mask, segmentValue, startRelay, validBits) {
    let nextMask = (toInt(mask, 0) >>> 0);
    for (let i = 0; i < validBits; i += 1) {
        const bit = (segmentValue.charAt(i) === '1');
        const bitPos = startRelay + i - 1;
        const bitMask = (1 << bitPos);
        if (bit) {
            nextMask |= bitMask;
        } else {
            nextMask &= ~bitMask;
        }
    }
    return (nextMask >>> 0);
}

function buildDeviceGroups() {
    const groups = [];
    const total = Math.max(1, toInt(RELAY_LAYOUT.totalOutputs, 12));
    const local = Math.max(1, Math.min(total, toInt(RELAY_LAYOUT.localOutputs, 4)));
    const modbusSpan = Math.max(1, toInt(RELAY_LAYOUT.modbusGroupOutputs, 8));

    groups.push({
        start: 1,
        end: local,
        label: trf('programs.js.037', { end: local }),
        isLocal: true,
    });

    let nextStart = local + 1;
    let modbusIdx = 1;

    while (nextStart <= total) {
        const nextEnd = Math.min(total, nextStart + modbusSpan - 1);
        groups.push({
            start: nextStart,
            end: nextEnd,
            label: trf('programs.js.038', {
                start: nextStart,
                end: nextEnd,
                idx: modbusIdx,
            }),
            isLocal: false,
        });
        nextStart = nextEnd + 1;
        modbusIdx += 1;
    }

    return groups;
}

function buildRelayMaskEditor(mask, rowIndex) {
    const groups = buildDeviceGroups();
    const normalizedMask = (toInt(mask, 0) >>> 0);

    const rows = groups.map((group) => {
        const groupLen = group.end - group.start + 1;
        const segmentCount = Math.ceil(groupLen / 4);

        let inputsHtml = '';
        for (let segmentIdx = 0; segmentIdx < segmentCount; segmentIdx += 1) {
            const segmentStart = group.start + (segmentIdx * 4);
            const validBits = Math.min(4, group.end - segmentStart + 1);
            const segmentValue = maskSegmentFromRange(normalizedMask, segmentStart, validBits);

            inputsHtml += `<input class="relay-mask-box" type="text" maxlength="4" minlength="4" pattern="[01]{4}" value="${segmentValue}" data-row-idx="${rowIndex}" data-seg-start="${segmentStart}" data-valid-bits="${validBits}" oninput="onRelayMaskSegmentInput(this)" onblur="onRelayMaskSegmentBlur(this)" onchange="onRelayMaskSegmentChange(this)">`;
        }

        return `<div class="relay-mask-device"><div class="relay-mask-label">${group.label}</div><div class="relay-mask-boxes">${inputsHtml}</div></div>`;
    });

    return `<div class="relay-mask-editor">${rows.join('')}<div class="relay-mask-help">${escapeHtml(tr('programs.js.039'))}</div></div>`;
}

function normalizeProgram(program, idx) {
    const entry = program || {};
    const programId = toInt(entry.program_id, idx + 1);

    return {
        program_id: programId,
        display_name: String(entry.name || ''),
        enabled: !!entry.enabled,
        price_units: toInt(entry.price_units, 0),
        duration_sec: toInt(entry.duration_sec, 0),
        pause_max_suspend_sec: toInt(entry.pause_max_suspend_sec, 0),
        relay_mask: (toInt(entry.relay_mask, 0) >>> 0),
    };
}

function rowHtml(program, idx) {
    const p = normalizeProgram(program, idx);
    const displayName = tr(p.display_name) || String(p.program_id);

    return `<tr>
        <td>${p.program_id}</td>
        <td>${escapeHtml(displayName)}</td>
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
}

window.onRelayMaskSegmentInput = function onRelayMaskSegmentInput(element) {
    element.value = sanitizeRelaySegment(element.value, false);
};

window.onRelayMaskSegmentBlur = function onRelayMaskSegmentBlur(element) {
    element.value = sanitizeRelaySegment(element.value, true);
};

window.onRelayMaskSegmentChange = function onRelayMaskSegmentChange(element) {
    const rowIndex = toInt(element.dataset.rowIdx, -1);
    const segmentStart = toInt(element.dataset.segStart, 1);
    const validBits = toInt(element.dataset.validBits, 4);

    if (rowIndex < 0 || rowIndex >= programs.length) {
        return;
    }

    const normalizedValue = sanitizeRelaySegment(element.value, true);
    element.value = normalizedValue;

    const current = normalizeProgram(programs[rowIndex], rowIndex);
    const nextMask = applySegmentToMask(current.relay_mask, normalizedValue, segmentStart, validBits);

    programs[rowIndex] = {
        ...current,
        relay_mask: nextMask,
    };
};

async function loadPrograms() {
    try {
        const response = await fetch('/api/programs');
        if (!response.ok) {
            throw new Error('HTTP ' + response.status);
        }

        const data = await response.json();
        programs = Array.isArray(data.programs) ? data.programs.map((program, idx) => normalizeProgram(program, idx)) : [];

        const totalOutputs = toInt(data.relay_outputs_count, RELAY_LAYOUT.totalOutputs);
        const localOutputs = toInt(data.relay_local_count, RELAY_LAYOUT.localOutputs);
        const modbusGroupOutputs = toInt(data.relay_modbus_group_count, RELAY_LAYOUT.modbusGroupOutputs);

        RELAY_LAYOUT.totalOutputs = Math.max(1, totalOutputs);
        RELAY_LAYOUT.localOutputs = Math.max(1, Math.min(RELAY_LAYOUT.totalOutputs, localOutputs));
        RELAY_LAYOUT.modbusGroupOutputs = Math.max(1, modbusGroupOutputs);

        render();
        showStatus(tr('programs.js.030'), true);
    } catch (error) {
        showStatus(tr('programs.js.031') + error.message, false);
    }
}

async function savePrograms() {
    try {
        const maxBits = Math.max(1, RELAY_LAYOUT.totalOutputs);
        const maxMask = (maxBits >= 31) ? 0x7FFFFFFF : ((1 << maxBits) - 1);

        programs = programs.map((program, idx) => {
            const normalized = normalizeProgram(program, idx);
            normalized.relay_mask = (normalized.relay_mask & maxMask) >>> 0;
            return normalized;
        });

        const payloadPrograms = programs.map((program) => ({
            program_id: program.program_id,
            enabled: program.enabled,
            price_units: program.price_units,
            duration_sec: program.duration_sec,
            pause_max_suspend_sec: program.pause_max_suspend_sec,
            relay_mask: program.relay_mask,
        }));

        const response = await fetch('/api/programs/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ programs: payloadPrograms }),
        });

        if (!response.ok) {
            const text = await response.text();
            throw new Error(text || ('HTTP ' + response.status));
        }

        showStatus(tr('programs.js.032'), true);
        render();
    } catch (error) {
        showStatus(tr('programs.js.033') + error.message, false);
    }
}

window.loadPrograms = loadPrograms;
window.savePrograms = savePrograms;

window.addEventListener('load', loadPrograms);
