# tasks.json — Documentazione

File: `data/tasks.json` (flashato su SPIFFS in `/spiffs/tasks.json`)

---

## Schema chiavi

| Chiave | Campo | Tipo | Descrizione |
|--------|-------|------|-------------|
| `n` | name | string | Nome del task (corrisponde al nome FreeRTOS) |
| `s` | state | int | Stato iniziale (enum, vedi tabella sotto) |
| `p` | priority | int | Priorità FreeRTOS (0–25) |
| `c` | core | int | Core di affinità: 0 o 1 |
| `m` | period_ms | int | Periodo del loop in millisecondi (0 = triggered/event) |
| `w` | stack_words | int | Dimensione stack in parole FreeRTOS (1 word = 4 byte) |
| `k` | stack_caps | int | Tipo di memoria per lo stack (enum, vedi tabella sotto) |

---

## Enum: `s` — State

| Valore | Nome | Descrizione |
|--------|------|-------------|
| `0` | idle | Task non avviato |
| `1` | run | Task avviato normalmente |
| `2` | pause | Task avviato ma in pausa |

---

## Enum: `k` — Stack Caps

| Valore | Nome | Descrizione |
|--------|------|-------------|
| `0` | spiram | Stack allocato in PSRAM (MALLOC_CAP_SPIRAM) |
| `1` | internal | Stack allocato in RAM interna (MALLOC_CAP_INTERNAL) |

---

## Tabella task espansa

| name | state | priority | core | period_ms | stack_words | stack_caps |
|------|-------|----------|------|-----------|-------------|------------|
| ws2812 | run | 5 | 0 | 100 | 4096 | internal |
| eeprom | run | 4 | 0 | 1000 | 2048 | spiram |
| io_expander | run | 4 | 0 | 500 | 2048 | spiram |
| sht40 | run | 4 | 0 | 200000 | 8192 | spiram |
| rs232 | idle | 5 | 0 | 10 | 4096 | spiram |
| rs485 | idle | 5 | 0 | 100 | 4096 | spiram |
| mdb | idle | 5 | 0 | 10 | 4096 | spiram |
| pwm | idle | 4 | 0 | 20 | 2048 | spiram |
| fsm | run | 4 | 0 | 100 | 32768 | spiram |
| digital_io | run | 4 | 0 | 50 | 4096 | spiram |
| io_process | run | 4 | 0 | 100 | 4096 | spiram |
| http_services | run | 4 | 0 | 100 | 32768 | spiram |
| scanner_cooldown | run | 4 | 0 | 100 | 4096 | spiram |
| log_sender | run | 5 | 0 | 1000 | 12288 | internal |
| touchscreen | run | 4 | 0 | 20 | 8192 | spiram |
| lvgl | run | 10 | 0 | 16 | 65536 | spiram |
| ntp | run | 3 | 0 | 600000 | 32768 | spiram |
| usb_scanner | run | 4 | 0 | 1000 | 32768 | spiram |
| cctalk_task | run | 5 | 0 | 200 | 4096 | spiram |
| mdb_engine | idle | 5 | 0 | 500 | 4096 | spiram |
| sd_monitor | run | 5 | 0 | 500 | 4096 | spiram |
| usb_lib | run | 10 | 0 | 0 | 4096 | internal |
| usb_host_lib | run | 17 | 0 | 0 | 4096 | internal |
| usb_monitor | run | 8 | 0 | 0 | 4096 | internal |
| usb_cdc_open | run | 8 | 0 | 0 | 4096 | internal |
| rs232_test | idle | 5 | 0 | 0 | 4096 | internal |
| rs485_test | idle | 5 | 0 | 0 | 4096 | internal |
| led_test | idle | 5 | 0 | 0 | 4096 | internal |
| mdb_test | idle | 5 | 0 | 0 | 4096 | internal |
| ioexp_test | idle | 5 | 0 | 0 | 4096 | internal |
| p1test | idle | 5 | 0 | 0 | 4096 | internal |
| p2test | idle | 5 | 0 | 0 | 4096 | internal |
| sd_fmt_work | idle | 5 | 0 | 0 | 4096 | internal |
| sd_fmt_timer | idle | 4 | 0 | 0 | 4096 | internal |

---

## Note

- `period_ms = 0` indica task event-driven (bloccano su coda/semaforo, non hanno loop periodico)
- La tabella sopra riflette lo snapshot corrente di `data/tasks.json`.
- In caso di modifica stack (`w`) o policy memoria (`k`) aggiornare **sempre** sia `data/tasks.json` sia questa documentazione.
- Stack in spiram è più economico in RAM interna ma leggermente più lento; usare internal per task real-time o con stack molto piccolo
- Il file viene aggiornato via HTTP POST `/tasks` ed è editabile dalla pagina web `/tasks`
