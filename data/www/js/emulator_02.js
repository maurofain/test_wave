(function () {
  const buttons = Array.from(document.querySelectorAll('.prog-btn'));
  const coinButtons = Array.from(document.querySelectorAll('.coin-btn'));
  const relays = Array.from(document.querySelectorAll('.relay'));
  const creditBox = document.getElementById('creditBox');
  const creditEl = document.getElementById('credit');
  const creditLabel = document.getElementById('creditLabel');
  const pauseElapsedEl = document.getElementById('pauseElapsed');
  const msgEl = document.getElementById('msg');
  const gaugePanel = document.getElementById('gaugePanel');
  const gaugeFill = document.getElementById('gfill');
  const gaugeText = document.getElementById('gtext');
  const chromeTimeEl = document.getElementById('chromeTime');
  const chromeLangEl = document.getElementById('chromeLang');
  const stopBtn = document.getElementById('stopButton');
  const stopLabel = document.getElementById('stopLabel');
  const programGrid = document.getElementById('programGrid');
  const programPopup = document.getElementById('programPopup');
  const cardSwitch = document.getElementById('cardSwitch');

  let credit = 0;
  let ecd = 0;
  let vcd = 0;
  let activeProgram = 0;
  let runningProgramName = '';
  let fsmState = 'unknown';
  let runningElapsedMs = 0;
  let runningElapsedSyncAt = 0;
  let pauseElapsedMs = 0;
  let pauseElapsedSyncAt = 0;
  let runningTargetMs = 0;
  let pendingStart = false;
  let preFineActive = false;
  let inactivityMs = 0;
  let splashScreenTimeMs = 0;
  let stopConfirm = false;
  let ecdWarningDismissed = false;
  let programsById = {};
  let programMetaByButton = {};
  let pendingCreditSync = null;
  let programPopupHideTimer = null;
  let configuredProgramCount = 10;

  const STOP_TEXT_DEFAULT = 'STOP';
  const STOP_TEXT_CONFIRM = 'Conferma annullamento';

  function pad2(value) {
    return String(value).padStart(2, '0');
  }

  function updateChromeTimeUi() {
    if (!chromeTimeEl) {
      return;
    }
    const now = new Date();
    chromeTimeEl.textContent =
      pad2(now.getHours()) + ':' + pad2(now.getMinutes()) + ':' + pad2(now.getSeconds());
  }

  function updateChromeLanguageUi() {
    if (!chromeLangEl) {
      return;
    }
    const lang = window.uiI18n && window.uiI18n.language ? window.uiI18n.language : 'it';
    chromeLangEl.textContent = String(lang).slice(0, 2).toUpperCase();
  }

  function escapeHtml(value) {
    return String(value || '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
  }

  function renderQueueMessages(messages) {
    if (!Array.isArray(messages) || messages.length === 0) {
      msgEl.textContent = 'Nessun evento in coda';
      return;
    }

    msgEl.innerHTML = messages
      .map(function (message) {
        return '<div>• ' + escapeHtml(message) + '</div>';
      })
      .join('');
  }

  function applyRelayIndicators(relayItems) {
    if (!Array.isArray(relayItems)) {
      return;
    }

    const byRelay = {};
    relayItems.forEach(function (item) {
      if (item && typeof item.relay_number === 'number') {
        byRelay[item.relay_number] = !!item.status;
      }
    });

    relays.forEach(function (relay) {
      const relayNumber = parseInt(relay.dataset.relay || '0', 10);
      if (!relayNumber) {
        return;
      }
      if (Object.prototype.hasOwnProperty.call(byRelay, relayNumber)) {
        relay.classList.toggle('on', !!byRelay[relayNumber]);
      }
    });
  }

  function formatElapsed(ms) {
    const totalSeconds = Math.max(0, Math.floor(ms / 1000));
    const minutes = String(Math.floor(totalSeconds / 60)).padStart(2, '0');
    const seconds = String(totalSeconds % 60).padStart(2, '0');
    return minutes + ':' + seconds;
  }

  function getShownRunningElapsedMs() {
    if (fsmState === 'running' && runningElapsedSyncAt) {
      return runningElapsedMs + (Date.now() - runningElapsedSyncAt);
    }
    return runningElapsedMs;
  }

  function getShownPauseElapsedMs() {
    if (fsmState === 'paused' && pauseElapsedSyncAt) {
      return pauseElapsedMs + (Date.now() - pauseElapsedSyncAt);
    }
    return pauseElapsedMs;
  }

  function getRemainingMs() {
    if (runningTargetMs <= 0) {
      return 0;
    }
    return Math.max(0, runningTargetMs - getShownRunningElapsedMs());
  }

  function getRunningProgramIdFromName() {
    if (!runningProgramName) {
      return 0;
    }

    const entries = Object.values(programsById);
    for (const entry of entries) {
      if (entry && entry.name === runningProgramName) {
        return Number(entry.program_id) || 0;
      }
    }
    return 0;
  }

  function syncRunningProgramIdentity() {
    if (!activeProgram && runningProgramName) {
      const resolvedId = getRunningProgramIdFromName();
      if (resolvedId > 0) {
        activeProgram = resolvedId;
      }
    }
  }

  function getVisibleProgramButtons() {
    return buttons.filter(function (btn) {
      return btn.dataset.available === '1';
    });
  }

  function updateProgramGridLayout() {
    if (!programGrid) {
      return;
    }

    const ROW_PAD = 6;
    const ROW_GAP = 6;

    const count = getVisibleProgramButtons().length;
    const singleColumn = count > 0 && count <= 5;
    const rows = singleColumn ? Math.max(1, count) : Math.max(1, Math.ceil(count / 2));

    const gridHeight = programGrid.clientHeight || 642;
    let usableHeight = gridHeight - (2 * ROW_PAD) - ((rows - 1) * ROW_GAP);
    if (usableHeight < rows) {
      usableHeight = rows;
    }

    let rowHeight = Math.floor(usableHeight / rows);

    const totalHeight = (rowHeight * rows) + ((rows - 1) * ROW_GAP);
    let yPad = Math.floor((gridHeight - totalHeight) / 2);
    if (yPad < ROW_PAD) {
      yPad = ROW_PAD;
    }

    programGrid.classList.toggle('single-column', singleColumn);
    programGrid.style.setProperty('--emu-program-rows', String(rows));
    programGrid.style.setProperty('--emu-program-row-h', rowHeight + 'px');
    programGrid.style.setProperty('--emu-program-gap-y', ROW_GAP + 'px');
    programGrid.style.paddingTop = yPad + 'px';
    programGrid.style.paddingBottom = yPad + 'px';

    buttons.forEach(function (btn) {
      const id = parseInt(btn.dataset.id || '0', 10);
      const isRightColumn = !singleColumn && id > 0 && (id % 2 === 0);
      btn.classList.toggle('is-right-column', isRightColumn);
    });
  }

  function setProgramButtonContent(btn, id, name) {
    if (!btn) {
      return;
    }

    const programName = name && String(name).trim() ? String(name).trim() : String(id);
    btn.innerHTML =
      '<span class="prog-btn-inner">' +
        '<span class="prog-btn-num-badge">' + String(id) + '</span>' +
        '<span class="prog-btn-name">' + escapeHtml(programName) + '</span>' +
      '</span>';
  }

  function showSelectedProgramPopup(programId) {
    if (!programPopup || !programId) {
      return;
    }

    const program = programMetaByButton[programId];
    const popupText = program && program.name ? program.name : ('Programma ' + programId);
    programPopup.textContent = popupText;
    programPopup.classList.add('is-visible');

    if (programPopupHideTimer) {
      clearTimeout(programPopupHideTimer);
    }

    programPopupHideTimer = setTimeout(function () {
      programPopup.classList.remove('is-visible');
      programPopupHideTimer = null;
    }, 2500);
  }

  function paintProgramButton(btn, state) {
    btn.classList.toggle('is-enabled', !!state.enabled);
    btn.classList.toggle('is-disabled', !state.enabled);
    btn.classList.toggle('is-active', !!state.active);
    btn.classList.toggle('is-paused', !!state.paused);
    btn.classList.toggle('is-prefine', !!state.prefine);
    btn.disabled = !state.enabled;
  }

  function getEcdRemainingSeconds() {
    if (splashScreenTimeMs <= inactivityMs) {
      return 0;
    }
    return Math.max(0, Math.floor((splashScreenTimeMs - inactivityMs) / 1000));
  }

  function isEcdWarningActive() {
    if (fsmState !== 'credit' || ecd <= 0 || ecdWarningDismissed) {
      return false;
    }
    const remainingSeconds = getEcdRemainingSeconds();
    return remainingSeconds > 0 && remainingSeconds <= 60;
  }

  function hasPendingCreditSync() {
    return !!(pendingCreditSync && pendingCreditSync.expiresAt > Date.now());
  }

  function clearPendingCreditSync() {
    pendingCreditSync = null;
  }

  function markPendingCreditSync() {
    pendingCreditSync = {
      creditFloor: Math.max(0, credit),
      ecdFloor: Math.max(0, ecd),
      vcdFloor: Math.max(0, vcd),
      expiresAt: Date.now() + 2000,
    };
  }

  function applySyncedCreditValue(localValue, incomingValue, floorKey) {
    if (!hasPendingCreditSync()) {
      return incomingValue;
    }

    const floor = pendingCreditSync && typeof pendingCreditSync[floorKey] === 'number'
      ? pendingCreditSync[floorKey]
      : 0;

    if (incomingValue < floor) {
      return localValue;
    }

    return incomingValue;
  }

  function finalizePendingCreditSync() {
    if (!pendingCreditSync) {
      return;
    }

    const expired = pendingCreditSync.expiresAt <= Date.now();
    const satisfied =
      credit >= pendingCreditSync.creditFloor &&
      ecd >= pendingCreditSync.ecdFloor &&
      vcd >= pendingCreditSync.vcdFloor;

    if (expired || satisfied) {
      clearPendingCreditSync();
    }
  }

  function updateCreditStateUi() {
    if (!creditBox) {
      return;
    }

    const borderColor = preFineActive ? 'var(--warn)' : 'var(--accent)';
    creditBox.style.borderColor = borderColor;
  }

  function updateGaugeUi() {
    const runningOrPaused = fsmState === 'running' || fsmState === 'paused';
    if (!gaugePanel || !gaugeFill || !gaugeText) {
      return;
    }

    if (!runningOrPaused || runningTargetMs <= 0) {
      gaugePanel.classList.remove('is-visible');
      gaugeFill.style.width = '10%';
      gaugeFill.style.backgroundColor = 'var(--accent)';
      gaugePanel.style.borderColor = 'var(--accent)';
      gaugeText.textContent = '0%';
      gaugeText.style.color = '#000';
      return;
    }

    const remainingMs = getRemainingMs();
    const remainingPct = Math.max(0, Math.min(100, Math.round((remainingMs / runningTargetMs) * 100)));
    let fillPct = 100 - Math.round((remainingMs * 90) / runningTargetMs);
    fillPct = Math.max(10, Math.min(100, fillPct));

    const gaugeColor = preFineActive ? 'var(--warn)' : 'var(--accent)';
    gaugePanel.classList.add('is-visible');
    gaugePanel.style.borderColor = gaugeColor;
    gaugeFill.style.width = fillPct + '%';
    gaugeFill.style.backgroundColor = gaugeColor;
    gaugeText.textContent = remainingPct + '%';
    gaugeText.style.color = preFineActive ? 'var(--text-main)' : '#000';
  }

  function updateStopButtonUi() {
    if (!stopBtn || !stopLabel) {
      return;
    }

    const runningOrPaused = fsmState === 'running' || fsmState === 'paused';
    const ecdWarningActive = isEcdWarningActive();

    stopBtn.classList.remove('is-visible', 'is-warn');
    stopBtn.disabled = false;

    if (runningOrPaused) {
      stopBtn.classList.add('is-visible');
      stopBtn.style.backgroundColor = preFineActive ? 'var(--warn)' : 'var(--danger)';
      stopBtn.style.borderColor = preFineActive ? 'var(--text-main)' : '#ff8080';
      stopLabel.textContent = stopConfirm ? STOP_TEXT_CONFIRM : STOP_TEXT_DEFAULT;
      return;
    }

    stopConfirm = false;

    if (ecdWarningActive) {
      stopBtn.classList.add('is-visible', 'is-warn');
      stopBtn.disabled = true;
      stopLabel.textContent = 'Il credito scadrà tra ' + getEcdRemainingSeconds() + ' secondi\ntocca il numero del credito per continuare';
      return;
    }

    stopBtn.style.backgroundColor = 'var(--danger-dark)';
    stopBtn.style.borderColor = '#ff8080';
    stopLabel.textContent = STOP_TEXT_DEFAULT;
  }

  function sanitizeProgramCount(value, fallbackValue) {
    const parsed = Number(value);
    if (!Number.isFinite(parsed)) {
      return fallbackValue;
    }
    const rounded = Math.floor(parsed);
    if (rounded < 1) {
      return fallbackValue;
    }
    if (rounded > 10) {
      return 10;
    }
    return rounded;
  }

  async function loadConfiguredProgramCount() {
    try {
      const response = await fetch('/api/config');
      if (!response.ok) {
        return configuredProgramCount;
      }
      const payload = await response.json();
      configuredProgramCount = sanitizeProgramCount(payload && payload.n_prg, configuredProgramCount);
      return configuredProgramCount;
    } catch (error) {
      console.warn('config fetch failed', error);
      return configuredProgramCount;
    }
  }

  function updateProgramAvailability() {
    syncRunningProgramIdentity();

    const runningOrPaused = fsmState === 'running' || fsmState === 'paused';
    buttons.forEach(function (btn) {
      const id = parseInt(btn.dataset.id || '0', 10);
      const program = programMetaByButton[id];
      if (!program || btn.dataset.available !== '1') {
        btn.style.display = 'none';
        btn.disabled = true;
        return;
      }

      btn.style.display = '';

      const enabled = runningOrPaused
        ? !!program.enabled
        : !!program.enabled && credit >= (program.price_units || 0);
      const active = runningOrPaused && id === activeProgram;
      const paused = active && fsmState === 'paused';

      paintProgramButton(btn, {
        enabled: enabled,
        active: active,
        paused: paused,
        prefine: runningOrPaused && preFineActive,
      });
    });

    updateProgramGridLayout();
  }

  function render() {
    const runningOrPaused = fsmState === 'running' || fsmState === 'paused';
    const shownRunningMs = getShownRunningElapsedMs();
    const shownPauseMs = getShownPauseElapsedMs();
    const ecdWarningActive = isEcdWarningActive();

    if (creditLabel) {
      creditLabel.textContent = 'Crediti';
    }

    if (creditEl) {
      creditEl.textContent = String(Math.max(0, credit));
    }

    if (pauseElapsedEl) {
      if (fsmState === 'paused' && shownPauseMs > 0) {
        pauseElapsedEl.textContent = 'Pausa: ' + formatElapsed(shownPauseMs);
      } else if (ecdWarningActive) {
        pauseElapsedEl.textContent = 'Il credito scadrà tra ' + getEcdRemainingSeconds() + ' secondi\ntocca il numero del credito per continuare';
      } else {
        pauseElapsedEl.textContent = '';
      }
    }

    updateCreditStateUi();
    updateGaugeUi();
    updateProgramAvailability();
    updateStopButtonUi();
  }

  function syncFromFsm(payload) {
    if (payload && typeof payload.credit_cents === 'number') {
      const nextCredit = Math.max(0, Math.floor(payload.credit_cents));
      credit = applySyncedCreditValue(credit, nextCredit, 'creditFloor');
    }
    if (payload && typeof payload.ecd_coins === 'number') {
      const nextEcd = Math.max(0, Math.floor(payload.ecd_coins));
      ecd = applySyncedCreditValue(ecd, nextEcd, 'ecdFloor');
    }
    if (payload && typeof payload.vcd_coins === 'number') {
      const nextVcd = Math.max(0, Math.floor(payload.vcd_coins));
      vcd = applySyncedCreditValue(vcd, nextVcd, 'vcdFloor');
    }
    if (payload && typeof payload.state === 'string') {
      fsmState = payload.state;
    }
    if (payload && typeof payload.running_elapsed_ms === 'number') {
      runningElapsedMs = Math.max(0, Math.floor(payload.running_elapsed_ms));
      runningElapsedSyncAt = Date.now();
    }
    if (payload && typeof payload.pause_elapsed_ms === 'number') {
      pauseElapsedMs = Math.max(0, Math.floor(payload.pause_elapsed_ms));
      pauseElapsedSyncAt = Date.now();
    }
    if (payload && typeof payload.running_target_ms === 'number') {
      runningTargetMs = Math.max(0, Math.floor(payload.running_target_ms));
    }
    if (payload && typeof payload.pre_fine_ciclo_active === 'boolean') {
      preFineActive = payload.pre_fine_ciclo_active;
    } else {
      preFineActive = false;
    }
    if (payload && typeof payload.inactivity_ms === 'number') {
      inactivityMs = Math.max(0, Math.floor(payload.inactivity_ms));
    }
    if (payload && typeof payload.splash_screen_time_ms === 'number') {
      splashScreenTimeMs = Math.max(0, Math.floor(payload.splash_screen_time_ms));
    }
    if (payload && typeof payload.running_program_name === 'string') {
      runningProgramName = payload.running_program_name;
    }

    if (cardSwitch && vcd === 0) {
      cardSwitch.checked = false;
    }

    if (fsmState === 'running' || fsmState === 'paused') {
      pendingStart = false;
      syncRunningProgramIdentity();
    } else if (!pendingStart) {
      activeProgram = 0;
      runningProgramName = '';
      runningTargetMs = 0;
      runningElapsedSyncAt = 0;
      pauseElapsedSyncAt = 0;
    }

    if (payload && Array.isArray(payload.relays)) {
      applyRelayIndicators(payload.relays);
    }

    finalizePendingCreditSync();
    render();
    renderQueueMessages(payload && payload.messages ? payload.messages : []);
  }

  async function refreshQueueMessages() {
    try {
      const response = await fetch('/api/emulator/fsm/messages');
      if (!response.ok) {
        return;
      }
      const payload = await response.json();
      syncFromFsm(payload);
    } catch (error) {
      console.warn('fsm messages fetch failed', error);
    }
  }

  async function loadProgramsMeta() {
    try {
      await loadConfiguredProgramCount();

      const response = await fetch('/api/programs');
      if (!response.ok) {
        return;
      }

      const payload = await response.json();
      const programs = Array.isArray(payload.programs) ? payload.programs : [];
      programsById = {};
      programMetaByButton = {};

      programs.forEach(function (program) {
        if (!program || typeof program.program_id !== 'number') {
          return;
        }
        programsById[program.program_id] = program;
      });

      buttons.forEach(function (btn) {
        const id = parseInt(btn.dataset.id || '0', 10);
        if (id > configuredProgramCount) {
          btn.dataset.available = '0';
          btn.style.display = 'none';
          btn.disabled = true;
          return;
        }

        const program = programsById[id];
        if (!program) {
          btn.dataset.available = '0';
          btn.style.display = 'none';
          btn.disabled = true;
          return;
        }

        programMetaByButton[id] = program;
        btn.dataset.available = '1';
        btn.dataset.price = String(program.price_units || 0);
        btn.dataset.enabled = program.enabled ? '1' : '0';
        setProgramButtonContent(btn, id, program.name || String(id));
      });

      syncRunningProgramIdentity();
      render();
    } catch (error) {
      console.warn('load programs failed', error);
    }
  }

  function dispatchHardwareCommand(type, payload) {
    const detail = {
      type: type,
      payload: payload,
      timestamp: Date.now(),
    };

    window.dispatchEvent(new CustomEvent('emulator:hardware-command', { detail: detail }));
    console.log('[EMULATOR_CMD]', detail);
  }

  window.dispatchHardwareCommand = dispatchHardwareCommand;

  function updateVirtualRelay(relayNumber, status, duration) {
    fetch('/api/emulator/relay', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        relay_number: relayNumber,
        status: status,
        duration: duration || 0,
      }),
    }).catch(function (error) {
      console.warn('virtual relay update failed', error);
    });
  }

  function applyOptimisticCreditDelta(delta, source) {
    credit = Math.max(0, credit + delta);
    if (source === 'card' || source === 'qr') {
      vcd = Math.max(0, vcd + delta);
    } else {
      ecd = Math.max(0, ecd + delta);
    }
    ecdWarningDismissed = false;
    markPendingCreditSync();
    render();
  }

  function rollbackOptimisticCreditDelta(delta, source) {
    credit = Math.max(0, credit - delta);
    if (source === 'card' || source === 'qr') {
      vcd = Math.max(0, vcd - delta);
    } else {
      ecd = Math.max(0, ecd - delta);
    }
    clearPendingCreditSync();
    render();
  }

  async function requestPauseToggle(programId) {
    const response = await fetch('/api/emulator/program/pause_toggle', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ program_id: programId }),
    });
    const payload = await response.json().catch(function () {
      return {};
    });

    if (!response.ok) {
      throw new Error(payload && payload.error ? payload.error : 'Errore pausa/ripresa: HTTP ' + response.status);
    }

    return payload;
  }

  async function requestProgramStop(programId) {
    const response = await fetch('/api/emulator/program/stop', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ program_id: programId || 0 }),
    });
    const payload = await response.json().catch(function () {
      return {};
    });

    if (!response.ok) {
      throw new Error(payload && payload.error ? payload.error : 'Errore stop: HTTP ' + response.status);
    }

    return payload;
  }

  buttons.forEach(function (btn) {
    btn.addEventListener('click', async function () {
      const programId = parseInt(btn.dataset.id || '0', 10);
      if (!programId || btn.disabled) {
        return;
      }

      if (credit <= 0 && fsmState !== 'running' && fsmState !== 'paused') {
        activeProgram = 0;
        render();
        msgEl.textContent = 'Credito insufficiente';
        return;
      }

      try {
        showSelectedProgramPopup(programId);

        if (activeProgram === programId && (fsmState === 'running' || fsmState === 'paused')) {
          dispatchHardwareCommand('program_pause_toggle', {
            program: programId,
            current_credit: credit,
            state: fsmState,
          });

          await requestPauseToggle(programId);
          stopConfirm = false;
          if (fsmState === 'paused') {
            fsmState = 'running';
            pauseElapsedMs = 0;
            pauseElapsedSyncAt = 0;
            msgEl.textContent = 'Programma ' + programId + ' ripreso';
          } else {
            fsmState = 'paused';
            pauseElapsedSyncAt = Date.now();
            msgEl.textContent = 'Programma ' + programId + ' in pausa';
          }

          render();
          await new Promise(function (resolve) {
            setTimeout(resolve, 250);
          });
          await refreshQueueMessages();
          return;
        }

        dispatchHardwareCommand('program_start', {
          program: programId,
          current_credit: credit,
        });

        const response = await fetch('/api/emulator/program/start', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ program_id: programId }),
        });
        const payload = await response.json().catch(function () {
          return {};
        });
        if (!response.ok) {
          msgEl.textContent = payload && payload.error ? payload.error : 'Errore start programma: HTTP ' + response.status;
          return;
        }

        const newProgram = programMetaByButton[programId];
        const wasRunning = fsmState === 'running' || fsmState === 'paused';
        const shownRunningMs = getShownRunningElapsedMs();

        if (wasRunning) {
          const oldProgram = programMetaByButton[activeProgram];
          const oldPrice = oldProgram && oldProgram.price_units > 0 ? oldProgram.price_units : 1;
          const newPrice = newProgram && newProgram.price_units > 0 ? newProgram.price_units : 1;
          const newDurationMs = newProgram && newProgram.duration_sec ? newProgram.duration_sec * 1000 : 0;
          const remainingOldMs = Math.max(0, runningTargetMs - shownRunningMs);
          runningTargetMs = runningTargetMs > 0 && newDurationMs > 0
            ? shownRunningMs + Math.round((remainingOldMs * newDurationMs * oldPrice) / (runningTargetMs * newPrice))
            : newDurationMs;
        } else {
          runningElapsedMs = 0;
          runningElapsedSyncAt = Date.now();
          runningTargetMs = newProgram && newProgram.duration_sec ? newProgram.duration_sec * 1000 : 0;
        }

        activeProgram = programId;
        runningProgramName = newProgram && newProgram.name ? newProgram.name : '';
        fsmState = 'running';
        pendingStart = true;
        stopConfirm = false;
        render();
        msgEl.textContent = 'Programma ' + programId + ' avviato';

        await new Promise(function (resolve) {
          setTimeout(resolve, 250);
        });
        await refreshQueueMessages();
      } catch (error) {
        msgEl.textContent = error && error.message ? error.message : String(error);
      }
    });
  });

  if (creditBox) {
    creditBox.addEventListener('click', function () {
      ecdWarningDismissed = true;
      render();
    });
  }

  if (cardSwitch) {
    cardSwitch.addEventListener('change', function () {
      msgEl.textContent = 'Tessera ' + (this.checked ? 'ON' : 'OFF');
    });
  }

  if (stopBtn) {
    stopBtn.addEventListener('click', async function () {
      if (stopBtn.disabled) {
        return;
      }

      const programId = activeProgram || getRunningProgramIdFromName();
      if (!programId) {
        return;
      }

      try {
        if (!stopConfirm) {
          stopConfirm = true;
          await requestPauseToggle(programId);
          if (fsmState === 'paused') {
            fsmState = 'running';
            pauseElapsedMs = 0;
            pauseElapsedSyncAt = 0;
          } else {
            fsmState = 'paused';
            pauseElapsedSyncAt = Date.now();
          }
          render();
          msgEl.textContent = 'Premi STOP di nuovo per annullare';
          await refreshQueueMessages();
          return;
        }

        await requestProgramStop(programId);
        stopConfirm = false;
        pendingStart = false;
        activeProgram = 0;
        runningProgramName = '';
        fsmState = 'credit';
        pauseElapsedMs = 0;
        pauseElapsedSyncAt = 0;
        render();
        msgEl.textContent = 'Programma annullato';
        await refreshQueueMessages();
      } catch (error) {
        msgEl.textContent = error && error.message ? error.message : String(error);
      }
    });
  }

  coinButtons.forEach(function (btn) {
    btn.addEventListener('click', async function () {
      const delta = parseInt(btn.dataset.coin || '0', 10);
      const source = btn.dataset.source || 'qr';
      const sourceLabel = { qr: 'QR', card: 'Tessera', cash: 'Monete' }[source] || source;
      if (!delta) {
        return;
      }

      dispatchHardwareCommand('coin_add', {
        value: delta,
        source: source,
        current_credit: credit,
      });

      if (source === 'card' && cardSwitch) {
        cardSwitch.checked = true;
      }

      applyOptimisticCreditDelta(delta, source);
      msgEl.textContent = sourceLabel + ' +' + delta + ' coin';

      try {
        const response = await fetch('/api/emulator/coin', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ coin: delta, source: source }),
        });
        if (!response.ok) {
          rollbackOptimisticCreditDelta(delta, source);
          msgEl.textContent = 'Errore inserimento credito: HTTP ' + response.status;
          return;
        }
        await refreshQueueMessages();
      } catch (error) {
        rollbackOptimisticCreditDelta(delta, source);
        msgEl.textContent = 'Errore credito: ' + (error && error.message ? error.message : error);
      }
    });
  });

  relays.forEach(function (relay) {
    relay.addEventListener('click', async function () {
      const relayNumber = parseInt(relay.dataset.relay || '0', 10);
      const relayLabel = relay.dataset.relayLabel || ('R' + relayNumber);
      const nextState = !relay.classList.contains('on');
      relay.classList.toggle('on', nextState);
      updateVirtualRelay(relayNumber, nextState, 0);
      dispatchHardwareCommand('relay_toggle', {
        relay: relayNumber,
        status: nextState,
      });
      msgEl.textContent = relayLabel + ' ' + (nextState ? 'ON' : 'OFF');
      await refreshQueueMessages();
    });
  });

  updateChromeLanguageUi();
  updateChromeTimeUi();
  loadProgramsMeta();
  refreshQueueMessages();

  (function startCctalkSequence() {
    try {
      const addr = 2;
      if (typeof window.sendCctalkFramesFromApp === 'function') {
        try {
          window.sendCctalkFramesFromApp(addr);
          console.log('[CCTALK] start via app (' + addr + ')');
        } catch (error) {
          console.warn('sendCctalkFramesFromApp failed', error);
        }
      } else if (typeof window.dispatchHardwareCommand === 'function') {
        try {
          window.dispatchHardwareCommand('cctalk_sequence', {
            addr: addr,
            sequence: 'enable_and_start',
          });
          console.log('[CCTALK] start via hardware command');
        } catch (error) {
          console.warn('dispatchHardwareCommand failed', error);
        }
      } else {
        console.warn('No app function to send CCTALK frames found');
      }
    } catch (error) {
      console.warn('CCTALK init failed', error);
    }
  })();

  setInterval(refreshQueueMessages, 700);
  setInterval(render, 200);
  setInterval(updateChromeTimeUi, 1000);
})();
