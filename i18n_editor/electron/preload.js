const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("editorApi", {
  init: () => ipcRenderer.invoke("editor:init"),
  getScopeData: (scope) => ipcRenderer.invoke("editor:get-scope-data", scope),
  updateTranslation: (payload) => ipcRenderer.invoke("editor:update-translation", payload),
  addKey: (payload) => ipcRenderer.invoke("editor:add-key", payload),
  search: (searchText) => ipcRenderer.invoke("editor:search", searchText),
  save: (opts) => ipcRenderer.invoke("editor:save", opts),
  setTranslatorEnabled: (enabled) => ipcRenderer.invoke("editor:set-translator-enabled", enabled),
  translate: (payload) => ipcRenderer.invoke("editor:translate", payload),
});
