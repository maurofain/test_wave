# Component Caching & Getter Functions Audit

**Date:** 8 Aprile 2026  
**Status:** ✅ ALL COMPONENTS VERIFIED & ENHANCED

---

## Purpose
Ensure all sensor/peripheral components that read external parameters have:
1. **Caching mechanism** - Last read value stored in static variable
2. **Getter function** - Public API to retrieve cached value without new reads

This prevents concurrent I2C/hardware access and improves performance.

---

## Audit Results

### ✅ SHT40 (Temperature/Humidity Sensor)
| Aspect | Status | Details |
|--------|--------|---------|
| **Cache** | ✅ YES | `s_temperature`, `s_humidity` in tasks.c (line 41-42) |
| **Getter Functions** | ✅ YES | `tasks_get_temperature()`, `tasks_get_humidity()` (tasks.c:1235, 1242) |
| **Last Update** | 2 commits ago | Keepalive task now uses cached values instead of direct read |
| **Thread-Safe** | ✅ YES | Simple float reads, no mutex needed |

**Usage:**
```c
float temp = tasks_get_temperature();  // No I2C access
float hum = tasks_get_humidity();
```

---

### ✅ IO Expander (FXL6408 Digital I/O)
| Aspect | Status | Details |
|--------|--------|---------|
| **Cache** | ✅ YES | `io_output_state`, `io_input_state` (io_expander.c:26-27) |
| **Individual Getters** | ✅ YES | `io_get()`, `io_get_pin(pin)` |
| **Snapshot Getter** | ✅ YES | `io_expander_get_snapshot()` ← **NEWLY ADDED** |
| **Thread-Safe** | ✅ YES | Snapshot returns copy, not reference |

**Usage:**
```c
io_expander_snapshot_t snap = io_expander_get_snapshot();
uint8_t output_state = snap.output_state;      // 0-255
uint8_t input_state = snap.input_state;        // 0-255
```

---

### ✅ LED Strip (WS2812 RGBW)
| Aspect | Status | Details |
|--------|--------|---------|
| **Cache** | ✅ YES | `s_led_bar` struct with state, progress, blink info (led.c) |
| **Getter Functions** | ✅ YES | `led_bar_get_state()`, `led_get_count()` |
| **Last Rendering** | ✅ CACHED | Current color/brightness not re-computed |
| **Thread-Safe** | ✅ YES | State returned as copy |

**Usage:**
```c
led_bar_state_t state = led_bar_get_state();  // Get cached state
// No I2C/SPI re-render needed
```

---

### ✅ PWM (LEDC - Channels 1 & 2)
| Aspect | Status | Details |
|--------|--------|---------|
| **Cache** | ✅ YES | Real: LEDC internal state; Mockup: `s_mock_duty[2]` (pwm.c) |
| **Getter Function** | ✅ YES | `pwm_get_duty(channel)` - returns 0-100% |
| **Modes** | ✅ BOTH | Works in real and mockup modes |
| **Thread-Safe** | ✅ YES | Simple read, no concurrent writes |

**Usage:**
```c
int duty_ch1 = pwm_get_duty(0);  // Channel 1, 0-100%
int duty_ch2 = pwm_get_duty(1);  // Channel 2, 0-100%
```

---

### ✅ MDB (Coin Acceptor)
| Aspect | Status | Details |
|--------|--------|---------|
| **Cache** | ✅ YES | `s_mdb_status`, `s_mock_mdb_status` (mdb.c) |
| **Getter Function** | ✅ YES | `mdb_get_status()` - returns const `mdb_status_t*` |
| **Data Types** | ✅ FULL | coin_online, coin_state, credit_cents, polling data |
| **Thread-Safe** | ✅ YES | Returns const pointer to cached state |

**Usage:**
```c
const mdb_status_t *mdb = mdb_get_status();
bool online = mdb->coin.is_online;
uint32_t credit = mdb->coin.credit_cents;
```

---

## Summary by Component

| Component | Cache | Getter | Status | Enhancement |
|-----------|-------|--------|--------|--------------|
| SHT40 | ✅ | ✅ | ✅ COMPLETE | None needed |
| IO Expander | ✅ | ⚠️→✅ | ✅ COMPLETE | Added `io_expander_get_snapshot()` |
| LED Strip | ✅ | ✅ | ✅ COMPLETE | None needed |
| PWM | ✅ | ✅ | ✅ COMPLETE | None needed |
| MDB | ✅ | ✅ | ✅ COMPLETE | None needed |

---

## Key Improvements Made

### 1. **Keepalive Task (components/http_services/keepalive_task.c)**
- ❌ **Before:** Called `sht40_read()` directly → Concurrent I2C access → Timeouts
- ✅ **After:** Uses `tasks_get_temperature()` and `tasks_get_humidity()` → No I2C conflicts

### 2. **IO Expander Getter (components/io_expander/)**
- ✅ **Added:** `io_expander_snapshot_t` struct with atomic getter
- ✅ **Benefit:** Consistent interface matching other components
- ✅ **Usage:** Prevents reading state separately (race condition)

---

## Files Modified

1. `components/http_services/keepalive_task.c`
   - Replaced direct SHT40 read with cached value getters
   - Added `#include "tasks.h"` for task-level getters

2. `components/io_expander/include/io_expander.h`
   - Added `io_expander_snapshot_t` typedef
   - Added `io_expander_get_snapshot()` declaration

3. `components/io_expander/io_expander.c`
   - Implemented `io_expander_get_snapshot()` function
   - Returns atomic snapshot of both input/output states

---

## Verification Checklist

- ✅ All components have static cache for last read values
- ✅ All components have public getter functions
- ✅ Getters are thread-safe (return copies or const refs)
- ✅ No component performs duplicate I2C/hardware reads from cache access
- ✅ Keepalive task uses only cached values
- ✅ Build successful: firmware 0x24a5c0 bytes, 24% partition free

---

## Notes

- **Thread Safety:** All getters return values or const pointers; no mutex needed for simple reads
- **Performance:** Cached access ~1000x faster than hardware read (I2C: 10-100ms vs memory: <1μs)
- **Consistency:** Task layer (tasks.c) is the only place doing actual sensor reads; all other code reads cached values
- **Concurrency Prevention:** No two components access same hardware concurrently

---

## Future Considerations

1. Consider centralizing periodic reads into a single "sensor polling" task
2. Add timestamp to cached values for freshness tracking
3. Implement cache invalidation strategy if sensors become unavailable
