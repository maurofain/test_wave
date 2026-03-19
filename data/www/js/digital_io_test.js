// Digital I/O Test Functions
// Visualizza stato DIP switches e relay tramite API digital_io

// Funzione principale per aggiornare lo stato dei DIP
async function updateDigitalIOState() {
    try {
        // 1. Ottieni snapshot completo
        const snapshotResponse = await fetch('/api/test/digital_io_snapshot', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'}
        });
        
        if (!snapshotResponse.ok) {
            throw new Error(`Snapshot failed: ${snapshotResponse.status}`);
        }
        
        const snapshot = await snapshotResponse.json();
        
        if (snapshot.status === 'ok') {
            updateDigitalIODisplay(snapshot);
        } else {
            console.error('Snapshot error:', snapshot.error);
            updateDigitalIOError(snapshot.error);
        }
        
    } catch (error) {
        console.error('Digital I/O update failed:', error);
        updateDigitalIOError(error.message);
    }
}

// Aggiorna display con i dati dello snapshot
function updateDigitalIODisplay(snapshot) {
    // Aggiorna stato DIP1-3
    for (let i = 1; i <= 3; i++) {
        const dipEl = document.getElementById(`dip${i}_state`);
        if (dipEl) {
            const isActive = (snapshot.inputs_mask & (1 << (i - 1))) !== 0;
            dipEl.textContent = isActive ? 'ON' : 'OFF';
            dipEl.className = `dip-state ${isActive ? 'dip-on' : 'dip-off'}`;
        }
    }
    
    // Aggiorna stato altri input
    for (let i = 4; i <= 8; i++) {
        const inputEl = document.getElementById(`input${i}_state`);
        if (inputEl) {
            const isActive = (snapshot.inputs_mask & (1 << (i - 1))) !== 0;
            inputEl.textContent = isActive ? 'HIGH' : 'LOW';
            inputEl.className = `input-state ${isActive ? 'input-high' : 'input-low'}`;
        }
    }
    
    // Aggiorna stato output (relay)
    for (let i = 1; i <= 8; i++) {
        const outputEl = document.getElementById(`output${i}_state`);
        if (outputEl) {
            const isActive = (snapshot.outputs_mask & (1 << (i - 1))) !== 0;
            outputEl.textContent = isActive ? 'ON' : 'OFF';
            outputEl.className = `output-state ${isActive ? 'output-on' : 'output-off'}`;
        }
    }
    
    // Aggiorna hex display
    const inputsHexEl = document.getElementById('digital_io_inputs_hex');
    const outputsHexEl = document.getElementById('digital_io_outputs_hex');
    
    if (inputsHexEl) inputsHexEl.textContent = `0x${snapshot.inputs_hex.toString(16).toUpperCase().padStart(4, '0')}`;
    if (outputsHexEl) outputsHexEl.textContent = `0x${snapshot.outputs_hex.toString(16).toUpperCase().padStart(4, '0')}`;
    
    // Aggiorna timestamp
    const timestampEl = document.getElementById('digital_io_timestamp');
    if (timestampEl) {
        timestampEl.textContent = new Date().toLocaleTimeString();
    }
}

// Gestisce errori
function updateDigitalIOError(error) {
    const errorEl = document.getElementById('digital_io_error');
    if (errorEl) {
        errorEl.textContent = `Errore: ${error}`;
        errorEl.style.display = 'block';
    }
    
    // Nasconde stati
    for (let i = 1; i <= 8; i++) {
        const dipEl = document.getElementById(`dip${i}_state`);
        const inputEl = document.getElementById(`input${i}_state`);
        const outputEl = document.getElementById(`output${i}_state`);
        
        if (dipEl) dipEl.textContent = '--';
        if (inputEl) inputEl.textContent = '--';
        if (outputEl) outputEl.textContent = '--';
    }
}

// Toggle relay (output)
async function toggleDigitalOutput(outputId) {
    try {
        // Prima leggi stato attuale
        const readResponse = await fetch('/api/test/digital_io_get_output', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({output_id: outputId})
        });
        
        if (!readResponse.ok) {
            throw new Error(`Read output ${outputId} failed`);
        }
        
        const readResult = await readResponse.json();
        if (readResult.status !== 'ok') {
            throw new Error(`Read error: ${readResult.error}`);
        }
        
        // Inverti stato
        const newState = !readResult.state;
        const setResponse = await fetch('/api/test/digital_io_set_output', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                output_id: outputId,
                value: newState ? 1 : 0
            })
        });
        
        if (!setResponse.ok) {
            throw new Error(`Set output ${outputId} failed`);
        }
        
        // Aggiorna display
        setTimeout(updateDigitalIOState, 100);
        
    } catch (error) {
        console.error(`Toggle output ${outputId} failed:`, error);
        alert(`Errore toggle output ${outputId}: ${error.message}`);
    }
}

// Auto-refresh
let digitalIOAutoRefresh = null;
let digitalIOAutoRefreshInterval = 2000; // 2 secondi

function toggleDigitalIOAutoRefresh() {
    const btn = document.getElementById('btn_digital_io_autorefresh');
    if (!btn) return;
    
    if (digitalIOAutoRefresh) {
        clearInterval(digitalIOAutoRefresh);
        digitalIOAutoRefresh = null;
        btn.textContent = '▶️ Auto-refresh';
        btn.style.background = '#3498db';
    } else {
        digitalIOAutoRefresh = setInterval(updateDigitalIOState, digitalIOAutoRefreshInterval);
        btn.textContent = '⏸️ Ferma Auto-refresh';
        btn.style.background = '#e74c3c';
        // Aggiorna subito
        updateDigitalIOState();
    }
}

// Inizializzazione quando DOM è pronto
document.addEventListener('DOMContentLoaded', function() {
    // Avvia auto-refresh se l'elemento esiste
    const autoRefreshEl = document.getElementById('btn_digital_io_autorefresh');
    if (autoRefreshEl && document.querySelector('#section_digital_io')) {
        toggleDigitalIOAutoRefresh();
    }
});

// Export funzioni globali
window.updateDigitalIOState = updateDigitalIOState;
window.toggleDigitalOutput = toggleDigitalOutput;
window.toggleDigitalIOAutoRefresh = toggleDigitalIOAutoRefresh;
