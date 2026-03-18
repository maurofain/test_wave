// Gestione parametro PreFineCiclo nella pagina config
function addPreFineCicloField() {
    // Trova la sezione Timeouts
    const timeoutsSection = document.querySelector('h2');
    if (!timeoutsSection) return;
    
    // Cerca il testo "Timeouts" nei titoli delle sezioni
    const sections = document.querySelectorAll('h2');
    let timeoutsSectionEl = null;
    
    sections.forEach(section => {
        if (section.textContent.includes('Timeouts') || 
            section.textContent.includes('⏱️')) {
            timeoutsSectionEl = section.parentElement;
        }
    });
    
    if (!timeoutsSectionEl) {
        console.warn('Sezione Timeouts non trovata');
        return;
    }
    
    // Verifica se il campo esiste già
    if (document.getElementById('pre_fine_ciclo_percent')) {
        return; // Già aggiunto
    }
    
    // Crea il nuovo campo PreFineCiclo
    const fieldHtml = `
        <div style="display:flex;align-items:center;gap:10px;margin-top:10px;">
            <span>{{110}}:</span>
            <input type="number" id="pre_fine_ciclo_percent" min="0" max="99" value="70"
                   style="width:80px;padding:6px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box">
            <span>%</span>
            <button onclick="savePreFineCiclo()" style="padding:6px 12px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;">Salva</button>
        </div>
    `;
    
    // Trova dove inserire il campo (dopo gli altri campi timeout)
    const container = timeoutsSectionEl.querySelector('.container, .section');
    if (container) {
        // Inserisce dopo gli elementi esistenti
        container.insertAdjacentHTML('beforeend', fieldHtml);
    } else {
        // Fallback: inserisce direttamente nella sezione
        timeoutsSectionEl.insertAdjacentHTML('beforeend', fieldHtml);
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
