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
