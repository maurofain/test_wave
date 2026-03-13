# REFACTORING JSON - confronto campi

Riferimento: verifica dei campi usati nel commit `68c070c9f430cfd50aad1a51c0b15cc2dffb4ae0` rispetto al mapping attuale in `api_config_get` / `api_config_save`.

Legenda stato:
- `OK`: allineato (short e/o compat legacy gestiti)
- `PARZIALE`: mapping presente ma con ambiguità o semantica non perfetta
- `MISMATCH`: non allineato

| Campo legacy (commit) | Campo refactor (attuale) | Variabile target | Stato | Note |
|---|---|---|---|---|
| `device_name` | `dname` | `cfg->device_name` | OK | Save accetta entrambi (`dname` + compat `device_name`). |
| `location_name` | `loc` | `cfg->location_name` | OK | Save accetta entrambi. |
| `num_programs` | `n_prg` | `cfg->num_programs` | OK | Save accetta entrambi con validazione valori ammessi. |
| `latitude` | `lat` | `cfg->latitude` | OK | Save accetta entrambi. |
| `longitude` | `lon` | `cfg->longitude` | OK | Save accetta entrambi. |
| `eth.enabled` | `eth.en` | `cfg->eth.enabled` | OK | Compat su `enabled`. |
| `eth.dhcp_enabled` | `eth.dhcp` | `cfg->eth.dhcp_enabled` | OK | Compat su `dhcp_enabled`. |
| `eth.subnet` | `eth.sub` | `cfg->eth.subnet` | OK | Compat su `subnet`. |
| `eth.gateway` | `eth.gw` | `cfg->eth.gateway` | OK | Compat su `gateway`. |
| `wifi.sta_enabled` | `wifi.sta` | `cfg->wifi.sta_enabled` | OK | Compat su `sta_enabled`. |
| `wifi.dhcp_enabled` | `wifi.dhcp` | `cfg->wifi.dhcp_enabled` | OK | Compat su `dhcp_enabled`. |
| `wifi.password` | `wifi.pwd` | `cfg->wifi.password` | OK | Compat su `password`. |
| `wifi.subnet` | `wifi.sub` | `cfg->wifi.subnet` | MISMATCH | In save non viene letto (`ip/sub/gw` Wi-Fi non caricati). |
| `wifi.gateway` | `wifi.gw` | `cfg->wifi.gateway` | MISMATCH | In save non viene letto (`ip/sub/gw` Wi-Fi non caricati). |
| `ntp_enabled` | `ntp_en` | `cfg->ntp_enabled` | OK | Compat su `ntp_enabled`. |
| `ntp.server1` | `ntp.s1` | `cfg->ntp.server1` | OK | Compat su `server1`. |
| `ntp.server2` | `ntp.s2` | `cfg->ntp.server2` | OK | Compat su `server2`. |
| `ntp.timezone_offset` | `ntp.tz` | `cfg->ntp.timezone_offset` | OK | Compat su `timezone_offset`. |
| `server.enabled` | `server.en` | `cfg->server.enabled` | OK | Compat su `enabled`. |
| `server.serial` | `server.ser` | `cfg->server.serial` | OK | Compat su `serial`. |
| `server.password` | `server.pwd` | `cfg->server.password` | OK | Compat su `password`. |
| `remote_log.server_port` | `rlog.port` | `cfg->remote_log.server_port` | OK | Compat su `remote_log` e `server_port`. |
| `remote_log.use_broadcast` | `rlog.bcast` | `cfg->remote_log.use_broadcast` | OK | Compat su `use_broadcast`. |
| `remote_log.write_to_sd` | `rlog.to_sd` | `cfg->remote_log.write_to_sd` | OK | Compat su `write_to_sd`. |
| `sensors.io_expander_enabled` | `sensors.io_exp` | `cfg->sensors.io_expander_enabled` | OK | Compat presente. |
| `sensors.temperature_enabled` | `sensors.temp` | `cfg->sensors.temperature_enabled` | OK | Compat presente. |
| `sensors.led_enabled` | `sensors.led` | `cfg->sensors.led_enabled` | OK | Compat presente. |
| `sensors.led_count` | `sensors.led_n` | `cfg->sensors.led_count` | OK | Compat presente. |
| `sensors.rs232_enabled` | `sensors.rs232` | `cfg->sensors.rs232_enabled` | OK | Compat presente. |
| `sensors.rs485_enabled` | `sensors.rs485` | `cfg->sensors.rs485_enabled` | OK | Compat presente. |
| `sensors.mdb_enabled` | `sensors.mdb` | `cfg->sensors.mdb_enabled` | OK | Compat presente. |
| `sensors.cctalk_enabled` | `sensors.cctalk` | `cfg->sensors.cctalk_enabled` | OK | Compat presente. |
| `sensors.eeprom_enabled` | `sensors.eeprom` | `cfg->sensors.eeprom_enabled` | OK | Compat presente. |
| `sensors.sd_card_enabled` | `sensors.sd` | `cfg->sensors.sd_card_enabled` | OK | Compat presente. |
| `sensors.pwm1_enabled` | `sensors.pwm1` | `cfg->sensors.pwm1_enabled` | OK | Compat presente. |
| `sensors.pwm2_enabled` | `sensors.pwm2` | `cfg->sensors.pwm2_enabled` | OK | Compat presente. |
| `display.enabled` | `display.en` | `cfg->display.enabled` | MISMATCH | Save cerca solo `display.enabled` (non `en`). |
| `display.lcd_brightness` | `display.brt` | `cfg->display.lcd_brightness` | OK | Compat su `lcd_brightness`. |
| `rs232.data_bits` | `rs232.data` | `cfg->rs232.data_bits` | OK | Parser serial accetta `data` + compat `data_bits`. |
| `rs232.stop_bits` | `rs232.stop` | `cfg->rs232.stop_bits` | OK | Parser serial accetta `stop` + compat `stop_bits`. |
| `rs485.data_bits` | `rs485.data` | `cfg->rs485.data_bits` | OK | Parser serial accetta `data` + compat `data_bits`. |
| `rs485.stop_bits` | `rs485.stop` | `cfg->rs485.stop_bits` | OK | Parser serial accetta `stop` + compat `stop_bits`. |
| `mdb_serial.*` | `mdb_ser.*` | `cfg->mdb_serial.*` | OK | Save accetta `mdb_ser` + compat `mdb_serial`. |
| `gpios.gpio33.mode` | `gpios.gpio33.m` | `cfg->gpios.gpio33.mode` | OK | Compat presente (`m`/`mode`). |
| `gpios.gpio33.state` | `gpios.gpio33.st` | `cfg->gpios.gpio33.initial_state` | OK | Compat presente (`st`/`state`). |
| `scanner.dual_pid` | `scanner.dpid` | `cfg->scanner.dual_pid` | OK | Compat presente. |
| `scanner.cooldown_ms` | `scanner.cool` | `cfg->scanner.cooldown_ms` | OK | Compat presente. |
| `ui.user_language` | `ui.ulang` | `cfg->ui.user_language` | OK | Compat presente (`user_language` / `language`). |
| `ui.backend_language` | `ui.blang` | `cfg->ui.backend_language` | OK | Compat presente. |
| `ui_language` (root) | `ui_language` (root) | `cfg->ui.user_language` | OK | Compat root-level mantenuta. |
| `timeouts.language_return_ms` | `timeouts.t_lang`/`t_prg` | `cfg->timeouts.*` | PARZIALE | Compat legacy puntata su `t_prg` (exit programs), non su `t_lang` (exit language). |

## Note operative

- Le incompatibilità principali da correggere lato parser save sono:
  1. `display.en` non letto (solo `display.enabled`).
  2. `wifi.ip/sub/gw` non letti in save.
  3. mapping legacy `timeouts.language_return_ms` da chiarire/allineare semanticamente.
