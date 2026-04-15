(function () {
  function normalizeText(value) {
    return String(value || '').toLowerCase().replace(/\s+/g, ' ').trim();
  }

  function labelTextOf(node) {
    if (!node) return '';
    var label = node.querySelector('label');
    if (label) return normalizeText(label.textContent);
    return normalizeText(node.textContent);
  }

  function findIdentitySection() {
    var sections = document.querySelectorAll('.section');
    for (var i = 0; i < sections.length; i++) {
      var h2 = sections[i].querySelector('h2');
      if (!h2) continue;
      var t = normalizeText(h2.textContent);
      if (t.indexOf('Configurazione Dispositivo') !== -1 || t.indexOf('identita dispositivo') !== -1) {
        return sections[i];
      }
    }
    return null;
  }

  function findNodeByLabel(section, needle) {
    var nodes = section.querySelectorAll('.form-group, .sw-row');
    var wanted = normalizeText(needle);
    for (var i = 0; i < nodes.length; i++) {
      var t = labelTextOf(nodes[i]);
      if (t.indexOf(wanted) !== -1) return nodes[i];
    }
    return null;
  }

  function toFormGroup(node) {
    if (!node) return null;
    if (node.classList && node.classList.contains('form-group')) {
      return node;
    }
    var wrapper = document.createElement('div');
    wrapper.className = 'form-group';
    wrapper.appendChild(node);
    return wrapper;
  }

  function createRow(leftNode, rightNode) {
    var row = document.createElement('div');
    row.className = 'form-row';

    if (leftNode) row.appendChild(toFormGroup(leftNode));

    if (rightNode) {
      row.appendChild(toFormGroup(rightNode));
    } else {
      var spacer = document.createElement('div');
      spacer.className = 'form-group';
      spacer.style.visibility = 'hidden';
      spacer.innerHTML = '&nbsp;';
      row.appendChild(spacer);
    }

    return row;
  }

  function applyIdentityLayout() {
    var section = findIdentitySection();
    if (!section || section.dataset.identityLayoutApplied === '1') {
      return;
    }

    var panelLang = findNodeByLabel(section, 'lingua pannello');
    var backendLang = findNodeByLabel(section, 'lingua backend');
    var ads = findNodeByLabel(section, 'attiva ads');
    var numPrograms = findNodeByLabel(section, 'numero programmi');
    var latitude = findNodeByLabel(section, 'latitudine');
    var longitude = findNodeByLabel(section, 'longitudine');

    if (!panelLang || !backendLang || !ads || !numPrograms || !latitude || !longitude) {
      return;
    }

    var anchor = numPrograms.parentElement;
    if (!anchor || anchor === section) {
      anchor = section;
    }

    var grid = document.createElement('div');
    grid.id = 'cfg_identity_layout_grid';
    grid.appendChild(createRow(panelLang, backendLang));
    grid.appendChild(createRow(ads, numPrograms));
    grid.appendChild(createRow(latitude, longitude));

    section.insertBefore(grid, anchor.nextSibling);
    section.dataset.identityLayoutApplied = '1';
  }

  function init() {
    applyIdentityLayout();
    window.setTimeout(applyIdentityLayout, 200);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
