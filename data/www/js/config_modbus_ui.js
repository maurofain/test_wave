(function () {
  function clampModules(value) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return 1;
    if (n < 1) return 1;
    if (n > 8) return 8;
    return n;
  }

  function getModbusEnabledElement() {
    return document.getElementById('cfg_modbus_enabled');
  }

  function getModbusModulesElement() {
    return document.getElementById('cfg_modbus_modules');
  }

  function ensureModbusSection() {
    if (document.getElementById('cfg_modbus_enabled')) {
      return;
    }

    var container = document.querySelector('.container');
    if (!container) {
      return;
    }

    var section = document.createElement('div');
    section.className = 'section';
    section.innerHTML =
      '<h2>Modbus</h2>' +
      '<div class="form-row">' +
        '<div class="form-group">' +
          '<label style="display:flex;align-items:center;gap:10px;">' +
            '<input type="checkbox" id="cfg_modbus_enabled"> Abilita Modbus' +
          '</label>' +
        '</div>' +
        '<div class="form-group">' +
          '<label for="cfg_modbus_modules">Moduli Waveshare installati</label>' +
          '<input type="text" id="cfg_modbus_modules" value="1" inputmode="numeric">' +
        '</div>' +
      '</div>';

    var firstSection = container.querySelector('.section');
    if (firstSection) {
      container.insertBefore(section, firstSection.nextSibling);
    } else {
      container.appendChild(section);
    }
  }

  function applyConfigToSection(config) {
    var enabledEl = getModbusEnabledElement();
    var modulesEl = getModbusModulesElement();
    if (!enabledEl || !modulesEl) {
      return;
    }

    var modbus = (config && typeof config.modbus === 'object') ? config.modbus : {};
    enabledEl.checked = !!modbus.enabled;

    var relayCount = parseInt(modbus.relay_count, 10);
    var inputCount = parseInt(modbus.input_count, 10);
    var maxCount = Math.max(isNaN(relayCount) ? 8 : relayCount, isNaN(inputCount) ? 8 : inputCount);
    var modules = clampModules(Math.ceil(maxCount / 8));
    modulesEl.value = String(modules);
  }

  function installConfigSaveInterceptor() {
    if (window.__config_modbus_save_hook_installed) {
      return;
    }
    window.__config_modbus_save_hook_installed = true;

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
          var enabledEl = getModbusEnabledElement();
          var modulesEl = getModbusModulesElement();

          if (enabledEl && modulesEl && payload && typeof payload === 'object') {
            var modules = clampModules(modulesEl.value);
            var outputs = modules * 8;

            var currentModbus = (payload.modbus && typeof payload.modbus === 'object') ? payload.modbus : {};
            payload.modbus = {
              enabled: !!enabledEl.checked,
              slave_id: (typeof currentModbus.slave_id === 'number') ? currentModbus.slave_id : 1,
              poll_ms: (typeof currentModbus.poll_ms === 'number') ? currentModbus.poll_ms : 100,
              timeout_ms: (typeof currentModbus.timeout_ms === 'number') ? currentModbus.timeout_ms : 200,
              retries: (typeof currentModbus.retries === 'number') ? currentModbus.retries : 2,
              relay_start: (typeof currentModbus.relay_start === 'number') ? currentModbus.relay_start : 0,
              relay_count: outputs,
              input_start: (typeof currentModbus.input_start === 'number') ? currentModbus.input_start : 0,
              input_count: outputs
            };

            init.body = JSON.stringify(payload);
          }
        }
      } catch (e) {
      }

      return originalFetch(input, init);
    };
  }

  function installModulesInputGuard() {
    var modulesEl = getModbusModulesElement();
    if (!modulesEl || modulesEl.dataset.modbusGuard === '1') {
      return;
    }
    modulesEl.dataset.modbusGuard = '1';

    modulesEl.addEventListener('input', function () {
      modulesEl.value = String(clampModules(modulesEl.value));
    });
  }

  async function loadModbusConfigSection() {
    try {
      var response = await fetch('/api/config', { cache: 'no-store' });
      if (!response.ok) {
        return;
      }
      var config = await response.json();
      applyConfigToSection(config);
    } catch (e) {
    }
  }

  function init() {
    ensureModbusSection();
    installConfigSaveInterceptor();
    installModulesInputGuard();
    loadModbusConfigSection();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
