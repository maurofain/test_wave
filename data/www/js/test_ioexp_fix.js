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

(function () {
  const MDB_POLL_MS = 1000;
  const MDB_POST_REFRESH_DELAYS = [250, 600, 1200, 2000];
  const MDB_REVALUE_LIMIT_WAIT_MS = 180;
  const MDB_REVALUE_LIMIT_MAX_ATTEMPTS = 12;
  const MDB_REVALUE_STATUS_REQUEST_PENDING = 1;
  const mdbCashlessState = {
    busy: false,
    sessionOpen: false,
    revalueLimit: 100,
    timerId: null
  };

  function mdbGetElement(id) {
    return document.getElementById(id);
  }

  function mdbBuildTestMarkup() {
    return "<div class='test-item'><span class='test-label'>Credito attuale</span><div class='test-controls'><input type='text' id='mdb_credit_current' value='--' readonly style='width:120px; font-family:monospace; font-weight:bold; text-align:right;'></div></div><div class='test-item'><span class='test-label'>Ricarica</span><div class='test-controls'><input type='number' id='mdb_revalue_amount' value='10' min='1' max='100' style='width:80px'><button type='button' id='btn_mdb_revalue' disabled>🔄 Ricarica</button></div></div><div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:5px;'><div style='display:flex; align-items:center; gap:8px;'><button onclick=\"copySerialMonitor('mdb', this)\" style='background:#7f8c8d; padding:4px 8px; font-size:12px;'>📋 Copia</button><span style='font-weight:bold;'>Monitor:</span></div><div class='test-controls'><select id='mdb_mode' onchange=\"clearSerial('mdb')\"><option value='HEX'>HEX</option><option value='TEXT'>TEXT</option></select><button onclick=\"clearSerial('mdb')\" style='background:#95a5a6; padding:4px 8px; font-size:12px;'>🗑️ Clear</button></div></div><div id='mdb_status' class='status-box'>Pronto per test MDB</div>";
  }

  function mdbReplaceTestSection() {
    const section = mdbGetElement('section_mdb');
    if (!section) {
      return false;
    }

    const title = section.querySelector('h2');
    if (!title) {
      return false;
    }

    if (mdbGetElement('mdb_credit_current') && mdbGetElement('btn_mdb_revalue')) {
      return true;
    }

    Array.from(section.children).forEach(function (child) {
      if (child !== title) {
        child.remove();
      }
    });
    title.insertAdjacentHTML('afterend', mdbBuildTestMarkup());

    if (window.uiI18n && typeof window.uiI18n.apply === 'function') {
      window.uiI18n.apply(section);
    }

    return true;
  }

  function mdbClampRechargeAmount() {
    const input = mdbGetElement('mdb_revalue_amount');
    const limit = Math.max(1, Math.min(100, mdbCashlessState.revalueLimit || 100));
    if (!input) {
      return limit;
    }

    let value = parseInt(input.value, 10);
    if (!Number.isFinite(value)) {
      value = 10;
    }
    if (value < 1) {
      value = 1;
    }
    if (value > limit) {
      value = limit;
    }

    input.min = '1';
    input.max = String(limit);
    input.value = String(value);
    return value;
  }

  function mdbUpdateRechargeControls(isReady) {
    const input = mdbGetElement('mdb_revalue_amount');
    const button = mdbGetElement('btn_mdb_revalue');
    const disabled = !isReady || mdbCashlessState.busy;

    if (input) {
      input.disabled = disabled;
    }
    if (button) {
      button.disabled = disabled;
      button.textContent = mdbCashlessState.busy ? '⏳' : '🔄 Ricarica';
    }
  }

  function mdbUpdateCreditField(cashless) {
    const input = mdbGetElement('mdb_credit_current');
    if (!input) {
      return;
    }

    if (cashless && cashless.online && cashless.session_open) {
      input.value = String(Math.max(0, parseInt(cashless.credit_cents, 10) || 0));
    } else {
      input.value = '--';
    }
  }

  async function mdbFetchCashlessStatus() {
    const response = await fetch('/api/mdb/cashless/status', { cache: 'no-store' });
    if (!response.ok) {
      throw new Error('HTTP ' + response.status);
    }

    const payload = await response.json();
    return payload && payload.cashless ? payload.cashless : null;
  }

  async function mdbRequestRevalueLimit() {
    try {
      const response = await fetch('/api/mdb/cashless/revalue_limit', { method: 'POST' });
      return response.ok;
    } catch (_error) {
      return false;
    }
  }

  function mdbDelay(milliseconds) {
    return new Promise(function (resolve) {
      window.setTimeout(resolve, milliseconds);
    });
  }

  function mdbUpdateRevalueLimit(cashless) {
    const rawLimit = parseInt(cashless && cashless.revalue_limit_cents, 10) || 0;
    if (rawLimit > 0) {
      mdbCashlessState.revalueLimit = Math.max(1, Math.min(100, rawLimit));
    }
    mdbClampRechargeAmount();
  }

  async function mdbWaitForRevalueLimit() {
    for (let attempt = 0; attempt < MDB_REVALUE_LIMIT_MAX_ATTEMPTS; attempt += 1) {
      const cashless = await mdbFetchCashlessStatus();
      const isReady = !!(cashless && cashless.online && cashless.session_open);

      mdbCashlessState.sessionOpen = isReady;
      mdbUpdateCreditField(cashless);

      if (!isReady) {
        mdbUpdateRechargeControls(false);
        return null;
      }

      mdbUpdateRevalueLimit(cashless);
      if ((parseInt(cashless.revalue_limit_cents, 10) || 0) > 0 &&
          parseInt(cashless.revalue_status, 10) !== MDB_REVALUE_STATUS_REQUEST_PENDING) {
        return cashless;
      }

      await mdbDelay(MDB_REVALUE_LIMIT_WAIT_MS);
    }

    return null;
  }

  async function mdbRefreshCashlessUi() {
    if (!mdbReplaceTestSection()) {
      return;
    }

    try {
      const cashless = await mdbFetchCashlessStatus();
      const isReady = !!(cashless && cashless.online && cashless.session_open);

      mdbUpdateCreditField(cashless);
      if (isReady) {
        mdbUpdateRevalueLimit(cashless);
      } else {
        mdbCashlessState.revalueLimit = 100;
        mdbClampRechargeAmount();
      }

      mdbCashlessState.sessionOpen = isReady;
      mdbUpdateRechargeControls(isReady);
    } catch (_error) {
      mdbCashlessState.sessionOpen = false;
      mdbCashlessState.revalueLimit = 100;
      mdbUpdateCreditField(null);
      mdbClampRechargeAmount();
      mdbUpdateRechargeControls(false);
    }
  }

  function mdbSchedulePostRechargeRefresh(stepIndex) {
    if (stepIndex >= MDB_POST_REFRESH_DELAYS.length) {
      mdbCashlessState.busy = false;
      mdbUpdateRechargeControls(mdbCashlessState.sessionOpen);
      return;
    }

    window.setTimeout(async function () {
      await mdbRefreshCashlessUi();
      if (!mdbCashlessState.sessionOpen) {
        mdbCashlessState.busy = false;
        mdbUpdateRechargeControls(false);
        return;
      }
      mdbSchedulePostRechargeRefresh(stepIndex + 1);
    }, MDB_POST_REFRESH_DELAYS[stepIndex]);
  }

  async function mdbHandleRechargeClick() {
    if (mdbCashlessState.busy) {
      return;
    }

    await mdbRefreshCashlessUi();
    if (!mdbCashlessState.sessionOpen) {
      return;
    }

    mdbCashlessState.busy = true;
    mdbUpdateRechargeControls(true);

    try {
      const limitRequested = await mdbRequestRevalueLimit();
      if (!limitRequested) {
        mdbCashlessState.busy = false;
        await mdbRefreshCashlessUi();
        return;
      }

      const limitStatus = await mdbWaitForRevalueLimit();
      if (!limitStatus) {
        mdbCashlessState.busy = false;
        await mdbRefreshCashlessUi();
        return;
      }

      const amount = mdbClampRechargeAmount();
      const response = await fetch('/api/mdb/cashless/revalue', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amount_cents: amount })
      });

      if (!response.ok) {
        mdbCashlessState.busy = false;
        await mdbRefreshCashlessUi();
        return;
      }

      mdbSchedulePostRechargeRefresh(0);
    } catch (_error) {
      mdbCashlessState.busy = false;
      await mdbRefreshCashlessUi();
    }
  }

  function mdbBindRechargeUi() {
    if (!mdbReplaceTestSection()) {
      return;
    }

    const input = mdbGetElement('mdb_revalue_amount');
    const button = mdbGetElement('btn_mdb_revalue');
    if (!input || !button || button.dataset.mdbBound === '1') {
      return;
    }

    button.dataset.mdbBound = '1';
    input.addEventListener('change', mdbClampRechargeAmount);
    input.addEventListener('input', mdbClampRechargeAmount);
    button.addEventListener('click', mdbHandleRechargeClick);
    mdbClampRechargeAmount();
    mdbRefreshCashlessUi();
    if (!mdbCashlessState.timerId) {
      mdbCashlessState.timerId = window.setInterval(mdbRefreshCashlessUi, MDB_POLL_MS);
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', mdbBindRechargeUi);
  } else {
    mdbBindRechargeUi();
  }
})();
