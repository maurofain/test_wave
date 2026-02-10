# LVGL + Waveshare 7-DSI-TOUCH-A Troubleshooting

Data: 10 febbraio 2026

## Contesto
Il display Waveshare 7-DSI-TOUCH-A (720x1280, MIPI-DSI) viene usato con ESP32-P4 e LVGL. L'interfaccia del progetto e' pensata in orizzontale (landscape), mentre il pannello lavora in verticale (portrait) e non supporta rotazione hardware a 90/270 gradi.

## Problemi riscontrati

### 1) Rotazione 270 gradi con LVGL (software)
- Impostando `sw_rotate = true` e `bsp_display_rotate(disp, LV_DISP_ROTATION_270)` si ottengono:
  - errori `esp_cache_msync(103): invalid addr or null pointer`
  - crash e watchdog
  - artefatti grafici (linee verticali, pixel in movimento)

**Causa:**
La rotazione 90/270 e' solo software. Richiede buffer aggiuntivi e molta RAM. Su questa piattaforma i buffer non sono sufficienti o la memoria non e' stabile per sostenere il rotazione software.

### 2) Rotazione hardware non disponibile
- Con `sw_rotate = false` e `bsp_display_rotate(disp, LV_DISP_ROTATION_270)` la visualizzazione risulta specchiata o corrotta.

**Causa:**
Il BSP supporta rotazione hardware solo a 0/180 gradi. 90/270 richiedono la rotazione software.

### 3) Artefatti con buffer troppo piccolo
- Riducendo troppo `buffer_size` gli aggiornamenti parziali causano tearing e artefatti.

**Causa:**
Buffer troppo ridotto per aggiornare aree ampie in modo consistente.

## Soluzioni applicate

### A) Configurazione stabile (portrait)
- `sw_rotate = false`
- `buffer_size = BSP_LCD_DRAW_BUFF_SIZE`
- `double_buffer = false`
- `buff_dma = true`

**Risultato:**
- Nessun crash
- Display stabile, ma in verticale

### B) Disabilitare il monitor FPS
Compariva un indicatore di performance in un angolo.

**Soluzione:**
- Disabilitati in `sdkconfig` e `sdkconfig.defaults`:
  - `CONFIG_LV_USE_SYSMON=n`
  - `CONFIG_LV_USE_PERF_MONITOR=n`

### C) UI orizzontale senza ruotare il display
Se serve una UI orizzontale senza rotazione software completa:
- Ruotare singoli widget con
  - `lv_obj_set_style_transform_angle(widget, 900, LV_PART_MAIN)`
  - impostando un pivot centrale

**Risultato:**
- UI percepita come orizzontale, senza ruotare il display.
- Richiede modifica layout widget per widget.

## Soluzioni non praticabili

### Rotazione software 270 gradi
- Instabile: errori di cache, watchdog, artefatti grafici
- Anche riducendo buffer e disabilitando DMA, i problemi restano

## Raccomandazioni finali
1. Usare la configurazione stabile in verticale (portrait) con layout adattato.
2. Se serve un layout orizzontale, ruotare solo i widget necessari.
3. Evitare la rotazione software completa per 270 gradi su questa piattaforma.

## File di riferimento
- [main/init.c](main/init.c)
- [sdkconfig](sdkconfig)
- [sdkconfig.defaults](sdkconfig.defaults)
