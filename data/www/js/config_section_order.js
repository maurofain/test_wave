(function () {
  function normalizeText(value) {
    return String(value || '')
      .toLowerCase()
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .replace(/\s+/g, ' ')
      .trim();
  }

  function sectionTitle(section) {
    if (!section) return '';
    var h2 = section.querySelector(':scope > h2') || section.querySelector('h2');
    return h2 ? normalizeText(h2.textContent) : '';
  }

  function matches(title, needles) {
    for (var i = 0; i < needles.length; i++) {
      if (title.indexOf(needles[i]) !== -1) return true;
    }
    return false;
  }

  function findSectionByNeedles(sections, needles) {
    for (var i = 0; i < sections.length; i++) {
      var sec = sections[i];
      if (!sec || sec.dataset.cfgOrdered === '1') continue;
      var title = sectionTitle(sec);
      if (matches(title, needles)) return sec;
    }
    return null;
  }

  function findAnySectionByNeedles(sections, needles) {
    for (var i = 0; i < sections.length; i++) {
      var sec = sections[i];
      if (!sec) continue;
      var title = sectionTitle(sec);
      if (matches(title, needles)) return sec;
    }
    return null;
  }

  function setSectionHeadingText(section, text) {
    if (!section) return;
    var h2 = section.querySelector(':scope > h2') || section.querySelector('h2');
    if (!h2) return;
    var icon = h2.querySelector('.section-toggle-icon');
    if (icon) {
      icon.remove();
    }
    h2.textContent = text;
    if (icon) {
      h2.appendChild(icon);
    }
  }

  function normalizeRequestedTitles(sections) {
    var ledSection = findAnySectionByNeedles(sections, ['striscia led', 'led strip']);
    setSectionHeadingText(ledSection, '💡 Striscia LED');

    var modbusSection = findAnySectionByNeedles(sections, ['modbus']);
    setSectionHeadingText(modbusSection, '🧩 Modbus');

    var serialSection = findAnySectionByNeedles(sections, ['porte seriali']);
    setSectionHeadingText(serialSection, '🔌 Porte Seriali');
  }

  function getContainer() {
    return document.querySelector('.container');
  }

  function getConfigForm() {
    return document.getElementById('configForm');
  }

  function isCommandSection(section) {
    if (!section) return false;
    return !!section.querySelector('button[type="submit"]');
  }

  function getTargetSections(container, form) {
    var inForm = [];
    if (form) {
      inForm = Array.prototype.slice.call(form.children).filter(function (node) {
        return node && node.classList && node.classList.contains('section') && node.querySelector('h2');
      });
    }

    var inContainer = [];
    if (container) {
      inContainer = Array.prototype.slice.call(container.children).filter(function (node) {
        return node && node.classList && node.classList.contains('section') && node.querySelector('h2');
      });
    }

    var all = inForm.slice();
    for (var i = 0; i < inContainer.length; i++) {
      if (all.indexOf(inContainer[i]) === -1) all.push(inContainer[i]);
    }
    return all;
  }

  function arraysEqualByRef(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) return false;
    }
    return true;
  }

  function areCommandSectionsLast(form) {
    var sections = Array.prototype.slice.call(form.children).filter(function (node) {
      return node && node.classList && node.classList.contains('section');
    });
    var firstCommandIndex = -1;
    for (var i = 0; i < sections.length; i++) {
      if (isCommandSection(sections[i])) {
        firstCommandIndex = i;
        break;
      }
    }
    if (firstCommandIndex === -1) return true;
    for (var j = firstCommandIndex; j < sections.length; j++) {
      if (!isCommandSection(sections[j])) return false;
    }
    return true;
  }

  function orderConfigSections() {
    var container = getContainer();
    var form = getConfigForm();
    if (!container || !form) return;

    var sections = getTargetSections(container, form);

    if (!sections.length) return;

    normalizeRequestedTitles(sections);

    var patterns = [
      ['configurazione dispositivo', 'identita dispositivo', 'identita'],
      ['password boot/emulatore', 'password emulatore'],
      ['periferiche hardware', 'periferiche'],
      ['ethernet'],
      ['wifi sta', 'wifi'],
      ['server remoto'],
      ['ntp'],
      ['logging remoto', 'logging'],
      ['timeouts', 'timeout'],
      ['display'],
      ['striscia led', 'led strip'],
      ['porte seriali'],
      ['cctalk'],
      ['modbus'],
      ['gpio ausiliario']
    ];

    var ordered = [];
    for (var i = 0; i < patterns.length; i++) {
      var sec = findSectionByNeedles(sections, patterns[i]);
      if (sec) {
        sec.dataset.cfgOrdered = '1';
        ordered.push(sec);
      }
    }

    if (!ordered.length) return;

    var contentSections = sections.filter(function (node) {
      return node && !isCommandSection(node);
    });

    var remaining = contentSections.filter(function (node) {
      return ordered.indexOf(node) === -1;
    });
    var finalOrder = ordered.concat(remaining);

    var currentOrder = Array.prototype.slice.call(form.children).filter(function (node) {
      return node && node.classList && node.classList.contains('section') && node.querySelector('h2') && !isCommandSection(node);
    });

    var orderAlreadyOk = arraysEqualByRef(currentOrder, finalOrder);
    var commandsAlreadyLast = areCommandSectionsLast(form);
    if (orderAlreadyOk && commandsAlreadyLast) {
      for (var x = 0; x < ordered.length; x++) {
        ordered[x].dataset.cfgOrdered = '0';
      }
      return;
    }

    var commandSections = Array.prototype.slice.call(form.children).filter(function (node) {
      return node && node.classList && node.classList.contains('section') && isCommandSection(node);
    });
    var firstCommandSection = commandSections.length ? commandSections[0] : null;

    for (var k = 0; k < finalOrder.length; k++) {
      if (firstCommandSection) {
        form.insertBefore(finalOrder[k], firstCommandSection);
      } else {
        form.appendChild(finalOrder[k]);
      }
    }

    for (var m = 0; m < ordered.length; m++) {
      ordered[m].dataset.cfgOrdered = '0';
    }

    for (var n = 0; n < commandSections.length; n++) {
      form.appendChild(commandSections[n]);
    }
  }

  var reorderTimer = null;
  function queueOrder() {
    if (reorderTimer) window.clearTimeout(reorderTimer);
    reorderTimer = window.setTimeout(function () {
      orderConfigSections();
      reorderTimer = null;
    }, 40);
  }

  function scheduleOrderAttempts() {
    queueOrder();
    window.setTimeout(queueOrder, 100);
    window.setTimeout(queueOrder, 400);
    window.setTimeout(queueOrder, 900);
    window.setTimeout(queueOrder, 1500);
    window.setTimeout(queueOrder, 2500);
  }

  function installObserver() {
    if (window.__config_section_order_observer_installed) return;
    var container = getContainer();
    if (!container) return;
    window.__config_section_order_observer_installed = true;
    var observer = new MutationObserver(function () {
      queueOrder();
    });
    observer.observe(container, { childList: true, subtree: true });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function () {
      scheduleOrderAttempts();
      installObserver();
    });
  } else {
    scheduleOrderAttempts();
    installObserver();
  }

  window.addEventListener('load', function () {
    scheduleOrderAttempts();
    installObserver();
  });
})();
