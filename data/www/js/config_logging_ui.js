/**
 * config_logging_ui.js - Gestisce i toggle per log selettivi
 * HTTP_SERVICES, LVGL, I/O Expander
 */

(function() {
    'use strict';

    // Array di toggle da gestire
    const LOG_TOGGLES = [
        { id: 'log_http_services', endpoint: '/api/logs/component/HTTP_SERVICES' },
        { id: 'log_lvgl', endpoint: '/api/logs/component/lvgl' },
        { id: 'log_io_exp', endpoint: '/api/logs/component/io_expander' }
    ];

    /**
     * Carica lo stato attuale dei toggle dal backend
     */
    async function loadLogToggleStates() {
        for (const toggle of LOG_TOGGLES) {
            const el = document.getElementById(toggle.id);
            if (!el) continue;

            try {
                // GET /api/logs/component/{COMPONENT}/status
                const statusUrl = toggle.endpoint + '/status';
                const resp = await fetch(statusUrl, { 
                    cache: 'no-store',
                    method: 'GET'
                });
                if (resp.ok) {
                    const data = await resp.json();
                    el.checked = !!data.enabled;
                } else {
                    console.warn(`Failed to load ${toggle.id} status: HTTP ${resp.status}`);
                }
            } catch (err) {
                console.warn(`Failed to load ${toggle.id} status:`, err);
                el.checked = false; // Default: disabilitato se errore
            }
        }
    }

    /**
     * Invia lo stato del toggle al backend
     */
    async function saveLogToggleState(id, enabled) {
        const toggle = LOG_TOGGLES.find(t => t.id === id);
        if (!toggle) return;

        try {
            // POST /api/logs/component/{COMPONENT}
            const resp = await fetch(toggle.endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: !!enabled })
            });

            if (!resp.ok) {
                console.error(`Failed to save ${id}: HTTP ${resp.status}`);
                return false;
            }

            const data = await resp.json();
            return !!data.enabled;
        } catch (err) {
            console.error(`Failed to save ${id}:`, err);
            return false;
        }
    }

    /**
     * Bind event listeners sui toggle
     */
    function initToggleListeners() {
        for (const toggle of LOG_TOGGLES) {
            const el = document.getElementById(toggle.id);
            if (!el) continue;

            el.addEventListener('change', async function() {
                const previous = !this.checked;
                try {
                    const applied = await saveLogToggleState(this.id, this.checked);
                    if (applied !== this.checked) {
                        this.checked = applied;
                    }
                } catch (err) {
                    this.checked = previous;
                    const msg = err && err.message ? err.message : 'Errore aggiornamento toggle';
                    alert(`❌ ${this.id}: ${msg}`);
                }
            });
        }
    }

    // Inizializza al caricamento della pagina
    function init() {
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', function() {
                loadLogToggleStates();
                initToggleListeners();
            });
        } else {
            loadLogToggleStates();
            initToggleListeners();
        }
    }

    init();
})();
