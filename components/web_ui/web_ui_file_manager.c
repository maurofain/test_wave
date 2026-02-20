#include "web_ui_internal.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "sd_card.h"
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "WEB_UI_FILES"

static bool storage_to_base_path(const char *storage, const char **base_path)
{
    if (!storage || !base_path) {
        return false;
    }
    if (strcmp(storage, "spiffs") == 0) {
        *base_path = "/spiffs";
        return true;
    }
    if (strcmp(storage, "sd") == 0 || strcmp(storage, "sdcard") == 0) {
        *base_path = "/sdcard";
        return true;
    }
    return false;
}

static bool is_safe_filename(const char *name)
{
    if (!name || name[0] == '\0') {
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    for (const char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            return false;
        }
    }
    return true;
}

static esp_err_t get_query_value(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    if (!req || !key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    char *query = malloc(qlen + 1);
    if (!query) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = httpd_req_get_url_query_str(req, query, qlen + 1);
    if (ret == ESP_OK) {
        ret = httpd_query_key_value(query, key, out, out_len);
    }

    free(query);
    return ret;
}

esp_err_t files_page_handler(httpd_req_t *req)
{
    const char *extra_style =
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}"
        "table{width:100%;border-collapse:collapse;margin-top:10px}"
        "th,td{padding:8px;border-bottom:1px solid #eee;text-align:left}"
        "th.num,td.num{text-align:right;font-family:monospace;font-variant-numeric:tabular-nums}"
        ".row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}"
        ".row-right{margin-left:auto;display:flex;gap:10px;align-items:center}"
        "input,select,button{padding:8px;border-radius:6px;border:1px solid #ccc}"
        "button{background:#3498db;color:#fff;border:none;font-weight:bold}"
        "button.danger{background:#c0392b}"
        ".status{margin-top:10px;font-family:monospace;white-space:pre-wrap}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "File Manager", extra_style, true);

    const char *body =
        "<div class='container'>"
        "<div class='section'><h2>📁 Mini File Manager (root only)</h2>"
        "<div class='row'>"
        "<label>Storage:</label>"
        "<select id='fm_storage' onchange='fmList()'><option value='spiffs'>SPIFFS</option><option value='sdcard'>SD Card</option></select>"
        "<button onclick='fmList()'>Aggiorna elenco</button>"
        "</div>"
        "<div class='row' style='margin-top:10px'>"
        "<input id='fm_file' type='file' />"
        "<button onclick='fmUpload()'>Carica file</button>"
        "</div>"
        "<div class='row' style='margin-top:10px'>"
        "<button onclick='fmToggleAll()'>Seleziona/Deseleziona tutto</button>"
        "<div class='row-right'>"
        "<button onclick='fmDownloadSelected()'>Scarica tutto</button>"
        "<button class='danger' onclick='fmDeleteSelected()'>Elimina tutto</button>"
        "</div>"
        "</div>"
        "<div id='fm_status' class='status'></div>"
        "<table><thead><tr><th>Sel</th><th>Nome</th><th class='num'>Dimensione (byte)</th><th>Azione</th></tr></thead><tbody id='fm_list'></tbody></table>"
        "</div></div>"
        "<script>"
        "function fmStorage(){return document.getElementById('fm_storage').value;}"
        "function fmSetStatus(t){document.getElementById('fm_status').textContent=t||'';}"
        "function esc(s){return String(s||'').replace(/[&<>\"]/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[m]));}"
        "function fmSelected(){return [...document.querySelectorAll('.fm_cb:checked')].map(b=>decodeURIComponent(b.value));}"
        "function fmToggleAll(){const boxes=[...document.querySelectorAll('.fm_cb')];if(!boxes.length)return;const all=boxes.every(b=>b.checked);boxes.forEach(b=>b.checked=!all);}"
        "async function fmList(){"
        "  fmSetStatus('Caricamento...');"
        "  try{"
        "    const r=await fetch('/api/files/list?storage='+encodeURIComponent(fmStorage()));"
        "    const j=await r.json();"
        "    if(!r.ok){fmSetStatus('Errore: '+(j.error||r.status));return;}"
        "    const tb=document.getElementById('fm_list'); tb.innerHTML='';"
        "    (j.files||[]).forEach(f=>{"
        "      const tr=document.createElement('tr');"
        "      tr.innerHTML='<td><input class=\"fm_cb\" type=\"checkbox\" value=\"'+encodeURIComponent(f.name)+'\"></td><td>'+esc(f.name)+'</td><td class=\"num\">'+f.size+'</td><td><button onclick=\"fmDownload(\\''+encodeURIComponent(f.name)+'\\')\">Scarica</button> <button class=\"danger\" onclick=\"fmDelete(\\''+encodeURIComponent(f.name)+'\\')\">Elimina</button></td>';"
        "      tb.appendChild(tr);"
        "    });"
        "    const usedBytes=Number(j.used_bytes||0);"
        "    const totalBytes=Number(j.total_bytes||0);"
        "    const usedKb=(usedBytes/1024).toFixed(3);"
        "    const totalKb=totalBytes>0?(totalBytes/1024).toFixed(3):'n/d';"
        "    fmSetStatus('Totale file: '+(j.files?j.files.length:0)+' | Spazio: '+usedKb+' / '+totalKb+' KB');"
        "  }catch(e){fmSetStatus('Errore rete: '+e);}"
        "}"
        "async function fmUpload(){"
        "  const inp=document.getElementById('fm_file');"
        "  if(!inp.files||!inp.files.length){fmSetStatus('Seleziona un file');return;}"
        "  const f=inp.files[0]; fmSetStatus('Upload '+f.name+'...');"
        "  try{"
        "    const r=await fetch('/api/files/upload?storage='+encodeURIComponent(fmStorage())+'&name='+encodeURIComponent(f.name),{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f});"
        "    const t=await r.text();"
        "    fmSetStatus(r.ok?('Upload OK: '+f.name):('Upload KO: '+t));"
        "    if(r.ok) fmList();"
        "  }catch(e){fmSetStatus('Errore upload: '+e);}"
        "}"
        "async function fmDelete(encName){"
        "  const name=decodeURIComponent(encName);"
        "  if(!confirm('Eliminare '+name+'?')) return;"
        "  fmSetStatus('Elimino '+name+'...');"
        "  try{"
        "    const body=JSON.stringify({storage:fmStorage(),name:name});"
        "    const r=await fetch('/api/files/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:body});"
        "    const t=await r.text();"
        "    fmSetStatus(r.ok?('Eliminato: '+name):('Errore delete: '+t));"
        "    if(r.ok) fmList();"
        "  }catch(e){fmSetStatus('Errore delete: '+e);}"
        "}"
        "function fmDownload(encName){"
        "  const name=decodeURIComponent(encName);"
        "  const url='/api/files/download?storage='+encodeURIComponent(fmStorage())+'&name='+encodeURIComponent(name);"
        "  window.open(url,'_blank');"
        "}"
        "function fmDownloadSelected(){"
        "  const names=fmSelected();"
        "  if(!names.length){fmSetStatus('Nessun file selezionato');return;}"
        "  names.forEach((n,i)=>setTimeout(()=>fmDownload(encodeURIComponent(n)),i*250));"
        "  fmSetStatus('Avviati download: '+names.length);"
        "}"
        "async function fmDeleteSelected(){"
        "  const names=fmSelected();"
        "  if(!names.length){fmSetStatus('Nessun file selezionato');return;}"
        "  if(!confirm('Eliminare '+names.length+' file selezionati?')) return;"
        "  let ok=0, ko=0;"
        "  for(const name of names){"
        "    try{"
        "      const body=JSON.stringify({storage:fmStorage(),name:name});"
        "      const r=await fetch('/api/files/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:body});"
        "      if(r.ok) ok++; else ko++;"
        "    }catch(e){ko++;}"
        "  }"
        "  fmSetStatus('Eliminazione completata. OK: '+ok+' KO: '+ko);"
        "  fmList();"
        "}"
        "window.addEventListener('load',fmList);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_files_list_get(httpd_req_t *req)
{
    char storage[16] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK) {
        snprintf(storage, sizeof(storage), "spiffs");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"invalid_storage\"}");
    }

    DIR *dir = opendir(base_path);
    if (!dir) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"cannot_open_storage\"}");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    cJSON_AddStringToObject(root, "storage", storage);
    cJSON_AddItemToObject(root, "files", files);

    uint64_t sum_file_bytes = 0;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        char full_path[320];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, de->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", de->d_name);
        cJSON_AddNumberToObject(item, "size", (double)st.st_size);
        cJSON_AddItemToArray(files, item);
        sum_file_bytes += (uint64_t)st.st_size;
    }
    closedir(dir);

    uint64_t total_bytes = 0;
    uint64_t used_bytes = sum_file_bytes;
    if (strcmp(storage, "spiffs") == 0) {
        size_t total = 0;
        size_t used = 0;
        if (esp_spiffs_info(NULL, &total, &used) == ESP_OK) {
            total_bytes = (uint64_t)total;
            used_bytes = (uint64_t)used;
        }
    } else if (strcmp(storage, "sd") == 0 || strcmp(storage, "sdcard") == 0) {
        uint64_t total_kb = sd_card_get_total_size();
        uint64_t used_kb = sd_card_get_used_size();
        if (total_kb > 0) {
            total_bytes = total_kb * 1024ULL;
            used_bytes = used_kb * 1024ULL;
        }
    }

    cJSON_AddNumberToObject(root, "used_bytes", (double)used_bytes);
    cJSON_AddNumberToObject(root, "total_bytes", (double)total_bytes);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"oom\"}");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr(req, json);
    free(json);
    return ret;
}

