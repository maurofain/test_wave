(function () {
    if (window.__touch_map_ui_ready) {
        return;
    }
    window.__touch_map_ui_ready = true;

    const TOUCH_BUTTON_MAX = 10;
    const TOUCH_INPUT_MIN = 1;
    const TOUCH_INPUT_MAX = 12;
    const TOUCH_INPUT_NONE = 0;

    let sInputOptions = [];

    function toInt(value, fallback = 0) {
        const parsed = parseInt(value, 10);
        return Number.isNaN(parsed) ? fallback : parsed;
    }

    function sanitizeInputId(value) {
        const parsed = toInt(value, TOUCH_INPUT_NONE);
        if (parsed < TOUCH_INPUT_MIN || parsed > TOUCH_INPUT_MAX) {
            return TOUCH_INPUT_NONE;
        }
        return parsed;
    }

    function inputCodeFromId(inputId) {
        return `IN${String(inputId).padStart(2, '0')}`;
    }

    function ensureStyles() {
        if (document.getElementById('touch_map_styles')) {
            return;
        }

        const style = document.createElement('style');
        style.id = 'touch_map_styles';
        style.textContent = `
            .touch-map-note { font-size: 12px; color: #566573; margin: 8px 0 12px 0; }
            .touch-map-table { width: 100%; border-collapse: collapse; margin-top: 8px; }
            .touch-map-table th, .touch-map-table td { border: 1px solid #e5e7eb; padding: 8px; text-align: left; }
            .touch-map-table th { background: #ecf0f1; color: #2c3e50; }
            .touch-map-label { font-weight: bold; color: #34495e; }
            .touch-map-select { width: 100%; padding: 6px; border: 1px solid #d0d7de; border-radius: 4px; box-sizing: border-box; }
            .touch-map-row.conflict td { background: #fdeaea; }
            .touch-map-select.conflict { background: #f8d7da; border-color: #c0392b; color: #7a1f27; }
            .touch-map-conflict { margin-top: 8px; font-weight: bold; color: #c0392b; min-height: 18px; }
        `;
        document.head.appendChild(style);
    }

    function ensureSection() {
        const form = document.getElementById('configForm');
        if (!form) {
            return null;
        }

        let section = document.getElementById('touch_button_map_section');
        if (section) {
            return section;
        }

        section = document.createElement('div');
        section.className = 'section collapsed';
        section.id = 'touch_button_map_section';

        const rowHtml = [];
        for (let buttonIndex = 1; buttonIndex <= TOUCH_BUTTON_MAX; buttonIndex += 1) {
            rowHtml.push(`
                <tr class="touch-map-row" data-button-index="${buttonIndex}">
                    <td class="touch-map-label">{{155}} ${buttonIndex}</td>
                    <td><select class="touch-map-select" id="touch_map_btn_${buttonIndex}" data-button-index="${buttonIndex}"></select></td>
                </tr>
            `);
        }

        section.innerHTML = `
            <h2>{{156}}<span class="section-toggle-icon">▸</span></h2>
            <p class="touch-map-note">{{157}}</p>
            <table class="touch-map-table">
                <thead>
                    <tr><th>{{158}}</th><th>{{159}}</th></tr>
                </thead>
                <tbody id="touch_map_rows">${rowHtml.join('')}</tbody>
            </table>
            <div id="touch_map_conflict_notice" class="touch-map-conflict"></div>
        `;

        const actionsSection = Array.from(form.querySelectorAll('.section')).find((item) => item.querySelector('button[type="submit"]'));
        if (actionsSection) {
            form.insertBefore(section, actionsSection);
        } else {
            form.appendChild(section);
        }

        const selectElements = section.querySelectorAll('.touch-map-select');
        selectElements.forEach((selectElement) => {
            selectElement.addEventListener('change', applyConflicts);
        });

        if (typeof initCollapsibleSections === 'function') {
            initCollapsibleSections();
        }

        return section;
    }

    function normalizeInputOptions(rawList) {
        if (!Array.isArray(rawList) || rawList.length === 0) {
            const fallback = [];
            for (let inputId = TOUCH_INPUT_MIN; inputId <= TOUCH_INPUT_MAX; inputId += 1) {
                fallback.push({ id: inputId, code: inputCodeFromId(inputId), available: true });
            }
            return fallback;
        }

        const result = [];
        const seen = new Set();
        rawList.forEach((item) => {
            if (!item || typeof item !== 'object') {
                return;
            }
            const id = sanitizeInputId(item.id);
            if (id === TOUCH_INPUT_NONE || seen.has(id)) {
                return;
            }
            seen.add(id);
            result.push({
                id,
                code: (item.code && typeof item.code === 'string') ? item.code : inputCodeFromId(id),
                available: (typeof item.available === 'boolean') ? item.available : true,
            });
        });

        result.sort((a, b) => a.id - b.id);
        return result;
    }

    function setOptionsToSelect(selectElement, selectedValue) {
        const selected = sanitizeInputId(selectedValue);
        const optionsHtml = [`<option value="${TOUCH_INPUT_NONE}">{{160}}</option>`];

        sInputOptions.forEach((item) => {
            const availabilityText = item.available ? '' : ' {{161}}';
            optionsHtml.push(`<option value="${item.id}">${item.code}${availabilityText}</option>`);
        });

        selectElement.innerHTML = optionsHtml.join('');
        const selectedString = String(selected);
        if (selectElement.querySelector(`option[value="${selectedString}"]`)) {
            selectElement.value = selectedString;
        } else {
            selectElement.value = String(TOUCH_INPUT_NONE);
        }
    }

    function applyConfigToSection(configJson) {
        ensureStyles();
        const section = ensureSection();
        if (!section) {
            return;
        }

        sInputOptions = normalizeInputOptions(configJson && configJson.digital_io_inputs);

        const mapButtons = (configJson && configJson.touch_map && Array.isArray(configJson.touch_map.buttons))
            ? configJson.touch_map.buttons
            : [];

        for (let buttonIndex = 1; buttonIndex <= TOUCH_BUTTON_MAX; buttonIndex += 1) {
            const selectElement = document.getElementById(`touch_map_btn_${buttonIndex}`);
            if (!selectElement) {
                continue;
            }

            const mappedInput = sanitizeInputId(mapButtons[buttonIndex - 1]);
            setOptionsToSelect(selectElement, mappedInput);
        }

        applyConflicts();
    }

    function readCurrentMapping() {
        const mapping = [];
        for (let buttonIndex = 1; buttonIndex <= TOUCH_BUTTON_MAX; buttonIndex += 1) {
            const selectElement = document.getElementById(`touch_map_btn_${buttonIndex}`);
            if (!selectElement) {
                mapping.push(TOUCH_INPUT_NONE);
                continue;
            }
            mapping.push(sanitizeInputId(selectElement.value));
        }
        return mapping;
    }

    function applyConflicts() {
        const rows = document.querySelectorAll('#touch_map_rows .touch-map-row');
        const usage = {};
        const values = readCurrentMapping();

        values.forEach((inputId) => {
            if (inputId <= TOUCH_INPUT_NONE) {
                return;
            }
            usage[inputId] = (usage[inputId] || 0) + 1;
        });

        let conflictCount = 0;
        rows.forEach((row) => {
            const buttonIndex = toInt(row.getAttribute('data-button-index'), 0);
            const selectElement = row.querySelector('.touch-map-select');
            if (!buttonIndex || !selectElement) {
                return;
            }

            const mappedInput = sanitizeInputId(selectElement.value);
            const hasConflict = mappedInput > 0 && usage[mappedInput] > 1;
            row.classList.toggle('conflict', hasConflict);
            selectElement.classList.toggle('conflict', hasConflict);
            if (hasConflict) {
                conflictCount += 1;
            }
        });

        const conflictNotice = document.getElementById('touch_map_conflict_notice');
        if (!conflictNotice) {
            return;
        }

        if (conflictCount > 0) {
            conflictNotice.textContent = '{{162}}';
        } else {
            conflictNotice.textContent = '';
        }
    }

    async function refreshFromApi() {
        ensureStyles();
        ensureSection();
        try {
            const response = await fetch('/api/config', { cache: 'no-store' });
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const configJson = await response.json();
            applyConfigToSection(configJson);
        } catch (error) {
            console.warn('[touch_map] load failed', error);
            applyConfigToSection({});
        }
    }

    function wrapConfigLoad() {
        if (typeof window.loadConfig !== 'function') {
            return;
        }
        if (window.loadConfig.__touchMapWrapped) {
            return;
        }

        const originalLoadConfig = window.loadConfig;
        const wrapped = async function (...args) {
            const result = await originalLoadConfig.apply(this, args);
            await refreshFromApi();
            return result;
        };
        wrapped.__touchMapWrapped = true;
        window.loadConfig = wrapped;
    }

    function wrapFetchForSave() {
        if (window.__touch_map_fetch_wrapped) {
            return;
        }
        window.__touch_map_fetch_wrapped = true;

        const previousFetch = window.fetch.bind(window);
        window.fetch = function (input, init) {
            let requestUrl = '';
            if (typeof input === 'string') {
                requestUrl = input;
            } else if (input && typeof input.url === 'string') {
                requestUrl = input.url;
            }

            if (requestUrl.indexOf('/api/config/save') !== -1 && init && typeof init.body === 'string') {
                try {
                    const payload = JSON.parse(init.body);
                    payload.touch_map = payload.touch_map || {};
                    payload.touch_map.buttons = readCurrentMapping();
                    init = { ...init, body: JSON.stringify(payload) };
                } catch (error) {
                    console.warn('[touch_map] save payload patch failed', error);
                }
            }

            return previousFetch(input, init);
        };
    }

    ensureStyles();
    ensureSection();
    wrapConfigLoad();
    wrapFetchForSave();

    window.addEventListener('load', function () {
        refreshFromApi();
    });
})();
