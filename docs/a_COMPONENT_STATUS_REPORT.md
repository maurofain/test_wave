# Report: Analisi Componenti Hardware — Funzioni Richieste

> Generato automaticamente — 13 aprile 2026
>
> Requisiti per ogni componente hardware:
> | # | Funzione | Descrizione |
> |---|---|---|
> | 1 | `init` | Inizializzazione periferica |
> | 2 | `activate` | Abilitazione del componente (avvia task/operazioni) |
> | 3 | `deactivate` | Disabilitazione del componente (ferma task/operazioni) |
> | 4 | `setup` | Configurazione parametri runtime |
> | 5 | `status` | Stato: `disabled` / `enabled` / `offline` / `online` |

---

## Legenda

| Simbolo | Significato |
|---|---|
| ✅ | Presente (nome esatto o equivalente funzionale) |
| ⚠️ | Parziale / equivalente indiretto |
| ❌ | Mancante |

---

## Tabella Riepilogativa

| Componente | `init` | `activate` | `deactivate` | `setup` | `status` |
|---|---|---|---|---|---|
| **cctalk** | ✅ `cctalk_driver_init` | ✅ `cctalk_driver_start_acceptor` | ✅ `cctalk_driver_stop_acceptor` | ❌ | ⚠️ `cctalk_driver_is_acceptor_online` |
| **mdb** | ✅ `mdb_init` | ✅ `mdb_start_engine` | ❌ | ❌ | ❌ |
| **usb_cdc_scanner** | ✅ `usb_cdc_scanner_init` | ⚠️ `send_on_command` | ⚠️ `send_off_command` | ⚠️ `send_setup_command` | ⚠️ `is_connected` |
| **io_expander** | ✅ `io_expander_init` | ❌ | ❌ | ⚠️ `set_config_enabled` | ❌ |
| **led** | ✅ `led_init` | ❌ | ❌ | ❌ | ❌ |
| **pwm** | ✅ `pwm_init` | ❌ | ❌ | ❌ | ❌ |
| **rs232** | ✅ `rs232_init` | ❌ | ❌ | ❌ | ❌ |
| **rs485** | ✅ `rs485_init` | ❌ | ✅ `rs485_deinit` | ❌ | ❌ |
| **sd_card** | ⚠️ `sd_card_init_monitor` | ✅ `sd_card_mount` | ✅ `sd_card_unmount` | ❌ | ⚠️ `sd_card_is_mounted` / `is_present` |
| **sht40** | ✅ `sht40_init` | ❌ | ❌ | ❌ | ⚠️ `sht40_is_ready` |
| **aux_gpio** | ✅ `aux_gpio_init` | ❌ | ❌ | ❌ | ❌ |
| **digital_io** | ✅ `digital_io_init` | ❌ | ❌ | ❌ | ❌ |
| **modbus_relay** | ✅ `modbus_relay_init` | ❌ | ✅ `modbus_relay_deinit` | ❌ | ✅ `modbus_relay_get_status` / `is_running` |
| **periph_i2c** | ✅ `periph_i2c_init` | ❌ | ❌ | ❌ | ❌ |
| **eeprom_24lc16** | ✅ `eeprom_24lc16_init` | ❌ | ❌ | ❌ | ⚠️ `eeprom_24lc16_is_available` |
| **audio_player** | ✅ `audio_player_init` | ⚠️ `play_file` | ✅ `audio_player_stop` | ⚠️ `set_volume` | ⚠️ `is_playing` |

---

## Analisi Dettagliata per Componente

### `cctalk`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `cctalk_init(...)` | `cctalk_driver_init()` | ✅ |
| activate | — | `cctalk_driver_start_acceptor()` | ✅ |
| deactivate | — | `cctalk_driver_stop_acceptor()` | ✅ |
| setup | — | — | ❌ Manca funzione di configurazione runtime |
| status | — | `cctalk_driver_is_acceptor_online()` restituisce solo bool | ⚠️ Occorre un enum `disabled/enabled/offline/online` |

**Da implementare:** `cctalk_driver_setup(config)` + `cctalk_driver_get_status()` → enum

---

### `mdb`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `mdb_init()` | ✅ | ✅ |
| activate | — | `mdb_start_engine()` | ✅ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** `mdb_stop_engine()`, `mdb_setup(config)`, `mdb_get_status()` → enum

---

### `usb_cdc_scanner`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `usb_cdc_scanner_init(config)` | ✅ | ✅ |
| activate | — | `send_on_command()` (HW on) | ⚠️ Manca `activate()` logico |
| deactivate | — | `send_off_command()` (HW off) | ⚠️ Manca `deactivate()` logico |
| setup | — | `send_setup_command()` | ⚠️ Non è setup parametrico |
| status | — | `is_connected()` → bool | ⚠️ Manca enum completo |

**Da implementare:** `usb_cdc_scanner_get_status()` → enum + rinomina/alias logici activate/deactivate

---

### `io_expander`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `io_expander_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | `set_config_enabled()` | ⚠️ Solo flag on/off, non parametrico |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate + `io_expander_get_status()` → enum

---

