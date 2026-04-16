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

(function () {
  const CASHLESS_POLL_MS = 1000;
  const CASHLESS_POST_REFRESH_DELAYS = [250, 600, 1200, 2000];
  const CASHLESS_REVALUE_LIMIT_WAIT_MS = 180;
  const CASHLESS_REVALUE_LIMIT_MAX_ATTEMPTS = 12;
  const CASHLESS_REVALUE_STATUS_REQUEST_PENDING = 1;
  const CASHLESS_DEFAULT_LIMIT_CENTS = 10000;
  const pendingRevalue = {
    baseCents: 0,
    addedCents: 0,
    expiresAt: 0,
  };
  const cashlessUiState = {
    busy: false,
    sessionOpen: false,
    revalueLimitCents: CASHLESS_DEFAULT_LIMIT_CENTS,
    timerId: null,
  };

  let manualStatusMessage = '';
  let manualStatusIsError = false;
  let manualStatusUntil = 0;

  function translate(text) {
    if (window.uiI18n && typeof window.uiI18n.translate === 'function') {
      return window.uiI18n.translate(text);
    }
    return text;
  }

  function formatCentsToDisplay(cents) {
    const safeCents = Number.isFinite(cents) ? Math.max(0, cents) : 0;
    return (safeCents / 100).toFixed(2);
  }

  function normalizeCashlessStatus(payload) {
    if (payload && payload.cashless) {
      return {
        online: !!payload.cashless.online,
        session_open: !!payload.cashless.session_open,
        credit_cents: Number(payload.cashless.credit_cents) || 0,
        revalue_limit_cents: Number(payload.cashless.revalue_limit_cents) || 0,
        revalue_status: Number(payload.cashless.revalue_status) || 0,
        approved_revalue_cents: Number(payload.cashless.approved_revalue_cents) || 0,
        approved_price_cents: Number(payload.cashless.approved_price_cents) || 0,
        state: Number(payload.cashless.state) || 0,
        last_response_code: Number(payload.cashless.last_response_code) || 0,
      };
    }

    if (payload && payload.mdb) {
      return {
        online: !!payload.mdb.cashless_online,
        session_open: !!payload.mdb.cashless_session_open,
        credit_cents: Number(payload.mdb.cashless_credit_cents) || 0,
        revalue_limit_cents: Number(payload.mdb.cashless_revalue_limit_cents) || 0,
        revalue_status: Number(payload.mdb.cashless_revalue_status) || 0,
        approved_revalue_cents: Number(payload.mdb.cashless_approved_revalue_cents) || 0,
        approved_price_cents: Number(payload.mdb.cashless_approved_price_cents) || 0,
        state: Number(payload.mdb.cashless_state) || 0,
        last_response_code: Number(payload.mdb.cashless_last_response) || 0,
      };
    }

    return null;
  }

  async function fetchCashlessStatus() {
    let response;
    let payload;

    response = await fetch('/api/mdb/cashless/status', { cache: 'no-store' });
    if (response.ok) {
      payload = await response.json();
      return normalizeCashlessStatus(payload);
    }

    response = await fetch('/status', { cache: 'no-store' });
    if (!response.ok) {
      throw new Error('HTTP ' + response.status);
    }

    payload = await response.json();
    return normalizeCashlessStatus(payload);
  }

  function setFieldCents(fieldId, cents) {
    const field = document.getElementById(fieldId);
    if (!field) {
      return;
    }

    const safeCents = Number.isFinite(cents) ? Math.max(0, Math.round(cents)) : 0;
    field.value = formatCentsToDisplay(safeCents);
    field.dataset.cents = String(safeCents);
  }

  function getFieldCents(fieldId) {
    const field = document.getElementById(fieldId);
    if (!field || !field.dataset) {
      return 0;
    }

    const parsed = parseInt(field.dataset.cents || '0', 10);
    return Number.isFinite(parsed) ? Math.max(0, parsed) : 0;
  }

  function clearPendingRevalue() {
    pendingRevalue.baseCents = 0;
    pendingRevalue.addedCents = 0;
    pendingRevalue.expiresAt = 0;
  }

  function setPendingRevalue(baseCents, addedCents) {
    pendingRevalue.baseCents = Math.max(0, Math.round(baseCents || 0));
    pendingRevalue.addedCents = Math.max(0, Math.round(addedCents || 0));
    pendingRevalue.expiresAt = Date.now() + 15000;
  }

  function hasPendingRevalue() {
    return pendingRevalue.addedCents > 0 && pendingRevalue.expiresAt > Date.now();
  }

  function getDisplayedCreditCents(actualCents, revalueStatus) {
    const safeActual = Math.max(0, Math.round(actualCents || 0));

    if (revalueStatus === 4) {
      clearPendingRevalue();
      return safeActual;
    }

    if (!hasPendingRevalue()) {
      clearPendingRevalue();
      return safeActual;
    }

    const optimisticTarget = pendingRevalue.baseCents + pendingRevalue.addedCents;
    if (safeActual >= optimisticTarget) {
      clearPendingRevalue();
      return safeActual;
    }

    return Math.max(safeActual, optimisticTarget);
  }

  function revalueStatusToString(statusCode) {
    switch (statusCode) {
      case 1: return 'request_pending';
      case 2: return 'in_progress';
      case 3: return 'approved';
      case 4: return 'denied';
      default: return 'idle';
    }
  }

  function setManualStatus(message, isError, holdMs) {
    manualStatusMessage = message || '';
    manualStatusIsError = !!isError;
    manualStatusUntil = Date.now() + (holdMs || 0);
  }

  function clampRechargeAmount() {
    const amountField = document.getElementById('cashless_revalue_amount');
    const limitCents = Math.max(1, cashlessUiState.revalueLimitCents || CASHLESS_DEFAULT_LIMIT_CENTS);
    const limitAmount = Math.max(0.01, limitCents / 100);

    if (!amountField) {
      return limitCents;
    }

    let amountValue = Number(String(amountField.value || '0').replace(',', '.'));
    if (!Number.isFinite(amountValue)) {
      amountValue = Math.min(10, limitAmount);
    }
    if (amountValue < 0.01) {
      amountValue = 0.01;
    }
    if (amountValue > limitAmount) {
      amountValue = limitAmount;
    }

    amountField.min = '0.01';
    amountField.step = '0.01';
    amountField.max = limitAmount.toFixed(2);
    amountField.value = amountValue.toFixed(2);
    return Math.round(amountValue * 100);
  }

  function updateRevalueLimit(cashless) {
    const limitCents = Number(cashless && cashless.revalue_limit_cents) || 0;
    cashlessUiState.revalueLimitCents = limitCents > 0 ? limitCents : CASHLESS_DEFAULT_LIMIT_CENTS;
    setFieldCents('cashless_revalue_limit', limitCents);
    clampRechargeAmount();
  }

  function renderCashlessStatus(cashless) {
    const statusBox = document.getElementById('cashless_revalue_status');
    if (!statusBox) {
      return;
    }

    if (manualStatusMessage && manualStatusUntil > Date.now()) {
      statusBox.innerHTML = '<div class="result" style="border-left-color:' + (manualStatusIsError ? '#e74c3c' : '#2ecc71') + '">' + manualStatusMessage + '</div>';
      return;
    }

    manualStatusMessage = '';
    manualStatusIsError = false;
    manualStatusUntil = 0;

    if (!cashless) {
      statusBox.innerHTML = '';
      return;
    }

    const parts = [];
    parts.push(cashless.online ? 'cashless_online' : translate('Lettore cashless offline'));
    parts.push('session=' + (cashless.session_open ? 'open' : 'idle'));
    parts.push('revalue=' + revalueStatusToString(cashless.revalue_status));

    if (cashless.approved_revalue_cents > 0) {
      parts.push('approved=' + formatCentsToDisplay(cashless.approved_revalue_cents) + ' EUR');
    }

    if (cashless.revalue_limit_cents > 0) {
      parts.push('limit=' + formatCentsToDisplay(cashless.revalue_limit_cents) + ' EUR');
    }

    statusBox.innerHTML = '<div class="result">' + parts.join(' | ') + '</div>';
  }

  function applyCashlessAvailability(isReady) {
    const limitButton = document.getElementById('cashless_revalue_limit_btn');
    const rechargeButton = document.getElementById('cashless_revalue_btn');
    const amountField = document.getElementById('cashless_revalue_amount');
    const disabled = !isReady || cashlessUiState.busy;

    if (limitButton) {
      limitButton.disabled = disabled;
    }
    if (rechargeButton) {
      rechargeButton.disabled = disabled;
      rechargeButton.textContent = cashlessUiState.busy ? '⏳' : translate('🔄 Ricarica');
    }
    if (amountField) {
      amountField.disabled = disabled;
    }
  }

  function createCashlessRow(labelText, controlBuilder) {
    const row = document.createElement('div');
    row.className = 'test-item';

    const label = document.createElement('span');
    label.className = 'test-label';
    label.textContent = translate(labelText);
    row.appendChild(label);

    const controls = document.createElement('div');
    controls.className = 'test-controls';
    controlBuilder(controls);
    row.appendChild(controls);

    return row;
  }

  function ensureCashlessRevaluePanel() {
    const mdbSection = document.getElementById('section_mdb');
    if (!mdbSection || document.getElementById('cashless_revalue_panel')) {
      return;
    }

    const title = document.createElement('h3');
    title.className = 'side-title';
    title.style.fontSize = '18px';
    title.style.marginTop = '16px';
    title.textContent = translate('🔄 Ricarica');
    mdbSection.appendChild(title);

    const panel = document.createElement('div');
    panel.id = 'cashless_revalue_panel';

    panel.appendChild(createCashlessRow('Credito attuale', function (controls) {
      const currentField = document.createElement('input');
      currentField.type = 'text';
      currentField.id = 'cashless_current_credit';
      currentField.readOnly = true;
      currentField.style.width = '120px';
      currentField.value = '0.00';
      currentField.dataset.cents = '0';
      controls.appendChild(currentField);

      const suffix = document.createElement('span');
      suffix.textContent = 'EUR';
      controls.appendChild(suffix);
    }));

    panel.appendChild(createCashlessRow('Limite ricarica', function (controls) {
      const limitField = document.createElement('input');
      limitField.type = 'text';
      limitField.id = 'cashless_revalue_limit';
      limitField.readOnly = true;
      limitField.style.width = '120px';
      limitField.value = '0.00';
      limitField.dataset.cents = '0';
      controls.appendChild(limitField);

      const suffix = document.createElement('span');
      suffix.textContent = 'EUR';
      controls.appendChild(suffix);

      const limitButton = document.createElement('button');
      limitButton.type = 'button';
      limitButton.id = 'cashless_revalue_limit_btn';
      limitButton.textContent = translate('Leggi limite');
      controls.appendChild(limitButton);
    }));

    panel.appendChild(createCashlessRow('Importo ricarica', function (controls) {
      const amountField = document.createElement('input');
      amountField.type = 'number';
      amountField.id = 'cashless_revalue_amount';
      amountField.min = '0.01';
      amountField.step = '0.01';
      amountField.value = '1.00';
      amountField.style.width = '120px';
      controls.appendChild(amountField);

      const suffix = document.createElement('span');
      suffix.textContent = 'EUR';
      controls.appendChild(suffix);

      const rechargeButton = document.createElement('button');
      rechargeButton.type = 'button';
      rechargeButton.id = 'cashless_revalue_btn';
      rechargeButton.textContent = translate('🔄 Ricarica');
      controls.appendChild(rechargeButton);
    }));

    const statusBox = document.createElement('div');
    statusBox.id = 'cashless_revalue_status';
    statusBox.className = 'status-box';
    panel.appendChild(statusBox);

    mdbSection.appendChild(panel);

    if (window.uiI18n && typeof window.uiI18n.apply === 'function') {
      window.uiI18n.apply(panel);
    }

    const limitButton = document.getElementById('cashless_revalue_limit_btn');
    if (limitButton) {
      limitButton.addEventListener('click', requestCashlessRevalueLimit);
    }

    const amountField = document.getElementById('cashless_revalue_amount');
    if (amountField) {
      amountField.addEventListener('change', clampRechargeAmount);
      amountField.addEventListener('input', clampRechargeAmount);
    }

    const rechargeButton = document.getElementById('cashless_revalue_btn');
    if (rechargeButton) {
      rechargeButton.addEventListener('click', submitCashlessRevalue);
    }
  }

  async function refreshCashlessRevaluePanel() {
    try {
      ensureCashlessRevaluePanel();

      const cashless = await fetchCashlessStatus();
      const isReady = !!(cashless && cashless.online && cashless.session_open);
      const creditCents = cashless ? cashless.credit_cents : 0;
      const displayedCreditCents = getDisplayedCreditCents(creditCents, cashless ? cashless.revalue_status : 0);

      setFieldCents('cashless_current_credit', displayedCreditCents);
      if (cashless) {
        updateRevalueLimit(cashless);
      } else {
        cashlessUiState.revalueLimitCents = CASHLESS_DEFAULT_LIMIT_CENTS;
        setFieldCents('cashless_revalue_limit', 0);
        clampRechargeAmount();
      }

      cashlessUiState.sessionOpen = isReady;
      applyCashlessAvailability(isReady);
      renderCashlessStatus(cashless);
    } catch (_error) {
      cashlessUiState.sessionOpen = false;
      cashlessUiState.revalueLimitCents = CASHLESS_DEFAULT_LIMIT_CENTS;
      clearPendingRevalue();
      setFieldCents('cashless_current_credit', 0);
      setFieldCents('cashless_revalue_limit', 0);
      clampRechargeAmount();
      applyCashlessAvailability(false);
      renderCashlessStatus(null);
    }
  }

  async function postCashlessRevalueLimitRequest() {
    try {
      const response = await fetch('/api/mdb/cashless/revalue_limit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
      });
      const payload = await response.json().catch(function () { return {}; });

      if (!response.ok || (payload && payload.status === 'error')) {
        setManualStatus(payload.message || payload.error || ('HTTP ' + response.status), true, 5000);
        renderCashlessStatus(null);
        return false;
      }

      setManualStatus(payload.message || 'revalue_limit_requested', false, 4000);
      renderCashlessStatus(null);
      return true;
    } catch (error) {
      setManualStatus((error && error.message) ? error.message : 'revalue_limit_failed', true, 5000);
      renderCashlessStatus(null);
      return false;
    }
  }

  function delay(milliseconds) {
    return new Promise(function (resolve) {
      window.setTimeout(resolve, milliseconds);
    });
  }

  async function waitForRevalueLimit() {
    for (let attempt = 0; attempt < CASHLESS_REVALUE_LIMIT_MAX_ATTEMPTS; attempt += 1) {
      const cashless = await fetchCashlessStatus();
      const isReady = !!(cashless && cashless.online && cashless.session_open);

      cashlessUiState.sessionOpen = isReady;
      if (!cashless) {
        break;
      }

      setFieldCents('cashless_current_credit', getDisplayedCreditCents(cashless.credit_cents, cashless.revalue_status));
      updateRevalueLimit(cashless);
      applyCashlessAvailability(isReady);
      renderCashlessStatus(cashless);

      if (!isReady) {
        return null;
      }

      if (cashless.revalue_limit_cents > 0 && cashless.revalue_status !== CASHLESS_REVALUE_STATUS_REQUEST_PENDING) {
        return cashless;
      }

      await delay(CASHLESS_REVALUE_LIMIT_WAIT_MS);
    }

    return null;
  }

  function schedulePostRechargeRefresh(stepIndex) {
    if (stepIndex >= CASHLESS_POST_REFRESH_DELAYS.length) {
      cashlessUiState.busy = false;
      applyCashlessAvailability(cashlessUiState.sessionOpen);
      return;
    }

    window.setTimeout(async function () {
      await refreshCashlessRevaluePanel();
      if (!cashlessUiState.sessionOpen) {
        cashlessUiState.busy = false;
        applyCashlessAvailability(false);
        return;
      }
      schedulePostRechargeRefresh(stepIndex + 1);
    }, CASHLESS_POST_REFRESH_DELAYS[stepIndex]);
  }

  async function requestCashlessRevalueLimit() {
    if (cashlessUiState.busy) {
      return;
    }

    await refreshCashlessRevaluePanel();
    if (!cashlessUiState.sessionOpen) {
      return;
    }

    cashlessUiState.busy = true;
    applyCashlessAvailability(true);

    try {
      const limitRequested = await postCashlessRevalueLimitRequest();
      if (!limitRequested) {
        return;
      }

      const limitStatus = await waitForRevalueLimit();
      if (!limitStatus) {
        setManualStatus('revalue_limit_unavailable', true, 5000);
        renderCashlessStatus(null);
      }
    } finally {
      cashlessUiState.busy = false;
      applyCashlessAvailability(cashlessUiState.sessionOpen);
    }
  }

  async function submitCashlessRevalue() {
    if (cashlessUiState.busy) {
      return;
    }

    await refreshCashlessRevaluePanel();
    if (!cashlessUiState.sessionOpen) {
      return;
    }

    cashlessUiState.busy = true;
    applyCashlessAvailability(true);

    try {
      const limitRequested = await postCashlessRevalueLimitRequest();
      if (!limitRequested) {
        cashlessUiState.busy = false;
        applyCashlessAvailability(cashlessUiState.sessionOpen);
        return;
      }

      const limitStatus = await waitForRevalueLimit();
      if (!limitStatus) {
        cashlessUiState.busy = false;
        applyCashlessAvailability(cashlessUiState.sessionOpen);
        return;
      }

      const amountCents = clampRechargeAmount();
      if (!(amountCents > 0)) {
        setManualStatus(translate('Importo non valido'), true, 5000);
        renderCashlessStatus(null);
        cashlessUiState.busy = false;
        applyCashlessAvailability(cashlessUiState.sessionOpen);
        return;
      }

      const currentCents = getFieldCents('cashless_current_credit');
      setPendingRevalue(currentCents, amountCents);
      setFieldCents('cashless_current_credit', currentCents + amountCents);

      const response = await fetch('/api/mdb/cashless/revalue', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ amount_cents: amountCents }),
      });
      const payload = await response.json().catch(function () { return {}; });

      if (!response.ok || (payload && payload.status === 'error')) {
        clearPendingRevalue();
        setFieldCents('cashless_current_credit', currentCents);
        setManualStatus(payload.message || payload.error || ('HTTP ' + response.status), true, 5000);
        renderCashlessStatus(null);
        cashlessUiState.busy = false;
        applyCashlessAvailability(cashlessUiState.sessionOpen);
        return;
      }

      setManualStatus(payload.message || 'revalue_requested', false, 5000);
      renderCashlessStatus(null);
      schedulePostRechargeRefresh(0);
    } catch (error) {
      clearPendingRevalue();
      setManualStatus((error && error.message) ? error.message : 'revalue_failed', true, 5000);
      renderCashlessStatus(null);
      cashlessUiState.busy = false;
      applyCashlessAvailability(cashlessUiState.sessionOpen);
    }
  }

  document.addEventListener('DOMContentLoaded', function () {
    ensureCashlessRevaluePanel();
    refreshCashlessRevaluePanel();
    if (!cashlessUiState.timerId) {
      cashlessUiState.timerId = window.setInterval(refreshCashlessRevaluePanel, CASHLESS_POLL_MS);
    }
  });
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
