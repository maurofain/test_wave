/* Global fetch wrapper: injects Authorization */
(function () {
  if (window.__auth_wrapped) {
    return;
  }

  window.__auth_wrapped = true;
  const originalFetch = window.fetch.bind(window);

  window.setAuthToken = function (token) {
    if (token) {
      localStorage.setItem('httpservices_token', token);
    } else {
      localStorage.removeItem('httpservices_token');
    }
  };

  window.getAuthToken = function () {
    return localStorage.getItem('httpservices_token');
  };

  window.clearAuthToken = function () {
    localStorage.removeItem('httpservices_token');
  };

  window.fetch = function (input, init) {
    try {
      const token = window.getAuthToken();
      if (token) {
        init = init || {};
        if (!init.headers) {
          init.headers = { Authorization: 'Bearer ' + token };
        } else if (init.headers instanceof Headers) {
          if (!init.headers.get('Authorization')) {
            init.headers.set('Authorization', 'Bearer ' + token);
          }
        } else if (Array.isArray(init.headers)) {
          let hasAuth = false;
          for (const header of init.headers) {
            if (header[0].toLowerCase() === 'authorization') {
              hasAuth = true;
              break;
            }
          }
          if (!hasAuth) {
            init.headers.push(['Authorization', 'Bearer ' + token]);
          }
        } else if (typeof init.headers === 'object') {
          if (!init.headers.Authorization && !init.headers.authorization) {
            init.headers.Authorization = 'Bearer ' + token;
          }
        }
      }
    } catch (error) {
      console.warn('auth wrapper failed', error);
    }

    return originalFetch(input, init);
  };

  window.goProtectedPath = function (path) {
    window.location.href = path;
    return false;
  };

  (function updateHeaderClock() {
    const tick = function () {
      const el = document.getElementById('hdr_clock');
      if (el) {
        el.textContent = new Date().toTimeString().slice(0, 8);
      }
    };

    tick();
    setInterval(tick, 1000);
  })();

  (function loadBuildInfo() {
    const el = document.getElementById('build_info');
    if (!el) {
      return;
    }

    fetch('/api/version', { cache: 'no-store' })
      .then(function (response) {
        return response.ok ? response.json() : null;
      })
      .then(function (data) {
        if (!data) {
          return;
        }
        const ts = data.build_timestamp || data.date || '';
        el.textContent = 'v' + (data.version || '?') + (ts ? ' (' + ts + ')' : '');
      })
      .catch(function () {
        el.textContent = 'v?';
      });
  })();

  (function loadRunMode() {
    fetch('/status', { cache: 'no-store' })
      .then(function (response) {
        return response.json();
      })
      .then(function (status) {
        const el = document.getElementById('hdr_run_mode');
        if (el && status && status.partition_running) {
          el.textContent = status.partition_running.toUpperCase();
        }
      })
      .catch(function () {});
  })();
})();
