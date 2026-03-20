(function () {
  function clampByte(value, fallbackValue) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return fallbackValue;
    if (n < 0) return 0;
    if (n > 255) return 255;
    return n;
  }

  function clampFlashCount(value) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return 5;
    if (n < 1) return 1;
    if (n > 20) return 20;
    return n;
  }

  function clampLedCount(value) {
    var n = parseInt(value, 10);
    if (isNaN(n)) return 1;
    if (n < 1) return 1;
    if (n > 1024) return 1024;
    return n;
  }

  function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(function (v) {
      var s = clampByte(v, 0).toString(16);
      return s.length === 1 ? '0' + s : s;
    }).join('');
  }

  function hexToRgb(hex) {
    var value = String(hex || '').trim();
    if (!/^#[0-9a-fA-F]{6}$/.test(value)) {
      return { r: 0, g: 0, b: 0 };
    }
    return {
      r: parseInt(value.slice(1, 3), 16),
      g: parseInt(value.slice(3, 5), 16),
      b: parseInt(value.slice(5, 7), 16)
    };
  }

  function ensureLedSection() {
    if (document.getElementById('cfg_led_count')) {
      return;
    }

    var container = document.querySelector('.container');
    if (!container) {
      return;
    }

    var section = document.createElement('div');
    section.className = 'section';
    section.innerHTML =
      '<h2>Striscia LED</h2>' +
      '<div class="form-row">' +
        '<div class="form-group">' +
          '<label style="display:flex;align-items:center;gap:10px;">' +
            '<input type="checkbox" id="cfg_led_enabled"> Abilita striscia LED' +
          '</label>' +
        '</div>' +
        '<div class="form-group">' +
          '<label for="cfg_led_count">Numero LED</label>' +
          '<input type="text" id="cfg_led_count" value="1" inputmode="numeric">' +
        '</div>' +
      '</div>' +
      '<div class="form-row">' +
        '<div class="form-group">' +
          '<label for="cfg_led_run_color">Colore progressione (verde)</label>' +
          '<input type="color" id="cfg_led_run_color" value="#006400">' +
        '</div>' +
        '<div class="form-group">' +
          '<label for="cfg_led_prefine_color">Colore sezione porpora</label>' +
          '<input type="color" id="cfg_led_prefine_color" value="#400040">' +
        '</div>' +
      '</div>' +
      '<div class="form-row">' +
        '<div class="form-group">' +
          '<label for="cfg_led_standby_color">Colore standby</label>' +
          '<input type="color" id="cfg_led_standby_color" value="#000030">' +
        '</div>' +
        '<div class="form-group">' +
          '<label for="cfg_led_flash_color">Colore flash fine programma</label>' +
          '<input type="color" id="cfg_led_flash_color" value="#ffffff">' +
        '</div>' +
      '</div>' +
      '<div class="form-row">' +
        '<div class="form-group">' +
          '<label for="cfg_led_flash_count">Numero flash fine programma</label>' +
          '<input type="text" id="cfg_led_flash_count" value="5" inputmode="numeric">' +
        '</div>' +
      '</div>';

    function getDirectChildUnderContainer(node) {
      var cur = node;
      while (cur && cur.parentElement && cur.parentElement !== container) {
        cur = cur.parentElement;
      }
      return (cur && cur.parentElement === container) ? cur : null;
    }

    function normalizeText(value) {
      return String(value || '')
        .toLowerCase()
        .replace(/\s+/g, ' ')
        .trim();
    }

    function findSectionByTitle(needles) {
      var sections = container.querySelectorAll('.section');
      for (var si = 0; si < sections.length; si++) {
        var h2 = sections[si].querySelector('h2');
        if (!h2) continue;
        var title = normalizeText(h2.textContent);
        for (var ni = 0; ni < needles.length; ni++) {
          if (title.indexOf(needles[ni]) !== -1) {
            return getDirectChildUnderContainer(sections[si]);
          }
        }
      }
      return null;
    }

    function findCommandAnchor() {
      var buttons = container.querySelectorAll('button');
      for (var bi = 0; bi < buttons.length; bi++) {
        var txt = String(buttons[bi].textContent || '').toLowerCase();
        if (txt.indexOf('salva configurazione') !== -1 ||
            txt.indexOf('backup') !== -1 ||
            txt.indexOf('aggiorna dati') !== -1 ||
            txt.indexOf('reset fabbrica') !== -1) {
          var direct = getDirectChildUnderContainer(buttons[bi]);
          if (direct) return direct;
        }
      }
      return null;
    }

    function moveCommandsToEnd() {
      var anchor = findCommandAnchor();
      if (anchor && anchor.parentElement === container) {
        container.appendChild(anchor);
      }
    }

    var displayAnchor = findSectionByTitle(['display']);
    if (displayAnchor) {
      var nextAfterDisplay = displayAnchor.nextElementSibling;
      if (nextAfterDisplay) {
        container.insertBefore(section, nextAfterDisplay);
      } else {
        container.appendChild(section);
      }
      moveCommandsToEnd();
      return;
    }

    var commandAnchor = findCommandAnchor();

    if (commandAnchor) {
      container.insertBefore(section, commandAnchor);
      moveCommandsToEnd();
      return;
    }

    var firstDirectSection = null;
    for (var i = 0; i < container.children.length; i++) {
      var child = container.children[i];
      if (child && child.classList && child.classList.contains('section')) {
        firstDirectSection = child;
        break;
      }
    }

    if (firstDirectSection) {
      var next = firstDirectSection.nextElementSibling;
      if (next) {
        container.insertBefore(section, next);
      } else {
        container.appendChild(section);
      }
    } else {
      container.appendChild(section);
    }
    moveCommandsToEnd();
  }

  function applyConfigToSection(config) {
    var sensors = (config && typeof config.sensors === 'object') ? config.sensors : {};

    var ledEnabled = document.getElementById('cfg_led_enabled');
    var ledCount = document.getElementById('cfg_led_count');
    var runColor = document.getElementById('cfg_led_run_color');
    var prefineColor = document.getElementById('cfg_led_prefine_color');
    var standbyColor = document.getElementById('cfg_led_standby_color');
    var flashColor = document.getElementById('cfg_led_flash_color');
    var flashCount = document.getElementById('cfg_led_flash_count');

    if (!ledEnabled || !ledCount || !runColor || !prefineColor || !standbyColor || !flashColor || !flashCount) {
      return;
    }

    ledEnabled.checked = !!sensors.led;
    ledCount.value = String(clampLedCount(sensors.led_n));
    runColor.value = rgbToHex(sensors.led_run_r, sensors.led_run_g, sensors.led_run_b);
    prefineColor.value = rgbToHex(sensors.led_prefine_r, sensors.led_prefine_g, sensors.led_prefine_b);
    standbyColor.value = rgbToHex(sensors.led_standby_r, sensors.led_standby_g, sensors.led_standby_b);
    flashColor.value = rgbToHex(sensors.led_flash_r, sensors.led_flash_g, sensors.led_flash_b);
    flashCount.value = String(clampFlashCount(sensors.led_flash_count));
  }

  function installInputGuards() {
    var ledCount = document.getElementById('cfg_led_count');
    if (ledCount && ledCount.dataset.ledGuard !== '1') {
      ledCount.dataset.ledGuard = '1';
      ledCount.addEventListener('input', function () {
        ledCount.value = String(clampLedCount(ledCount.value));
      });
    }

    var flashCount = document.getElementById('cfg_led_flash_count');
    if (flashCount && flashCount.dataset.ledGuard !== '1') {
      flashCount.dataset.ledGuard = '1';
      flashCount.addEventListener('input', function () {
        flashCount.value = String(clampFlashCount(flashCount.value));
      });
    }
  }

  function installConfigSaveInterceptor() {
    if (window.__config_led_save_hook_installed) {
      return;
    }
    window.__config_led_save_hook_installed = true;

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
          var sensors = (payload && payload.sensors && typeof payload.sensors === 'object') ? payload.sensors : {};

          var ledEnabled = document.getElementById('cfg_led_enabled');
          var ledCount = document.getElementById('cfg_led_count');
          var runColor = document.getElementById('cfg_led_run_color');
          var prefineColor = document.getElementById('cfg_led_prefine_color');
          var standbyColor = document.getElementById('cfg_led_standby_color');
          var flashColor = document.getElementById('cfg_led_flash_color');
          var flashCount = document.getElementById('cfg_led_flash_count');

          if (ledEnabled && ledCount && runColor && prefineColor && standbyColor && flashColor && flashCount) {
            var runRgb = hexToRgb(runColor.value);
            var prefineRgb = hexToRgb(prefineColor.value);
            var standbyRgb = hexToRgb(standbyColor.value);
            var flashRgb = hexToRgb(flashColor.value);

            sensors.led = !!ledEnabled.checked;
            sensors.led_n = clampLedCount(ledCount.value);

            sensors.led_run_r = runRgb.r;
            sensors.led_run_g = runRgb.g;
            sensors.led_run_b = runRgb.b;

            sensors.led_prefine_r = prefineRgb.r;
            sensors.led_prefine_g = prefineRgb.g;
            sensors.led_prefine_b = prefineRgb.b;

            sensors.led_standby_r = standbyRgb.r;
            sensors.led_standby_g = standbyRgb.g;
            sensors.led_standby_b = standbyRgb.b;

            sensors.led_flash_r = flashRgb.r;
            sensors.led_flash_g = flashRgb.g;
            sensors.led_flash_b = flashRgb.b;
            sensors.led_flash_count = clampFlashCount(flashCount.value);

            payload.sensors = sensors;
            init.body = JSON.stringify(payload);
          }
        }
      } catch (e) {
      }

      return originalFetch(input, init);
    };
  }

  async function loadLedConfigSection() {
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
    ensureLedSection();
    installInputGuards();
    installConfigSaveInterceptor();
    loadLedConfigSection();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
