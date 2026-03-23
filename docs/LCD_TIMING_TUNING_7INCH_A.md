# LCD Timing Tuning (Pannello attivo 7_INCH_A)

## Configurazione attiva rilevata

- Pannello: `CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A=y` (`sdkconfig.defaults`)
- Formato colore: `CONFIG_BSP_LCD_COLOR_FORMAT_RGB565=y` (`sdkconfig.defaults`)
- Driver DPI: `ILI9881C_720_1280_PANEL_60HZ_DPI_CONFIG` (`components/waveshare__esp_lcd_ili9881c/include/esp_lcd_ili9881c.h`)

## Tabella pratica (parametro -> effetto visivo -> range consigliato -> rischio)

| Parametro | Valore attuale | Effetto visivo | Range consigliato | Rischio |
|---|---:|---|---|---|
| `dpi_clock_freq_mhz` | `80` | Fluidita/FPS e stabilita generale | `70–90` (step 2–5) | Alto: flicker/schermo nero; Basso: UI meno fluida |
| `hsync_back_porch` | `239` | Stabilita orizzontale e allineamento | `200–280` | Troppo basso: artefatti/tearing laterale |
| `hsync_pulse_width` | `50` | Aggancio sync orizzontale | `20–80` | Righe spurie/sync instabile |
| `hsync_front_porch` | `33` | Margine orizzontale fine | `16–64` | Disturbi sui bordi |
| `vsync_back_porch` | `20` | Stabilita verticale | `12–40` | Salti verticali |
| `vsync_pulse_width` | `30` | Aggancio sync verticale | `4–40` | Rolling/flicker verticale |
| `vsync_front_porch` | `2` | Margine verticale fine | `2–16` | Glitch verticali |
| DSI lane bitrate | `1000 Mbps` (effettivo) | Margine banda MIPI | `840–1200` | Alto: errori link; Basso: underrun/tearing |
| Formato colore | `RGB565` | Qualita colore vs banda | `RGB565` per stabilita | `RGB888` aumenta banda/instabilita timing |
| `CONFIG_BSP_LCD_DPI_BUFFER_NUMS` | `0` | Smoothness/tearing | `1–2` (se RAM ok) | Buffer basso: tearing/stutter |
| `CONFIG_LV_DEF_REFR_PERIOD` | `15 ms` | Reattivita UI percepita | `10–20 ms` | Troppo basso: carico CPU; alto: lag UI |

## Note importanti

- Formula refresh rate (dal driver):
  - `FPS = dpi_clock / Htotal / Vtotal`
  - riferimento: commento in `esp_lcd_ili9881c.h`
- Con i valori attuali (`80 MHz`, porch/sync correnti) il refresh teorico e circa `57.6 Hz`.
- Il valore `CONFIG_BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS=1500` in `sdkconfig.defaults` puo non coincidere con l'effettivo runtime perche il codice usa il mapping pannello in `components/waveshare__esp32_p4_nano/include/bsp/display.h` (per 7" A: `1000`) applicato in `esp32_p4_nano.c`.

## Procedura di taratura consigliata (senza tentativi alla cieca)

1. Mantieni porch/sync attuali e varia solo `dpi_clock_freq_mhz` (`80 -> 75 -> 70` o `80 -> 85`).
2. Se compaiono artefatti laterali, aumenta prima `hsync_back_porch` (+16/+24).
3. Se compaiono salti verticali, aumenta `vsync_back_porch` (+4/+8).
4. Se il link e instabile, riduci bitrate DSI (es. `1000 -> 933 -> 860`).
5. Solo dopo, valuta `RGB888` e/o aumento buffer DPI (`CONFIG_BSP_LCD_DPI_BUFFER_NUMS`).

## Verifica minima dopo ogni modifica

- Boot completo senza schermo nero.
- Nessun flicker in schermate statiche (30 s).
- Nessun tearing visibile durante animazioni/timer.
- Touch/UI reattiva senza lag evidente.
