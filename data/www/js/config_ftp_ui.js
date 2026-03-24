(function () {
  function ensureFtpSection() {
    if (document.getElementById('ftp_en')) {
      return;
    }

    const serverPasswordInput = document.getElementById('server_password');
    const serverSection = serverPasswordInput ? serverPasswordInput.closest('.section') : null;
    if (!serverSection || !serverSection.parentNode) {
      return;
    }

    const ftpSection = document.createElement('div');
    ftpSection.className = 'section collapsed';
    ftpSection.innerHTML = "<h2 tabindex='0'>🗂️ Agente FTP<span class='section-toggle-icon'>▸</span></h2><div class='sw-row'><label class='switch'><input type='checkbox' id='ftp_en' name='ftp_en'><span class='slider'></span></label><span>FTP abilitato</span></div><div class='form-group indent'><label>Server FTP</label><input type='text' id='ftp_server' name='ftp_server' placeholder='ftp.example.com:21'></div><div class='form-group indent'><label>Utente FTP</label><input type='text' id='ftp_user' name='ftp_user' placeholder='utente'></div><div class='form-group indent'><label>Password FTP</label><input type='password' id='ftp_password' name='ftp_password' placeholder='password'></div><div class='form-group indent'><label>Path FTP</label><input type='text' id='ftp_path' name='ftp_path' placeholder='/remote/path'></div>";

    serverSection.insertAdjacentElement('afterend', ftpSection);

    if (window.uiI18n && typeof window.uiI18n.apply === 'function') {
      window.uiI18n.apply(ftpSection);
    }
    if (typeof window.initCollapsibleSections === 'function') {
      window.initCollapsibleSections();
    }
  }

  function setFtpValues(cfg) {
    if (!cfg || !cfg.ftp) {
      return;
    }
    const ftp = cfg.ftp;
    const en = document.getElementById('ftp_en');
    const server = document.getElementById('ftp_server');
    const user = document.getElementById('ftp_user');
    const password = document.getElementById('ftp_password');
    const path = document.getElementById('ftp_path');

    if (en) en.checked = !!ftp.en;
    if (server) server.value = (typeof ftp.server === 'string') ? ftp.server : '';
    if (user) user.value = (typeof ftp.user === 'string') ? ftp.user : '';
    if (password) password.value = (typeof ftp.password === 'string') ? ftp.password : '';
    if (path) path.value = (typeof ftp.path === 'string') ? ftp.path : '';
  }

  function wrapLoadConfig() {
    if (window.__ftp_load_wrapped) {
      return;
    }
    if (typeof window.loadConfig !== 'function') {
      setTimeout(wrapLoadConfig, 150);
      return;
    }

    const originalLoadConfig = window.loadConfig;
    window.loadConfig = async function () {
      await originalLoadConfig.apply(this, arguments);
      ensureFtpSection();
      try {
        const response = await fetch('/api/config', { cache: 'no-store' });
        if (response.ok) {
          const cfg = await response.json();
          setFtpValues(cfg);
        }
      } catch (err) {
        console.warn('ftp config load failed', err);
      }
    };

    window.__ftp_load_wrapped = true;
  }

  function wrapFetchSaveConfig() {
    if (window.__ftp_fetch_wrapped) {
      return;
    }

    const originalFetch = window.fetch.bind(window);
    window.fetch = async function (input, init) {
      try {
        const url = (typeof input === 'string') ? input : (input && input.url ? input.url : '');
        if (url.indexOf('/api/config/save') >= 0 && init && init.method === 'POST' && typeof init.body === 'string') {
          const payload = JSON.parse(init.body);
          payload.ftp = {
            en: !!(document.getElementById('ftp_en') && document.getElementById('ftp_en').checked),
            server: document.getElementById('ftp_server') ? document.getElementById('ftp_server').value : '',
            user: document.getElementById('ftp_user') ? document.getElementById('ftp_user').value : '',
            password: document.getElementById('ftp_password') ? document.getElementById('ftp_password').value : '',
            path: document.getElementById('ftp_path') ? document.getElementById('ftp_path').value : ''
          };
          init.body = JSON.stringify(payload);
        }
      } catch (err) {
        console.warn('ftp config save patch failed', err);
      }
      return originalFetch(input, init);
    };

    window.__ftp_fetch_wrapped = true;
  }

  ensureFtpSection();
  wrapLoadConfig();
  wrapFetchSaveConfig();

  window.addEventListener('load', function () {
    ensureFtpSection();
  });
})();
