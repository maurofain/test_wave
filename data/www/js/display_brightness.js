// Gestione luminosità display per pagina config
let displayBrightTimer = null;
let displayBrightPersistTimer = null;

// Funzione per gestire l'input della luminosità (live update)
function onDisplayBrightInput(val) {
    // Validazione e clamping del valore
    const brightness = parseInt(val, 10);
    const safeVal = isNaN(brightness) ? 0 : Math.max(0, Math.min(100, brightness));
    
    // Aggiorna l'elemento UI che mostra il valore
    const brightValEl = document.getElementById('bright_val');
    if (brightValEl) {
        brightValEl.innerText = safeVal;
    }
    
    // Applica immediatamente la luminosità (live update)
    if (document.getElementById('display_en').checked) {
        applyDisplayBrightness(safeVal);
    }
    
    // Debounce per salvataggio persistente
    if (displayBrightTimer) {
        clearTimeout(displayBrightTimer);
    }
    
    // Timer per salvataggio dopo che l'utente smette di modificare
    displayBrightTimer = setTimeout(() => {
        persistDisplayBrightness(safeVal);
    }, 1000); // 1 secondo dopo l'ultimo input
}

// Applica la luminosità al display (chiamata API)
async function applyDisplayBrightness(brightness) {
    try {
        // Chiama API per impostare la luminosità live (senza salvare)
        const response = await fetch('/api/test/led_bright', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ bright: brightness })
        });
        
        if (!response.ok) {
            console.warn('Display brightness apply failed:', response.status);
        }
    } catch (error) {
        console.error('Error applying display brightness:', error);
    }
}

// Salva la luminosità in modo persistente
async function persistDisplayBrightness(brightness) {
    try {
        // Prepara la configurazione da salvare
        const backlightSwitch = document.getElementById('display_backlight');
        const config = {
            display: {
                enabled: document.getElementById('display_en').checked,
                brt: brightness,
                backlight: backlightSwitch ? backlightSwitch.checked : true
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
            showBrightnessFeedback('✅ Luminosità salvata', true);
        } else {
            showBrightnessFeedback('❌ Errore salvataggio', false);
        }
    } catch (error) {
        console.error('Error persisting display brightness:', error);
        showBrightnessFeedback('❌ Errore connessione', false);
    }
}

// Mostra feedback all'utente
function showBrightnessFeedback(message, success) {
    const alertEl = document.getElementById('alert');
    if (alertEl) {
        alertEl.innerText = message;
        alertEl.className = success ? 'success' : 'error';
        
        // Auto-hide dopo 2 secondi
        setTimeout(() => {
            alertEl.innerText = '';
            alertEl.className = '';
        }, 2000);
    }
}

// Inizializzazione quando la pagina è caricata
function initDisplayBrightness() {
    const brightSlider = document.getElementById('lcd_bright');
    const displayEnable = document.getElementById('display_en');
    const backlightSwitch = document.getElementById('display_backlight');
    
    if (brightSlider && displayEnable) {
        // Imposta l'event handler per l'input
        brightSlider.addEventListener('input', (e) => {
            onDisplayBrightInput(e.target.value);
        });
        
        // Disabilita slider se display non abilitato
        brightSlider.disabled = !displayEnable.checked;
        
        // Event handler per abilitazione display
        displayEnable.addEventListener('change', (e) => {
            brightSlider.disabled = !e.target.checked;
            
            // Gestisci anche lo switch backlight
            if (backlightSwitch) {
                backlightSwitch.disabled = !e.target.checked;
                if (!e.target.checked) {
                    backlightSwitch.checked = false;
                    setBacklight(false);
                }
            }
            
            // Se si abilita il display, applica la luminosità corrente
            if (e.target.checked) {
                const currentBrightness = brightSlider.value;
                applyDisplayBrightness(currentBrightness);
            }
        });
        
        // Carica e applica la luminosità iniziale
        const initialBrightness = brightSlider.value;
        if (displayEnable.checked) {
            applyDisplayBrightness(initialBrightness);
        }
    }
    
    // Inizializzazione controllo backlight
    if (backlightSwitch && displayEnable) {
        // Imposta stato iniziale
        backlightSwitch.disabled = !displayEnable.checked;
        
        // Event listener per switch backlight
        backlightSwitch.addEventListener('change', function() {
            setBacklight(this.checked);
        });
        
        // Carica stato iniziale backlight
        loadBacklightState();
    }
}

// Funzione per impostare il backlight
async function setBacklight(turnOn) {
    try {
        const endpoint = turnOn ? '/api/test/backlight_on' : '/api/test/backlight_off';
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        
        if (response.ok) {
            const message = turnOn ? '✅ Backlight acceso' : '✅ Backlight spento';
            showBrightnessFeedback(message, true);
        } else {
            showBrightnessFeedback('❌ Errore impostazione backlight', false);
        }
    } catch (error) {
        console.error('Error setting backlight:', error);
        showBrightnessFeedback('❌ Errore connessione', false);
    }
}

// Carica stato backlight
async function loadBacklightState() {
    try {
        // Carica la configurazione corrente per ottenere lo stato del backlight
        const response = await fetch('/api/config');
        if (response.ok) {
            const config = await response.json();
            const backlightSwitch = document.getElementById('display_backlight');
            if (backlightSwitch && config.display) {
                backlightSwitch.checked = config.display.backlight !== false; // default true se non specificato
            }
        }
    } catch (error) {
        console.log('Config endpoint not available, using default backlight=true');
        const backlightSwitch = document.getElementById('display_backlight');
        if (backlightSwitch) {
            backlightSwitch.checked = true; // default
        }
    }
}

// Auto-inizializzazione
if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initDisplayBrightness);
    } else {
        initDisplayBrightness();
    }
}

// Esporta le funzioni globalmente per compatibilità
if (typeof window !== 'undefined') {
    window.onDisplayBrightInput = onDisplayBrightInput;
    window.applyDisplayBrightness = applyDisplayBrightness;
    window.persistDisplayBrightness = persistDisplayBrightness;
}
