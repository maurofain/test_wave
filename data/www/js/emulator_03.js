(function () {
  if (window.__ui_i18n_ready) {
    return;
  }

  window.__ui_i18n_ready = true;
  const lang = 'it';
  const table = {} || {};
  const skipTags = { SCRIPT: 1, STYLE: 1, NOSCRIPT: 1 };

  function mapText(text) {
    if (!text) {
      return text;
    }
    return table[text] !== undefined && table[text] !== null ? String(table[text]) : text;
  }

  function applyNode(node) {
    if (!node) {
      return;
    }

    if (node.nodeType === Node.TEXT_NODE) {
      const parent = node.parentElement;
      if (parent && skipTags[parent.tagName]) {
        return;
      }
      const value = node.nodeValue;
      if (!value) {
        return;
      }
      const trimmed = value.trim();
      if (!trimmed) {
        return;
      }
      const translated = mapText(trimmed);
      if (translated !== trimmed) {
        node.nodeValue = value.replace(trimmed, translated);
      }
      return;
    }

    if (node.nodeType !== Node.ELEMENT_NODE || skipTags[node.tagName]) {
      return;
    }

    if (node.hasAttribute && node.hasAttribute('data-i18n')) {
      const key = node.getAttribute('data-i18n');
      if (key) {
        const translated = mapText(key);
        if (translated && translated !== key) {
          node.textContent = translated;
        }
      }
    }

    const attrs = ['placeholder', 'title', 'aria-label', 'value'];
    for (const attr of attrs) {
      if (node.hasAttribute && node.hasAttribute(attr)) {
        const oldValue = node.getAttribute(attr);
        const newValue = mapText(oldValue);
        if (newValue !== oldValue) {
          node.setAttribute(attr, newValue);
        }
      }
    }

    for (const child of node.childNodes) {
      applyNode(child);
    }
  }

  function apply(root) {
    if (root) {
      applyNode(root);
    }
  }

  window.uiI18n = {
    language: lang,
    table: table,
    apply: apply,
    translate: mapText,
  };

  document.addEventListener('DOMContentLoaded', function () {
    apply(document.body);
    const observer = new MutationObserver(function (mutations) {
      for (const mutation of mutations) {
        for (const node of mutation.addedNodes) {
          applyNode(node);
        }
      }
    });
    observer.observe(document.body, { subtree: true, childList: true });
  });
})();
