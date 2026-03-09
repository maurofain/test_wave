# Report di Inizializzazione Sistema - Boot Sequence Analysis

## Timeline Complessiva
**Boot Start**: T=1008ms | **Boot Complete**: T=17789ms | **Durata Totale**: ~16,7 secondi

---

## Sequenza di Inizializzazione Dettagliata

| Fase | Inizio | Fine | Durata | Descrizione |
|------|--------|------|--------|-------------|
| **Core Dump** | 1008ms | 1008ms | - | Init coredump flash (partizione 188KB) |
| **I2C Bus** | 1009ms | 1059ms | **50ms** | Init bus I2C (port=1) - BSP |
| **IO Expander** | 1059ms | 1059ms | <1ms | GPIO3 check (value=1) |
| **Boot Guard** | 1079ms | 1079ms | - | Reset reason=3, crashes=0, limit=3 |
| **SPIFFS Mount** | 1169ms | 1169ms | **~90ms** | 29 file, 1438KB totale, 910KB usato |
| **EEPROM Init** | 1409ms | 1539ms | **130ms** | ❌ Errore CRC + write fallita (ESP_ERR_INVALID_STATE) |
| **Remote Logging** | 2819ms | 2819ms | - | Early capture init (connessione remota fallita) |
| **Memory Check 1** | 2839ms | 2849ms | - | INTERNAL: 287KB, DMA: 247KB, PSRAM: 31.5MB |
| **I2C Diagnostics** | 2859ms | 2879ms | **20ms** | Scan 3 device (@0x18, @0x45, @0x5D) |
| **LVGL Init** | ~2880ms | 4309ms | **~1429ms** | Display driver + touch + backlight 80% |
| **Display Config** | 4329ms | 4329ms | - | 720×1280 (after rotation) |
| **Ethernet Init** | 4339ms | 6109ms | **1770ms** | MDC/MDIO setup, MAC set, driver start, DHCP |
| **Memory Check 2** | 4309ms | 4309ms | - | Heap INTERNAL: 132KB, PSRAM: 31.2MB |
| **Web UI Init** | 6539ms | 6549ms | **~10ms** | ⚠️ /spiffs/www dir creation failed |
| **SD Card Init** | ~7100ms | 7109ms | - | SDHC 90GB detected, 1MHz clock |
| **IP Acquisition** | 7109ms | 7109ms | - | 192.168.2.16/255.255.255.0 (gateway 2.209) |
| **NTP/SNTP** | 7129ms | 11979ms | **~4850ms** | Time sync background task |
| **HTTP Login** | 7149ms | 7279ms | **130ms** | Initial token acquisition (JWT received) |
| **Tasks Load & Start** | 7289ms | 7429ms | **140ms** | 15 tasks launched (eeprom, fsm, http_services, lvgl, ntp, usb_scanner, etc.) |
| **USB Scanner** | 7399ms | 10559ms | **3160ms** | Host install, device discovery, CDC open, enable |
| **Stability Window** | 7429ms | 17459ms | **10000ms** | Forced wait (BOOT_COUNTER_RESET_DELAYED) |
| **CCTalk Init** | 17459ms | 17689ms | **230ms** | 4-cmd sequence (Address Poll → Inhibit → Master Inhibit → Request Status) |
| **Ads Page Load** | 17769ms | 17779ms | **10ms** | 7 images loaded from SPIFFS, carousel timer set (30s rotation) |
| **Boot Done** | 17789ms | 17789ms | - | Counter reset, app ready, endpoints live |

---

## Analisi per Device/Subsistema

### 🔋 Power & Core
- **Core Dump**: 0ms (check-only)
- **Total**: Negligible

### 📡 I2C & GPIO
- **Test time**: 1009-1059ms = **50ms**
- **Status**: ✅ OK
- **Devices found**: 3 (0x18, 0x45, 0x5D)

### 🗂️ Storage
| Subsystem | Init Time | Status | Notes |
|-----------|-----------|--------|-------|
| SPIFFS | 1169ms | ✅ | 29 files, 910KB/1438KB |
| EEPROM | 1409-1539ms | ❌ | CRC mismatch + write fail |
| SD Card | ~7100ms | ✅ | 90GB detected |

### 🖥️ Display & Graphics
| Component | Duration | Status |
|-----------|----------|--------|
| LVGL Init | 1429ms | ✅ |
| Touch Calibration | Included | ✅ |
| Backlight | 4329ms | ✅ (80%) |
| Resolution | 720×1280 | ✅ |

