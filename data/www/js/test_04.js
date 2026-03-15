(function () {
  document.addEventListener("DOMContentLoaded", function () {
    document.querySelectorAll("{{036}}").forEach(function (h) {
      h.tabIndex = 0;
      h.addEventListener("click", function () {
        const section = this.parentElement;
        section.classList.toggle("collapsed");
        const icon = this.querySelector(".section-toggle-icon");
        if (icon) {
          icon.innerText = section.classList.contains("collapsed") ? "▸" : "▾";
        }
      });
    });
  });

  if (typeof window.dispatchHardwareCommand !== "function") {
    window.dispatchHardwareCommand = function (type, payload) {
      const detail = {
        type,
        payload: payload || {},
        timestamp: Date.now(),
      };
      window.dispatchEvent(new CustomEvent("emulator:hardware-command", { detail }));
      console.log("{{050}}", detail);
    };
  }

  if (typeof window.runTest !== "function") {
    window.runTest = async function (test, params = {}) {
      try {
        const r = await fetch(`/api/test/${test}`, {
          method: "POST",
          body: JSON.stringify(params),
          headers: { "Content-Type": "application/json" },
        });
        return await r.json();
      } catch (e) {
        console.error("{{067}}", e);
        return { error: "fallback" };
      }
    };
  }

  if (typeof window.runScannerCommand !== "function") {
    window.runScannerCommand = async function (cmd) {
      if (window.dispatchHardwareCommand && (cmd === "scanner_on" || cmd === "scanner_off")) {
        window.dispatchHardwareCommand(cmd, {});
      }
      try {
        const r = await fetch(`/api/test/${cmd}`, { method: "POST" });
        const res = await r.json().catch(() => ({}));
        const el = document.getElementById("scanner_status");
        if (el) {
          el.textContent = `${r.ok ? "✅" : "❌"} ${res.message || res.error || `HTTP ${r.status}`}`;
        }
      } catch (e) {
        console.error(e);
      }
    };
  }
})();
