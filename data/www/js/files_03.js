function fmStorage() {
  const sw = document.getElementById("fm_storage_sw");
  return sw && sw.checked ? "sdcard" : "spiffs";
}

function fmSetStatus(text) {
  const el = document.getElementById("fm_status");
  if (el) {
    el.textContent = text || "";
  }
}

function esc(value) {
  return String(value || "").replace(/[&<>"]/g, (c) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
  }[c]));
}

function fmSelected() {
  return [...document.querySelectorAll(".fm_cb:checked")].map((b) => decodeURIComponent(b.value));
}

function fmToggleAll() {
  const boxes = [...document.querySelectorAll(".fm_cb")];
  if (!boxes.length) {
    return;
  }
  const allChecked = boxes.every((b) => b.checked);
  boxes.forEach((b) => {
    b.checked = !allChecked;
  });
}

async function fmList() {
  fmSetStatus("{{030}}");
  try {
    const r = await fetch(`/api/files/list?storage=${encodeURIComponent(fmStorage())}`);
    const j = await r.json();
    if (!r.ok) {
      fmSetStatus(`{{031}} ${j.error || r.status}`);
      return;
    }

    const tb = document.getElementById("fm_list");
    if (!tb) {
      return;
    }
    tb.innerHTML = "";

    (j.files || []).forEach((f) => {
      const encodedName = encodeURIComponent(f.name || "");
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td><input class="fm_cb" type="checkbox" value="${encodedName}"></td>
        <td>${esc(f.name)}</td>
        <td class="num">${Number(f.size || 0)}</td>
        <td>
          <button onclick="fmView('${encodedName}')">{{032}}</button>
          <button onclick="fmDownload('${encodedName}')">{{033}}</button>
          <button class="danger" onclick="fmDelete('${encodedName}')">{{034}}</button>
        </td>
      `;
      tb.appendChild(tr);
    });

    const usedBytes = Number(j.used_bytes || 0);
    const totalBytes = Number(j.total_bytes || 0);
    const usedKb = (usedBytes / 1024).toFixed(3);
    const totalKb = totalBytes > 0 ? (totalBytes / 1024).toFixed(3) : "n/d";
    const totalFiles = (j.files || []).length;
    fmSetStatus(`{{035}} ${totalFiles} {{036}} ${usedKb} / ${totalKb} {{037}}`);
  } catch (e) {
    fmSetStatus(`{{038}} ${e}`);
  }
}

async function fmUpload() {
  const inp = document.getElementById("fm_file");
  if (!inp || !inp.files || !inp.files.length) {
    fmSetStatus("{{039}}");
    return;
  }

  const f = inp.files[0];
  fmSetStatus(`{{040}} ${f.name}...`);

  try {
    const r = await fetch(`/api/files/upload?storage=${encodeURIComponent(fmStorage())}&name=${encodeURIComponent(f.name)}`, {
      method: "POST",
      headers: { "Content-Type": "application/octet-stream" },
      body: f,
    });
    const t = await r.text();
    fmSetStatus(r.ok ? `{{041}} ${f.name}` : `{{042}} ${t}`);
    if (r.ok) {
      fmList();
    }
  } catch (e) {
    fmSetStatus(`{{043}} ${e}`);
  }
}

async function fmDelete(encName) {
  const name = decodeURIComponent(encName);
  if (!confirm(`{{044}} ${name}?`)) {
    return;
  }

  fmSetStatus(`{{045}} ${name}...`);
  try {
    const r = await fetch("/api/files/delete", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ storage: fmStorage(), name }),
    });
    const t = await r.text();
    fmSetStatus(r.ok ? `{{046}} ${name}` : `{{047}} ${t}`);
    if (r.ok) {
      fmList();
    }
  } catch (e) {
    fmSetStatus(`{{047}} ${e}`);
  }
}

function fmDownload(encName) {
  const name = decodeURIComponent(encName);
  const url = `/api/files/download?storage=${encodeURIComponent(fmStorage())}&name=${encodeURIComponent(name)}`;
  window.open(url, "_blank");
}

async function fmView(encName) {
  const name = decodeURIComponent(encName);
  const url = `/api/files/download?storage=${encodeURIComponent(fmStorage())}&name=${encodeURIComponent(name)}`;
  fmSetStatus(`{{048}} ${name}...`);

  try {
    const r = await fetch(url);
    if (!r.ok) {
      fmSetStatus(`{{049}} ${r.status}`);
      return;
    }

    const raw = await r.text();
    let shown = raw;
    const lower = name.toLowerCase();
    if (lower.endsWith(".json") || lower.endsWith(".jsn")) {
      try {
        shown = JSON.stringify(JSON.parse(raw), null, 2);
      } catch (_e) {
      }
    }

    const wrap = document.getElementById("fm_view_wrap");
    const title = document.getElementById("fm_view_title");
    const content = document.getElementById("fm_view_content");
    if (title) {
      title.textContent = `📄 ${name}`;
    }
    if (content) {
      content.textContent = shown;
    }
    if (wrap) {
      wrap.style.display = "block";
    }
    fmSetStatus(`{{050}} ${name}`);
  } catch (e) {
    fmSetStatus(`{{049}} ${e}`);
  }
}

function fmCloseView() {
  const wrap = document.getElementById("fm_view_wrap");
  const content = document.getElementById("fm_view_content");
  const title = document.getElementById("fm_view_title");

  if (content) {
    content.textContent = "";
  }
  if (title) {
    title.textContent = "{{051}}";
  }
  if (wrap) {
    wrap.style.display = "none";
  }
}

function fmDownloadSelected() {
  const names = fmSelected();
  if (!names.length) {
    fmSetStatus("{{052}}");
    return;
  }

  names.forEach((n, i) => {
    setTimeout(() => fmDownload(encodeURIComponent(n)), i * 250);
  });
  fmSetStatus(`{{053}} ${names.length}`);
}

async function fmDeleteSelected() {
  const names = fmSelected();
  if (!names.length) {
    fmSetStatus("{{052}}");
    return;
  }
  if (!confirm(`{{044}} ${names.length} {{054}}`)) {
    return;
  }

  let ok = 0;
  let ko = 0;
  for (const name of names) {
    try {
      const r = await fetch("/api/files/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ storage: fmStorage(), name }),
      });
      if (r.ok) {
        ok += 1;
      } else {
        ko += 1;
      }
    } catch (_e) {
      ko += 1;
    }
  }

  fmSetStatus(`{{055}} ${ok} {{056}} ${ko}`);
  fmList();
}

function fmUpdateCopyBtn() {
  const btn = document.getElementById("btn_copy_to");
  if (!btn) {
    return;
  }
  btn.textContent = fmStorage() === "sdcard" ? "{{057}}" : "{{058}}";
}

async function fmCopyToOther() {
  const from = fmStorage();
  const to = from === "sdcard" ? "spiffs" : "sdcard";
  fmSetStatus(`{{059}} ${from} {{060}} ${to}...`);

  try {
    const r = await fetch("/api/files/copy", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ from, to }),
    });
    const j = await r.json();
    if (r.ok) {
      fmSetStatus(`{{061}} ${j.copied}, {{062}} ${j.failed}`);
    } else {
      fmSetStatus(`{{063}} ${j.error || r.status}`);
    }
  } catch (e) {
    fmSetStatus(`{{063}} ${e}`);
  }
}

window.addEventListener("load", () => {
  fmList();
  fmUpdateCopyBtn();
});