### 🌐 Networking
| Service | Init-Complete | Duration | Status |
|---------|--------------|----------|--------|
| Ethernet | 4339-6109ms | 1770ms | ✅ |
| IP (DHCP) | 7109ms | - | ✅ 192.168.2.16 |
| NTP (async) | 7129-11979ms | ~4850ms | ✅ sync'd by 11979ms |
| Web UI | 6539-6549ms | ~10ms | ⚠️ www dir failed |
| HTTP Login | 7149-7279ms | 130ms | ✅ JWT acquired |

### 🔌 Hardware Peripherals
| Device | Start | Complete | Duration | Status |
|--------|-------|----------|----------|--------|
| USB Scanner | 7399ms | 10559ms | 3160ms | ✅ CDC-ACM open |
| Touchscreen | 7369ms | - | - | ✅ (valid handle) |
| SHT40 | 7309ms | - | - | ✅ (mock: 25°C/50%) |
| CCTalk | 17459ms | 17689ms | 230ms | ✅ all 4 cmds OK |

### 💾 Memory (Snapshot)
| Heap Type | Before LVGL | After LVGL | After All |
|-----------|-------------|-----------|-----------|
| INTERNAL | 287KB | 132KB | ~130KB |
| DMA | 247KB | 93KB | ~70KB |
| PSRAM | 31.5MB | 31.3MB | ~31.0MB |

**Memory pressure**: Low - PSRAM dominant for graphics buffers

---

## Task Execution Order (T=7289ms)

```
✅ eeprom (2KB)
✅ io_expander (2KB)
✅ sht40 (8KB) → [MOCK] 25°C
❌ rs232 (skipped)
❌ rs485 (skipped)
❌ mdb (skipped)
❌ pwm (skipped)
✅ fsm (32KB) → [idle state]
✅ http_services (32KB)
✅ scanner_cooldown (4KB)
✅ touchscreen (8KB)
✅ lvgl (65KB)
✅ ntp (32KB)
✅ usb_scanner (32KB) → USB enumeration in progress
✅ cctalk (4KB)
❌ mdb_engine (skipped)
✅ sd_monitor (4KB)
```

**Total stack allocated**: ~231KB | **Status**: 15 active, 4 skipped

---

## Critical Path Analysis

```
BOOT START (1008ms)
    ↓
I2C + IO (50ms) → EEPROM{FAIL}
    ↓
SPIFFS (1169ms)
    ↓
LVGL + Display (1429ms) ← HEAVY
    ↓
Ethernet (1770ms) ← HEAVY
    ↓
NTP async (4850ms background)
HTTP Login (130ms)
USB Scanner (3160ms) ← HEAVY & parallel
Tasks Start (140ms)
    ↓
Stability Window (10000ms) ← FORCED WAIT
    ↓
CCTalk Init (230ms)
    ↓
Ads Page Show (10ms)
    ↓
BOOT COMPLETE (17789ms)
```

---

## Error Summary

| Error | Timestamp | Component | Impact |
|-------|-----------|-----------|--------|
| ❌ EEPROM CRC mismatch | 1409ms | device_config | Config defaults used |
| ❌ EEPROM write fail | 1539ms | device_config | ESP_ERR_INVALID_STATE |
| ⚠️ Web pages dir create fail | 6539ms | web_ui | Cache miss expected |
| ⚠️ HTTP connect fail (preboot) | 2819ms | remote_logging | No network yet (normal) |

---

## Performance Characteristics

| Metric | Value | Status |
|--------|-------|--------|
| **Stability window** | 10.0s | 🔴 Configured delay (tuneable) |
| **USB enumeration** | 3.16s | 🟡 Slow (Sperimentali driver) |
| **LVGL graphics** | 1.43s | 🟢 Good |
| **Network ready** | 7.11s | 🟢 Fast (Ethernet, no WiFi) |
| **Boot to ads** | 17.77s | 🟡 Long (stability window) |
| **Boot to ready** | 17.79s | 🟡 Long (stability window) |

---

## Recommendations

1. **EEPROM Error**: CRC mismatch - verify EEPROM hardware or clear stored config
2. **Stability Window**: 10s can be reduced to 5s if boot reliability proven
3. **USB Scanner**: 3.16s enumeration is slow - consider timeout config
4. **Web Pages**: Check `/spiffs/www` directory ownership/permissions
5. **CCTalk**: Init sequence is efficient (230ms) - good response times