### `led`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `led_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate (power on/off strip) + `led_get_status()` → enum

---

### `pwm`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `pwm_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate + `pwm_get_status()` → enum

---

### `rs232`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `rs232_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** deinit/activate/deactivate + `rs232_get_status()` → enum

---

### `rs485`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `rs485_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | `rs485_deinit()` | ⚠️ È deinit, non deactivate logica |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate + `rs485_get_status()` → enum; rinominare/alias deinit→deactivate

---

### `sd_card`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `sd_card_init_monitor()` | ⚠️ Inizializza solo il task monitor | ⚠️ |
| activate | — | `sd_card_mount()` | ✅ |
| deactivate | — | `sd_card_unmount()` | ✅ |
| setup | — | — | ❌ |
| status | — | `is_mounted()` / `is_present()` | ⚠️ Manca enum disabled/enabled/offline/online |

**Da implementare:** `sd_card_setup(config)` + `sd_card_get_status()` → enum

---

### `sht40`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `sht40_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | `sht40_is_ready()` → bool | ⚠️ Manca enum |

**Da implementare:** activate/deactivate + `sht40_get_status()` → enum

---

### `aux_gpio`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `aux_gpio_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate + `aux_gpio_get_status()` → enum

---

### `digital_io`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `digital_io_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate + `digital_io_get_status()` → enum

---

### `modbus_relay`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `modbus_relay_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | `modbus_relay_deinit()` | ⚠️ È deinit, non deactivate logica |
| setup | — | — | ❌ |
| status | — | `get_status()` / `is_running()` | ⚠️ `get_status()` ha tipo proprio, non condivide enum globale |

**Da implementare:** activate + `modbus_relay_setup(config)` + allineamento enum status

---

### `periph_i2c`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `periph_i2c_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | — | ❌ |

**Da implementare:** activate/deactivate + `periph_i2c_get_status()` → enum

---

### `eeprom_24lc16`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `eeprom_24lc16_init()` | ✅ | ✅ |
| activate | — | — | ❌ |
| deactivate | — | — | ❌ |
| setup | — | — | ❌ |
| status | — | `is_available()` → bool | ⚠️ Manca enum |

**da implementare:** activate/deactivate + `eeprom_24lc16_get_status()` → enum

---

### `audio_player`
| # | Richiesto | Esistente | Gap |
|---|---|---|---|
| init | `audio_player_init()` | ✅ | ✅ |
| activate | — | `play_file()` (avvia riproduzione) | ⚠️ Non è activate logico |
| deactivate | — | `audio_player_stop()` | ✅ (funzionale) |
| setup | — | `set_volume()` | ⚠️ Parziale |
| status | — | `is_playing()` → bool | ⚠️ Manca enum |

**Da implementare:** `audio_player_activate()` / `audio_player_get_status()` → enum

---

## Riepilogo Gap per Tipo

| Funzione | Componenti dove manca completamente | Componenti parziali |
|---|---|---|
| **init** | — | `sd_card` (solo monitor) |
| **activate** | `mdb`, `io_expander`, `led`, `pwm`, `rs232`, `rs485`, `sht40`, `aux_gpio`, `digital_io`, `periph_i2c`, `eeprom_24lc16` | `usb_cdc_scanner`, `audio_player` |
| **deactivate** | `mdb`, `io_expander`, `led`, `pwm`, `rs232`, `sht40`, `aux_gpio`, `digital_io`, `periph_i2c`, `eeprom_24lc16` | `usb_cdc_scanner`, `rs485`(deinit), `modbus_relay`(deinit) |
| **setup** | `mdb`, `io_expander`, `led`, `pwm`, `rs232`, `rs485`, `sd_card`, `sht40`, `aux_gpio`, `digital_io`, `modbus_relay`, `periph_i2c`, `eeprom_24lc16` | `usb_cdc_scanner`, `cctalk`, `audio_player` |
| **status→enum** | `mdb`, `io_expander`, `led`, `pwm`, `rs232`, `rs485`, `aux_gpio`, `digital_io`, `periph_i2c` | `cctalk`, `usb_cdc_scanner`, `sd_card`, `sht40`, `modbus_relay`, `eeprom_24lc16`, `audio_player` |

---

## Raccomandazione

Definire un tipo `hw_component_status_t` condiviso (in un header comune, es. `components/hw_common.h`):

```c
typedef enum {
    HW_STATUS_DISABLED = 0,  /**< Componente disabilitato in configurazione */
    HW_STATUS_ENABLED,       /**< Abilitato ma non ancora operativo          */
    HW_STATUS_OFFLINE,       /**< Abilitato ma periferica non risponde        */
    HW_STATUS_ONLINE,        /**< Abilitato e operativo                       */
} hw_component_status_t;
```

Ogni componente deve poi esporre:
```c
hw_component_status_t <component>_get_status(void);
```

**Priorità di implementazione** (per impatto su watchdog/FSM):
1. `mdb` — deactivate + status
2. `cctalk` — setup + status enum
3. `rs232` / `rs485` — activate/deactivate + status
4. `sht40` / `io_expander` — status enum
5. Tutti gli altri — status enum (low effort)