esp_err_t api_files_upload_post(httpd_req_t *req)
{
    char storage[16] = {0};
    char name[128] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK ||
        get_query_value(req, "name", name, sizeof(name)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_query");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path) || !is_safe_filename(name)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

    FILE *f = fopen(full_path, "wb");
    if (!f) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "open_failed");
    }

    int remaining = req->content_len;
    char buf[1024];
    while (remaining > 0) {
        int chunk = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int r = httpd_req_recv(req, buf, chunk);
        if (r <= 0) {
            fclose(f);
            remove(full_path);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "recv_failed");
        }
        size_t w = fwrite(buf, 1, (size_t)r, f);
        if (w != (size_t)r) {
            fclose(f);
            remove(full_path);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "write_failed");
        }
        remaining -= r;
    }

    fclose(f);
    return httpd_resp_sendstr(req, "ok");
}

esp_err_t api_files_delete_post(httpd_req_t *req)
{
    char body[256];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_body");
    }
    body[len] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_json");
    }

    const cJSON *storage = cJSON_GetObjectItemCaseSensitive(root, "storage");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(storage) || !cJSON_IsString(name)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_fields");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage->valuestring, &base_path) || !is_safe_filename(name->valuestring)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name->valuestring);
    cJSON_Delete(root);

    if (remove(full_path) != 0) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "delete_failed");
    }

    return httpd_resp_sendstr(req, "ok");
}

esp_err_t api_files_download_get(httpd_req_t *req)
{
    char storage[16] = {0};
    char name[128] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK ||
        get_query_value(req, "name", name, sizeof(name)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_query");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path) || !is_safe_filename(name)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "not_found");
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "open_failed");
    }

    httpd_resp_set_type(req, "application/octet-stream");
    char cd[196];
    snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", name);
    httpd_resp_set_hdr(req, "Content-Disposition", cd);

    char buf[1024];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);

    return httpd_resp_send_chunk(req, NULL, 0);
}
