// Test LED Bar - Device virtuale per gestione 2 strisce sincrone
let ledBarTestActive = false;
let ledBarTestTimer = null;
let ledBarTestPhase = 'idle'; // 'idle', 'progress', 'finished'

async function startLedBarTest() {
    if (ledBarTestActive) {
        stopLedBarTest();
        return;
    }
    
    try {
        // Ferma altri test LED attivi
        await stopAllLedTests();
        
        ledBarTestActive = true;
        ledBarTestPhase = 'idle';
        
        // Aggiorna UI
        const btn = document.getElementById('btn_led_bar_test');
        if (btn) {
            btn.innerHTML = '{{098}}';
            btn.style.background = '#e74c3c';
        }
        
        const statusEl = document.getElementById('led_bar_status');
        if (statusEl) {
            statusEl.innerHTML = '⏳ {{099}}';
        }
        
        // Inizializza LED bar (assume 20 LED totali = 10 per striscia)
        const initRes = await mbPost('led_bar/init', { total_leds: 20 });
        if (!initRes.ok) {
            throw new Error(initRes.error || 'LED bar init failed');
        }
        
        // Imposta stato IDLE
        await mbPost('led_bar/set_state', { state: 0 }); // LED_BAR_STATE_IDLE = 0
        
        // Avvia test sequenziale
        ledBarTestTimer = setTimeout(() => {
            if (ledBarTestActive) {
                startProgressPhase();
            }
        }, 1000); // 1 secondo di IDLE
        
    } catch (error) {
        console.error('LED bar test error:', error);
        stopLedBarTest();
        const statusEl = document.getElementById('led_bar_status');
        if (statusEl) {
            statusEl.innerHTML = '❌ ' + (error.message || 'Errore test');
        }
    }
}

async function startProgressPhase() {
    if (!ledBarTestActive) return;
    
    ledBarTestPhase = 'progress';
    
    // Imposta stato RUNNING
    await mbPost('led_bar/set_state', { state: 1 }); // LED_BAR_STATE_RUNNING = 1
    
    // Progressione 0-100% in 5 secondi (20 step ogni 250ms)
    let progress = 0;
    const progressInterval = setInterval(async () => {
        if (!ledBarTestActive || progress > 100) {
            clearInterval(progressInterval);
            if (ledBarTestActive) {
                startFinishedPhase();
            }
            return;
        }
        
        try {
            await mbPost('led_bar/set_progress', { progress_percent: progress });
            progress += 5; // 5% ogni 250ms = 100% in 5 secondi
        } catch (error) {
            console.error('Progress error:', error);
            clearInterval(progressInterval);
            stopLedBarTest();
        }
    }, 250);
}

async function startFinishedPhase() {
    if (!ledBarTestActive) return;
    
    ledBarTestPhase = 'finished';
    
    // Imposta stato FINISHED per lampeggio
    await mbPost('led_bar/set_state', { state: 3 }); // LED_BAR_STATE_FINISHED = 3
    
    // Lampeggio per 3 secondi poi ferma
    setTimeout(() => {
        if (ledBarTestActive) {
            stopLedBarTest();
        }
    }, 3000);
}

async function stopLedBarTest() {
    ledBarTestActive = false;
    
    if (ledBarTestTimer) {
        clearTimeout(ledBarTestTimer);
        ledBarTestTimer = null;
    }
    
    try {
        // Spegne LED bar
        await mbPost('led_bar/clear', {});
    } catch (error) {
        console.error('Stop LED bar error:', error);
    }
    
    // Aggiorna UI
    const btn = document.getElementById('btn_led_bar_test');
    if (btn) {
        btn.innerHTML = '{{097}}';
        btn.style.background = '#27ae60';
    }
    
    const statusEl = document.getElementById('led_bar_status');
    if (statusEl) {
        statusEl.innerHTML = '{{100}}';
    }
}

async function stopAllLedTests() {
    // Ferma altri test LED che potrebbero essere attivi
    try {
        // Chiama clear per spegnere tutti i LED
        await mbPost('led/clear', {});
    } catch (error) {
        // Ignora errori se il endpoint non esiste
    }
}

// Funzione di inizializzazione da chiamare al caricamento della pagina
function initLedBarTest() {
    const btn = document.getElementById('btn_led_bar_test');
    if (btn) {
        btn.addEventListener('click', startLedBarTest);
    }
}

// Auto-inizializzazione
if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initLedBarTest);
    } else {
        initLedBarTest();
    }
}
