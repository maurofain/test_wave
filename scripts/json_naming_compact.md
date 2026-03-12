# JSON Key Rename Map — Compattazione campi config

Rinominazione applicata in: `device_config.c`, `web_ui.c`, `web_ui_logs.c`, `webpages_embedded.c` (e HTML derivati).

Le funzioni di parse mantengono retrocompatibilità: provano prima il nuovo nome, poi il vecchio come fallback.

## Root

| Vecchio nome       | Nuovo nome |
|--------------------|------------|
| `device_name`      | `dname`    |
| `location_name`    | `loc`      |
| `num_programs`     | `n_prg`    |
| `latitude`         | `lat`      |
| `longitude`        | `lon`      |
| `ntp_enabled`      | `ntp_en`   |
| `remote_log` (obj) | `rlog`     |
| `mdb_serial` (obj) | `mdb_ser`  |

## Sezione `eth`

| Vecchio nome   | Nuovo nome |
|----------------|------------|
| `enabled`      | `en`       |
| `dhcp_enabled` | `dhcp`     |
| `subnet`       | `sub`      |
| `gateway`      | `gw`       |

## Sezione `wifi`

| Vecchio nome   | Nuovo nome |
|----------------|------------|
| `sta_enabled`  | `sta`      |
| `dhcp_enabled` | `dhcp`     |
| `password`     | `pwd`      |
| `subnet`       | `sub`      |
| `gateway`      | `gw`       |

## Sezione `ntp`

| Vecchio nome      | Nuovo nome |
|-------------------|------------|
| `server1`         | `s1`       |
| `server2`         | `s2`       |
| `timezone_offset` | `tz`       |

## Sezione `server`

| Vecchio nome | Nuovo nome |
|--------------|------------|
| `enabled`    | `en`       |
| `serial`     | `ser`      |
| `password`   | `pwd`      |

## Sezione `rlog` (ex `remote_log`)

| Vecchio nome    | Nuovo nome |
|-----------------|------------|
| `server_port`   | `port`     |
| `use_broadcast` | `bcast`    |
| `write_to_sd`   | `to_sd`    |

## Sezione `sensors`

| Vecchio nome            | Nuovo nome |
|-------------------------|------------|
| `io_expander_enabled`   | `io_exp`   |
| `temperature_enabled`   | `temp`     |
| `led_enabled`           | `led`      |
| `led_count`             | `led_n`    |
| `rs232_enabled`         | `rs232`    |
| `rs485_enabled`         | `rs485`    |
| `mdb_enabled`           | `mdb`      |
| `cctalk_enabled`        | `cctalk`   |
| `eeprom_enabled`        | `eeprom`   |
| `sd_card_enabled`       | `sd`       |
| `pwm1_enabled`          | `pwm1`     |
| `pwm2_enabled`          | `pwm2`     |

## Sezione `scanner`

| Vecchio nome  | Nuovo nome |
|---------------|------------|
| `enabled`     | `en`       |
| `dual_pid`    | `dpid`     |
| `cooldown_ms` | `cool`     |

## Sezione `display`

| Vecchio nome     | Nuovo nome |
|------------------|------------|
| `enabled`        | `en`       |
| `lcd_brightness` | `brt`      |

## Sezioni `rs232`, `rs485`, `mdb_ser` (seriale)

| Vecchio nome | Nuovo nome |
|--------------|------------|
| `data_bits`  | `data`     |
| `parity`     | `par`      |
| `stop_bits`  | `stop`     |

## Sezione `gpios.gpio33`

| Vecchio nome | Nuovo nome |
|--------------|------------|
| `mode`       | `m`        |
| `state`      | `st`       |

## Sezione `ui`

| Vecchio nome       | Nuovo nome |
|--------------------|------------|
| `user_language`    | `ulang`    |
| `backend_language` | `blang`    |

## Campi NON rinominati

Rimasti invariati per brevità già sufficiente o uso in contesti non JSON:
`ip`, `ssid`, `url`, `vid`, `pid`, `baud`, `rx_buf`, `tx_buf`,
`dns1`, `dns2`, `gpio33`, `updated`, `image_source`, `ui_language` (root compat),
campi Modbus (`slave_id`, `poll_ms`, `timeout_ms`, `retries`, `relay_start`,
`relay_count`, `input_start`, `input_count`), campi CCTalk (`address`, `baud`).
