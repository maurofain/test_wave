(function () {
  function clampSeconds(value) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return 3;
    if (n < 0) return 0;
    if (n > 10) return 10;
    return n;
  }

  function normalizeText(value) {
    return String(value || '').toLowerCase().replace(/\s+/g, ' ').trim();
  }

  function findIdentitySection() {
    var sections = document.querySelectorAll('.section');
    for (var i = 0; i < sections.length; i++) {
      var h2 = sections[i].querySelector('h2');
      if (!h2) continue;
      var t = normalizeText(h2.textContent);
      if (t.indexOf('identità dispositivo') !== -1 || t.indexOf('identita dispositivo') !== -1) {
        return sections[i];
      }
    }
    return null;
  }

  function createFormGroup() {
    var group = document.createElement('div');
    group.className = 'form-group';
    group.id = 'cfg_program_end_message_sec_group';

    var label = document.createElement('label');
    label.setAttribute('for', 'cfg_program_end_message_sec');
    label.textContent = 'Messaggio fine programma (sec, 0=off)';

    var input = document.createElement('input');
    input.type = 'text';
    input.id = 'cfg_program_end_message_sec';
    input.value = '3';
    input.inputMode = 'numeric';

    group.appendChild(label);
    group.appendChild(input);
    return group;
  }

  function ensureField() {
    if (document.getElementById('cfg_program_end_message_sec')) {
      return;
    }

    var section = findIdentitySection();
    if (!section) {
      return;
    }

    var group = createFormGroup();
    var grid = document.getElementById('cfg_identity_layout_grid');

    if (grid) {
      var row = document.createElement('div');
      row.className = 'form-row';
      row.appendChild(group);

      var spacer = document.createElement('div');
      spacer.className = 'form-group';
      spacer.style.visibility = 'hidden';
      spacer.innerHTML = '&nbsp;';
      row.appendChild(spacer);

      grid.appendChild(row);
    } else {
      section.appendChild(group);
    }
  }

  function applyConfigToField(config) {
    var el = document.getElementById('cfg_program_end_message_sec');
    if (!el) return;

    var ui = (config && typeof config.ui === 'object') ? config.ui : {};
    var sec = ui.program_end_message_sec;
    if (typeof sec === 'undefined') sec = ui.program_end_sec;
    el.value = String(clampSeconds(sec));
  }

  function installInputGuard() {
    var el = document.getElementById('cfg_program_end_message_sec');
    if (!el || el.dataset.endSecGuard === '1') return;
    el.dataset.endSecGuard = '1';
    el.addEventListener('input', function () {
      el.value = String(clampSeconds(el.value));
    });
  }

  function installConfigSaveInterceptor() {
    if (window.__config_program_end_sec_save_hook_installed) {
      return;
    }
    window.__config_program_end_sec_save_hook_installed = true;

    var originalFetch = window.fetch.bind(window);
    window.fetch = function (input, init) {
      try {
        var url = '';
        if (typeof input === 'string') {
          url = input;
        } else if (input && typeof input.url === 'string') {
          url = input.url;
        }

        if (url.indexOf('/api/config/save') !== -1 && init && typeof init.body === 'string') {
          var payload = JSON.parse(init.body);
          var el = document.getElementById('cfg_program_end_message_sec');
          if (el && payload && typeof payload === 'object') {
            var ui = (payload.ui && typeof payload.ui === 'object') ? payload.ui : {};
            ui.program_end_message_sec = clampSeconds(el.value);
            payload.ui = ui;
            init.body = JSON.stringify(payload);
          }
        }
      } catch (e) {
      }

      return originalFetch(input, init);
    };
  }

  function refreshFromApi() {
    fetch('/api/config')
      .then(function (r) { return r.json(); })
      .then(function (cfg) { applyConfigToField(cfg); })
      .catch(function () {});
  }

  function init() {
    ensureField();
    installInputGuard();
    installConfigSaveInterceptor();
    refreshFromApi();
    window.setTimeout(function () {
      ensureField();
      installInputGuard();
      refreshFromApi();
    }, 300);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
