(function () {
  function ensureIoControls(outGrid, inGrid) {
    if (!outGrid || !inGrid) {
      return false;
    }

    if (outGrid.children.length === 8 && inGrid.children.length === 8) {
      return true;
    }

    outGrid.innerHTML = '';
    inGrid.innerHTML = '';

    for (let pin = 0; pin < 8; pin += 1) {
      const outButton = document.createElement('button');
      outButton.innerText = 'OUT' + pin;
      outButton.id = 'out_btn_' + pin;
      outButton.onclick = function () {
        if (typeof window.toggleIO === 'function') {
          window.toggleIO(pin);
        }
      };
      outGrid.appendChild(outButton);

      const inBadge = document.createElement('span');
      inBadge.id = 'in_val_' + pin;
      inBadge.style = 'padding:8px; border:1px solid #ccc; border-radius:4px; min-width:40px; text-align:center; background:#eee; color:#333;';
      inBadge.innerText = 'IN' + pin;
      inGrid.appendChild(inBadge);
    }

    return true;
  }

  function renderIoState(inputMask, outputMask) {
    for (let pin = 0; pin < 8; pin += 1) {
      const outButton = document.getElementById('out_btn_' + pin);
      if (outButton) {
        outButton.style.background = ((outputMask >> pin) & 1) === 1 ? '#2ecc71' : '#95a5a6';
      }

      const inBadge = document.getElementById('in_val_' + pin);
      if (inBadge) {
        const isOn = ((inputMask >> pin) & 1) === 1;
        inBadge.style.background = isOn ? '#2ecc71' : '#34495e';
        inBadge.style.color = isOn ? '#fff' : '#bdc3c7';
      }
    }
  }

  async function refreshIoExpanderPanel() {
    try {
      const outGrid = document.getElementById('outputs_grid');
      const inGrid = document.getElementById('inputs_grid');
      if (!outGrid || !inGrid) {
        return;
      }

      if (!ensureIoControls(outGrid, inGrid)) {
        return;
      }

      const response = await fetch('/api/test/io_get', { method: 'POST', cache: 'no-store' });
      if (!response.ok) {
        return;
      }

      const payload = await response.json();
      if (!payload || typeof payload.input !== 'number' || typeof payload.output !== 'number') {
        return;
      }

      renderIoState(payload.input >>> 0, payload.output >>> 0);
    } catch (_error) {
    }
  }

  refreshIoExpanderPanel();
  setInterval(refreshIoExpanderPanel, 1000);
})();

/* LED Bar Test Integration */
let ledBarTestActive = false;
let ledBarTestTimer = null;
let ledBarTestPhase = 'idle';

async function startLedBarTest() {
  if (ledBarTestActive) {
    stopLedBarTest();
    return;
  }
  
  try {
    ledBarTestActive = true;
    ledBarTestPhase = 'idle';
    
    const btn = document.getElementById('btn_led_bar_test');
    if (btn) {
      btn.innerHTML = '{{098}}';
      btn.style.background = '#e74c3c';
    }
    
    const statusEl = document.getElementById('led_bar_status');
    if (statusEl) {
      statusEl.innerHTML = '⏳ {{099}}';
    }
    
    // Inizializza LED bar (20 LED totali = 10 per striscia)
    const initRes = await mbPost('test/led_bar_init', { total_leds: 20 });
    if (!initRes.ok) {
      throw new Error(initRes.error || 'LED bar init failed');
    }
    
    // Imposta stato IDLE
    await mbPost('test/led_bar_set_state', { state: 0 });
    
    // Avvia test sequenziale
    ledBarTestTimer = setTimeout(() => {
      if (ledBarTestActive) {
        startProgressPhase();
      }
    }, 1000);
    
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
  
  await mbPost('test/led_bar_set_state', { state: 1 });
  
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
      await mbPost('test/led_bar_set_progress', { progress_percent: progress });
      progress += 5;
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
  
  await mbPost('test/led_bar_set_state', { state: 3 });
  
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
    await mbPost('test/led_bar_clear', {});
  } catch (error) {
    console.error('Stop LED bar error:', error);
  }
  
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

// Inizializzazione pulsante LED bar se presente
document.addEventListener('DOMContentLoaded', function() {
  const btn = document.getElementById('btn_led_bar_test');
  if (btn) {
    btn.addEventListener('click', startLedBarTest);
  }
});
