// Gestione parametro PreFineCiclo nella pagina config
const PREFINE_CICLO_LEGACY_ID = '110';
const PREFINE_CICLO_LABEL_FALLBACK = 'Soglia PreFineCiclo (%)';

// Escape HTML minimale per inserire testo tradotto in modo sicuro
function escapeHtml(value) {
    return String(value || '')
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

// Recupera la lingua corrente della UI (query string -> uiI18n -> default)
function getCurrentUiLanguage() {
    try {
        const langFromQuery = new URLSearchParams(window.location.search).get('lang');
        if (langFromQuery && langFromQuery.length === 2) {
            return langFromQuery.toLowerCase();
        }
    } catch (error) {
        console.warn('Impossibile leggere lang da query string:', error);
    }

    if (window.uiI18n && typeof window.uiI18n.language === 'string' && window.uiI18n.language.length === 2) {
        return window.uiI18n.language.toLowerCase();
    }

    return 'it';
}

// Cerca un testo i18n per legacyId nei record API, preferendo lo scope config
function findTextByLegacyId(records, legacyId) {
    if (!Array.isArray(records)) {
        return '';
    }

    let genericMatch = '';

    for (const item of records) {
        if (!item || typeof item !== 'object') {
            continue;
        }

        const itemKey = item.key != null ? String(item.key).trim() : '';
        if (itemKey !== String(legacyId)) {
            continue;
        }

        const text = item.text != null ? String(item.text).trim() : '';
        if (!text) {
            continue;
        }

        const scope = item.scope != null ? String(item.scope).toLowerCase() : '';
        const isConfigScope = scope.includes('config') || scope === '2';
        if (isConfigScope) {
            return text;
        }

        if (!genericMatch) {
            genericMatch = text;
        }
    }

    return genericMatch;
}

// Risolve la label i18n usando prima la tabella runtime e poi l'API testi
async function resolveLabelByLegacyId(legacyId, fallbackText) {
    if (window.uiI18n && typeof window.uiI18n.translate === 'function') {
        const candidates = [String(legacyId), `config.text.${legacyId}`, `p_config.${legacyId}`, `{{${legacyId}}}`];
        for (const key of candidates) {
            const translated = window.uiI18n.translate(key);
            if (translated && translated !== key && !/^\{\{\d+\}\}$/.test(translated)) {
                return translated;
            }
        }
    }

    try {
        const language = getCurrentUiLanguage();
        const response = await fetch(`/api/ui/texts?lang=${encodeURIComponent(language)}`, { cache: 'no-store' });
        if (response.ok) {
            const data = await response.json();
            const resolvedText = findTextByLegacyId(data.records, legacyId);
            if (resolvedText) {
                return resolvedText;
            }
        }
    } catch (error) {
        console.warn('Errore risoluzione label i18n PreFineCiclo:', error);
    }

    return fallbackText;
}

async function addPreFineCicloField() {
    // Verifica se il campo esiste già
    if (document.getElementById('pre_fine_ciclo_percent')) {
        return; // Già aggiunto
    }
    
    const slideshowField = document.getElementById('slideshow_speed_s');
    if (!slideshowField) {
        console.warn('Campo slideshow_speed_s non trovato');
        return;
    }

    const exitField = document.getElementById('timeout_language_exit_s');
    const exitGroup = exitField ? exitField.closest('.form-group') : null;
    const targetRow = (exitGroup && exitGroup.parentElement && exitGroup.parentElement.classList.contains('form-row'))
        ? exitGroup.parentElement
        : slideshowField.closest('.form-row') || slideshowField.closest('.sw-row') || slideshowField.parentElement;
    
    // Crea il nuovo campo PreFineCiclo
    const preFineLabel = await resolveLabelByLegacyId(PREFINE_CICLO_LEGACY_ID, PREFINE_CICLO_LABEL_FALLBACK);

    const fieldHtml = `
        <div class="form-group">
            <label>${escapeHtml(preFineLabel)}</label>
            <div style="display:flex;align-items:center;gap:8px;flex-wrap:wrap;">
                <input type="number" id="pre_fine_ciclo_percent" min="0" max="99" value="70"
                       style="width:80px;padding:8px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box">
                <span>%</span>
                <button type="button" onclick="savePreFineCiclo()" style="padding:6px 10px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;font-size:12px;">Salva</button>
            </div>
        </div>
    `;
    
    if (exitGroup && exitGroup.parentElement && exitGroup.parentElement.classList.contains('form-row')) {
        exitGroup.insertAdjacentHTML('afterend', fieldHtml);
    } else {
        targetRow.insertAdjacentHTML('beforeend', fieldHtml);
    }
    
    // Carica il valore corrente dalla configurazione
    loadPreFineCicloValue();
}

// Carica il valore corrente del parametro PreFineCiclo
async function loadPreFineCicloValue() {
    try {
        const response = await fetch('/api/config');
        const config = await response.json();
        
        if (config.timeouts && config.timeouts.pre_fine_ciclo_percent !== undefined) {
            const field = document.getElementById('pre_fine_ciclo_percent');
            if (field) {
                field.value = config.timeouts.pre_fine_ciclo_percent;
            }
        }
    } catch (error) {
        console.error('Errore caricamento PreFineCiclo:', error);
    }
}

// Salva il parametro PreFineCiclo
async function savePreFineCiclo() {
    const field = document.getElementById('pre_fine_ciclo_percent');
    if (!field) return;
    
    // Validazione
    let value = parseInt(field.value, 10);
    if (isNaN(value) || value < 0) value = 0;
    if (value > 99) value = 99;
    
    field.value = value; // Aggiorna il campo con il valore validato
    
    try {
        // Prepara la configurazione da salvare
        const config = {
            timeouts: {
                pre_fine_ciclo_percent: value
            }
        };
        
        // Salva la configurazione
        const response = await fetch('/api/config/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        if (response.ok) {
            // Mostra feedback di successo
            showPreFineCicloFeedback('✅ PreFineCiclo salvato', true);
        } else {
            showPreFineCicloFeedback('❌ Errore salvataggio', false);
        }
    } catch (error) {
        console.error('Errore salvataggio PreFineCiclo:', error);
        showPreFineCicloFeedback('❌ Errore connessione', false);
    }
}

// Mostra feedback all'utente
function showPreFineCicloFeedback(message, success) {
    // Crea un elemento di feedback temporaneo
    const feedback = document.createElement('div');
    feedback.textContent = message;
    feedback.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 10px 15px;
        border-radius: 5px;
        color: white;
        font-weight: bold;
        z-index: 9999;
        background: ${success ? '#27ae60' : '#e74c3c'};
        box-shadow: 0 2px 5px rgba(0,0,0,0.2);
    `;
    
    document.body.appendChild(feedback);
    
    // Auto-remove dopo 3 secondi
    setTimeout(() => {
        if (feedback.parentNode) {
            feedback.parentNode.removeChild(feedback);
        }
    }, 3000);
}

// Inizializzazione quando la pagina è caricata
function initPreFineCicloField() {
    // Aspetta che il DOM sia completamente caricato
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            setTimeout(addPreFineCicloField, 100); // Piccolo ritardo per assicurarsi che le sezioni siano caricate
        });
    } else {
        setTimeout(addPreFineCicloField, 100);
    }
}

// Auto-inizializzazione
initPreFineCicloField();

// Esporta le funzioni globalmente per compatibilità
if (typeof window !== 'undefined') {
    window.addPreFineCicloField = addPreFineCicloField;
    window.savePreFineCiclo = savePreFineCiclo;
    window.loadPreFineCicloValue = loadPreFineCicloValue;
}
