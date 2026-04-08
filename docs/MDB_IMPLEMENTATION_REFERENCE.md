# Riferimento Implementazione Protocollo MDB
## Basato sull'analisi del progetto test_mdb

---

## Indice
1. [Struttura del Progetto](#struttura-del-progetto)
2. [Architettura Component MDB](#architettura-component-mdb)
3. [Strutture Dati](#strutture-dati)
4. [Protocollo di Comunicazione](#protocollo-di-comunicazione)
5. [Macchine a Stati](#macchine-a-stati)
6. [Integrazione in Main](#integrazione-in-main)
7. [Sistema di Configurazione](#sistema-di-configurazione)
8. [Gestione Messaggi](#gestione-messaggi)
9. [Testing & Diagnostica](#testing--diagnostica)
10. [Checklist Implementazione per test_wave](#checklist-implementazione-per-test_wave)

---

## Struttura del Progetto

### Layout File (reference test_mdb)
```
test_mdb/
├── components/
│   ├── mdb/
│   │   ├── include/
│   │   │   └── mdb.h              # API pubblica + enumerazioni + strutture
│   │   ├── mdb.c                  # Implementazione (740 righe)
│   │   ├── test/
│   │   │   ├── mdb_test.h         # Interfaccia testing
│   │   │   └── mdb_test.c         # Task di test
│   │   └── CMakeLists.txt         # Registrazione component
│   └── device_config/
│       ├── include/
│       │   └── device_config.h    # Strutture configurazione
│       └── device_config.c
├── main/
│   ├── main.c                     # Applicazione test (~900 righe)
│   └── CMakeLists.txt
└── docs/
    └── PK4_MDB_TEST.md            # Guida connessione & setup
```

### Punti di Integrazione Chiave
- **Component MDB** risiede in `components/mdb/` (riutilizzabile, portabile)
- **Device config** fornisce parametri di inizializzazione
- **Main loop** gestisce sequenze di test e polling
- **UART driver** di ESP-IDF gestisce comunicazione a 9-bit

---

## MDB Component Architecture

### Header File Structure (mdb.h)

#### 1. **Address Definitions**
```c
#define MDB_ADDR_COIN_CHANGER    0x10
#define MDB_ADDR_CASHLESS_1      0x10  // First cashless device
#define MDB_ADDR_CASHLESS_2      0x60  // Second cashless device
#define MDB_ADDR_BILL_VALIDATOR  0x30
```

#### 2. **Command Definitions**
```c
#define MDB_CMD_RESET            0x00
#define MDB_CMD_SETUP            0x01
#define MDB_CMD_POLL             0x02
#define MDB_CMD_VEND             0x03  (Cashless) / 0x05 (Coin)
#define MDB_CMD_ENABLE           0x04
#define MDB_CMD_REVALUE          0x05
#define MDB_CMD_EXPANSION        0x07
```

#### 3. **Response Codes (Cashless-specific)**
```c
#define CASHLESS_RESP_JUST_RESET       0x00
#define CASHLESS_RESP_READ_CONFIG      0x01
#define CASHLESS_RESP_DISPLAY_REQUEST  0x02
#define CASHLESS_RESP_BEGIN_SESSION    0x03
#define CASHLESS_RESP_SESSION_CANCEL   0x04
#define CASHLESS_RESP_VEND_APPROVED    0x05
#define CASHLESS_RESP_VEND_DENIED      0x06
#define CASHLESS_RESP_END_SESSION      0x07
#define CASHLESS_RESP_REQUEST_ID       0x09
#define CASHLESS_RESP_MALFUNCTION      0x0A
#define CASHLESS_RESP_OUT_OF_SEQUENCE  0x0B
#define CASHLESS_RESP_REVAL_APPROVED   0x0D
#define CASHLESS_RESP_REVAL_DENIED     0x0E
#define CASHLESS_RESP_REVAL_LIMIT      0x0F
```

#### 4. **Special Bytes**
```c
#define MDB_ACK                    0x00  // Acknowledgement
#define MDB_NAK                    0xFF  // Negative acknowledgement
#define MDB_RET                    0xAA  // Retry
```

#### 5. **Timeouts (Cashless)**
```c
#define CASHLESS_FIRST_BYTE_RESPONSE   25  // ms
#define CASHLESS_INTER_BYTE_RESPONSE    8  // ms
```

---

## Data Structures

### State Enumerations

#### Device State Machine
```c
typedef enum {
    MDB_STATE_INACTIVE = 0,
    MDB_STATE_INIT_RESET,
    MDB_STATE_INIT_SETUP,
    MDB_STATE_INIT_EXPANSION,
    MDB_STATE_INIT_ENABLE,
    MDB_STATE_IDLE_POLLING,
    MDB_STATE_ERROR
} mdb_device_state_t;
```

#### Vend Status (Transaction state)
```c
typedef enum {
    MDB_VEND_IDLE = 0,
    MDB_VEND_PENDING,
    MDB_VEND_WORKING,
    MDB_VEND_APPROVED,
    MDB_VEND_DENIED,
} mdb_vend_status_t;
```

#### Revalue Status (Refund/top-up state)
```c
typedef enum {
    MDB_REVALUE_IDLE = 0,
    MDB_REVALUE_PENDING,
    MDB_REVALUE_REQUEST_LIMIT,
    MDB_REVALUE_LIMIT,
    MDB_REVALUE_WORKING,
    MDB_REVALUE_APPROVED,
    MDB_REVALUE_DENIED,
    MDB_REVALUE_END,
} mdb_revalue_status_t;
```

### Main Status Structure

#### Cashless Device Structure
```c
typedef struct {
    mdb_device_state_t state;
    uint8_t address;                    // 0x10 or 0x60
    bool is_online;
    int error_count;
    uint32_t last_poll_ms;
    
    // Configuration data (from SETUP response)
    bool config_read;
    bool expansion_read;
    uint8_t feature_level;
    char manuf_code[4];                 // e.g., "MHI"
    char model_number[13];              // e.g., "PK4......"
    uint8_t manuf_version;
    
    // Session management
    bool session_open;
    uint32_t credit_cents;
    uint32_t request_price;             // Price requested for vend
    uint32_t approved_price;            // Price approved by terminal
    
    // Vend transaction state
    mdb_vend_status_t vend_status;
    uint32_t vend_sold;
    bool vend_failure;
    bool vend_abort;
    
    // Revalue transaction state
    mdb_revalue_status_t revalue_status;
    uint32_t revalue_price;
    uint32_t approved_revalue;
    uint32_t revalue_limit;
    
    // Payment media information
    mdb_cashless_key_type_t key_type;
    mdb_cashless_device_type_t device_type;
    uint32_t key_num;
    uint32_t key_price_group;
    
    // Features
    bool cash_sale_support;
    uint32_t session_timeout;
} mdb_cashless_device_t;
```

#### Global MDB Status
```c
typedef struct {
    struct {
        mdb_device_state_t state;
        uint32_t last_poll_ms;
        uint32_t credit_cents;
        bool is_online;
        uint8_t feature_level;
        uint16_t currency_code;
        uint8_t scaling_factor;
        uint8_t decimal_places;
        uint16_t coin_routing;
        uint8_t coin_values[16];
    } coin;
    struct {
        mdb_device_state_t state;
        bool is_online;
    } bill;
    struct {
        mdb_cashless_device_t devices[2];  // Devices at 0x10 and 0x60
    } cashless;
} mdb_status_t;
```

---

## Communication Protocol

### 9-Bit Protocol Implementation

The MDB bus uses **9-bit serial communication**:
- **Bits 0-7**: Data byte (8 bits)
- **Bit 8** (MSB): **Mode bit** - controls framing
  - **1 = Address byte** (command with device address)
  - **0 = Data or checksum byte**

### Simulating 9-Bit via Parity

Since ESP32 lacks native 9-bit support, MDB uses **dynamic parity switching**:

```c
// Helper function: calculate parity of a byte
static bool get_byte_parity(uint8_t data) {
    return __builtin_parity(data);
}

// To send bit 9 = 1:
// Calculate data parity
// If data parity is even (0): use ODD parity -> parity bit = 1
// If data parity is odd (1): use EVEN parity -> parity bit = 1

// To send bit 9 = 0:
// If data parity is even (0): use EVEN parity -> parity bit = 0
// If data parity is odd (1): use ODD parity -> parity bit = 0
```

### Packet Structure

**Master → Device:**
```
[ADDRESS (9b=1) | PAYLOAD_BYTE_0 (9b=0) | ... | CHECKSUM (9b=0)]
```

**Device → Master:**
```
[RESPONSE_BYTE_0 (9b=0) | RESPONSE_BYTE_1 (9b=0) | ... | CHECKSUM (9b=0)]
```

### Transmission Example (RESET command)
```
// Send RESET to cashless device at 0x10:
// Command byte = 0x10 | 0x00 = 0x10
// Payload = (empty)
// Checksum = 0x10

TX: [0x10 (9b=1) | 0x10 (9b=0)]  // Address + checksum (same when no payload)
RX: [0x00 (9b=0) | 0x00 (9b=0)]  // "Just Reset" response + checksum
```

### API Functions (mdb.c)

```c
// Initialize UART for MDB communication
esp_err_t mdb_init(void);

// Send a full MDB packet (with auto checksum)
esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len);

// Send raw byte with explicit bit 9 control
esp_err_t mdb_send_raw_byte(uint8_t data, bool mode_bit);

// Receive a response packet (with checksum validation)
esp_err_t mdb_receive_packet(uint8_t *out_data, size_t max_len, 
                             size_t *out_len, uint32_t timeout_ms);

// Get current status
const mdb_status_t* mdb_get_status(void);
```

---

## State Machines

### Coin Changer State Machine

Located in: `mdb_coin_sm()` function

**Sequence:**
1. **INACTIVE** → Enabled in config → **INIT_RESET**
2. **INIT_RESET** → Send RESET → Wait 500ms → **INIT_SETUP**
3. **INIT_SETUP** → Send SETUP, parse response → **INIT_ENABLE**
   - Extract: feature level, currency, scaling factor, decimals, coin values
   - Send ACK on success
4. **INIT_ENABLE** → Send ENABLE (enable all coin types) → **IDLE_POLLING**
   - If timeout 3x → Disable and return to **INACTIVE**
5. **IDLE_POLLING** → Send POLL every cycle
   - ACK response = no events (stay polling)
   - Data response = parse coin events
     - Extract coin type and routing (tube vs. hopper)
     - Calculate coin value = `coin_values[type] * scaling_factor`
     - Update credit
   - Error count >= 20 → **INIT_RESET** (reinitialize)

**Key Parameters:**
- Poll interval: 90ms (normal) or 2500ms (error state)
- Error retry threshold: 20 consecutive timeouts
- Setup retry limit: 5 attempts

### Cashless State Machine

Located in: `mdb_cashless_sm(device_index)` function
**Operates for two devices**: 0x10 and 0x60

**Sequence:**
1. **INACTIVE** → Enabled in config → **INIT_RESET**
2. **INIT_RESET** → Send RESET, wait 500ms → **INIT_SETUP**
3. **INIT_SETUP** → Send SETUP
   - Parse: feature_level (byte 0), capabilities (byte 7)
   - Check bit 3 of byte 7 for cash sale support
   - Send ACK
   - → **INIT_EXPANSION**
   - Retry 3x on timeout → **INIT_RESET**
4. **INIT_EXPANSION** → Send EXPANSION (request ID)
   - Parse: manuf_code (bytes 1-3), model (bytes 4-15), version (byte 16)
   - Set `expansion_read = true`
   - Send ACK
   - → **INIT_ENABLE**
5. **INIT_ENABLE** → Send ENABLE
   - Wait for ACK response
   - `is_online = true`
   - → **IDLE_POLLING**
6. **IDLE_POLLING** → Send POLL, parse response
   - **ACK (0x00)** = No activity, stay polling
   - **Data response** = Parse cashless response code:
     - `0x03` = **BEGIN_SESSION**: Extract credit, session ID, card ID
     - `0x04` = **SESSION_CANCEL**: Close session
     - `0x05` = **VEND_APPROVED**: Extract approved amount, execute transaction
     - `0x06` = **VEND_DENIED**: Handle denial, close session
     - `0x07` = **END_SESSION**: Finalize
     - `0x09` = **REQUEST_ID**: Device identification
     - Send ACK after data response
   - Error count >= 20 → **INIT_RESET** (reinitialize)

**Poll interval:** 90ms (normal) or 2500ms (with errors)

### Session State Machine (test_mdb example)

```c
typedef enum {
    SESSION_IDLE,                      // No session
    SESSION_OPEN,                      // BEGIN_SESSION received, waiting for article selection
    SESSION_ARTICLE_SELECTED,          // Article selected, ready for VEND_REQUEST
    SESSION_VEND_REQUESTED,            // VEND_REQUEST sent, waiting for response
    SESSION_VEND_APPROVED,             // VEND_APPROVED received, processing payment
    SESSION_ENDING
} cashless_session_state_t;
```

---

## Integration in Main

### Initialization Sequence

```c
// 1. Initialize MDB UART
esp_err_t ret = mdb_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "mdb_init failed: %s", esp_err_to_name(ret));
    return;
}

// 2. Initialize device configuration
device_config_init();

// 3. Start MDB polling engine (usual via tasks.c)
xTaskCreate(mdb_engine_run, "mdb_engine", 4096, NULL, 5, NULL);
```

### Main Loop Structure (test_mdb)

```c
void app_main(void) {
    mdb_init();
    boot_button_init();
    
    while (1) {
        // Phase 1: RESET
        send_and_receive("RESET", 0x10, NULL, 0, 300);
        // Phase 2: SETUP
        send_prebuilt_frame_and_receive("SETUP", setup_frame, size, 900);
        // Phase 3: EXPANSION
        send_and_receive("EXPANSION", 0x17, expansion_payload, size, 900);
        // Phase 4: ENABLE
        send_and_receive("ENABLE", 0x14, enable_payload, size, 300);
        
        // Phase 5: Continuous polling with event handling
        while (1) {
            if (boot_pressed()) {
                // Execute vend command with VEND_REQUEST
                send_vend_command("VEND", VEND_REQUEST, amount, item_number);
            }
            send_poll_and_auto_ack("POLL", 120);
        }
    }
}
```

### Task Structure

From `tasks.c` (not in test_mdb but mentioned):
```c
// In tasks.c, MDB polling engine runs as:
xTaskCreate(mdb_engine_run, "mdb_engine", STACK_SIZE, NULL, PRIORITY, NULL);

// Inside mdb_engine_run():
void mdb_engine_run(void *arg) {
    while (1) {
        mdb_coin_sm();
        mdb_cashless_sm(0);  // Device at 0x10
        mdb_cashless_sm(1);  // Device at 0x60
        vTaskDelay(pdMS_TO_TICKS(500));  // Polling cycle every 500ms
    }
}
```

---

## Configuration System

### device_config.h

Centralized configuration structure:

```c
typedef struct {
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint16_t rx_buf_size;
    uint16_t tx_buf_size;
} device_serial_config_t;

typedef struct {
    bool coin_acceptor_en;
    bool cashless_en;
} device_mdb_config_t;

typedef struct {
    device_serial_config_t mdb_serial;
    device_mdb_config_t mdb;
} device_config_t;
```

### Typical Configuration Values

```c
// In device_config.c:
device_config_t g_device_config = {
    .mdb_serial = {
        .baud_rate = 9600,
        .stop_bits = 1,
        .rx_buf_size = 256,
        .tx_buf_size = 256,
    },
    .mdb = {
        .coin_acceptor_en = true,      // Enable coin module
        .cashless_en = true,            // Enable cashless devices
    }
};

device_config_t *device_config_get(void) {
    return &g_device_config;
}
```

### UART GPIO Configuration (hardcoded in mdb.c)

```c
#define MDB_UART_PORT   2       // UART 2
#define MDB_TX_GPIO     22      // TX pin
#define MDB_RX_GPIO     23      // RX pin
```

These can be moved to `sdkconfig` or `device_config.h` for flexibility.

---

## Message Handling

### Cashless Response Parsing

In test_mdb's main loop, responses are decoded:

```c
// BEGIN_SESSION (0x03)
if (resp_code == 0x03 && len >= 11) {
    uint16_t session_id = (data[2] << 8) | data[3];
    uint16_t credit = (data[1] << 8) | data[2];
    // Extract payment media ID, key type, etc.
}

// VEND_APPROVED (0x05)
if (resp_code == 0x05 && len >= 3) {
    uint16_t approved_amount = (data[1] << 8) | data[2];
    // Execute payment logic
}

// REQUEST_ID (0x09)
if (resp_code == 0x09 && len >= 29) {
    char manuf[4], model[13];
    strncpy(manuf, (const char*)(data + 1), 3);
    strncpy(model, (const char*)(data + 4), 12);
    // Store device identity
}
```

### VEND Command Format

```c
// VEND command structure:
// [0x13 (cmd) | subcommand | amount_h | amount_l | item_h | item_l | checksum]

#define VEND_REQUEST            0x00
#define VEND_CANCEL             0x01
#define VEND_SUCCESS            0x02
#define VEND_FAILURE            0x03
#define VEND_SESSION_COMPLETE   0x04
#define CASH_SALE               0x05

// Example: Request €10.00 for item 0x0001
uint8_t payload[5] = {
    0x00,           // VEND_REQUEST
    0x03,           // amount_h (1000 cents = 0x03E8)
    0xE8,           // amount_l
    0x00,           // item_h (0x0001)
    0x01            // item_l
};
mdb_send_packet(0x10 | 0x03, payload, 5);  // 0x13 = command byte
```

### ACK Protocol

**When to send ACK:**
- After receiving data from POLL (not after simple ACK response)
- As master: Use `mdb_send_raw_byte(0x00, false)` (9-bit = 0)

**Real implementation rule:**
```c
if (resp_len > 1 || (resp[0] != MDB_ACK && resp[0] != MDB_NAK)) {
    // Send master ACK
    mdb_send_raw_byte(0x00, false);
}
```

---

## Testing & Diagnostics

### Logging System (test_mdb)

Structured logging with phase names:
```
[M] RESET_110: 1100 (9b=1)
[M] RESET_110: RX 0000 #JUST_RESET
[M] SETUP_111: 1100 0002 0000 13 (9b=1)
[M] SETUP_111: RX 0102 0304 0506 0708 ... #CONFIG
```

**Log Deduplication:** Prevents spam from repetitive "no activity" polls

### Hardware Verification Checklist

```
1. [ ] GPIO pins: TX=22, RX=23
2. [ ] Ground common between controller and PK4
3. [ ] Baudrate: 9600 8N1
4. [ ] No conflicts with other peripherals
5. [ ] Device powered and responding
6. [ ] Check inverted TX/RX if no response
```

### Serial Monitor Output Example

```
[MDB] Inizializzazione MDB su UART 2 (TX:22 RX:23)
[MDB] Motore di polling MDB avviato
[MDB] Cashless 0x10: Invio RESET...
[MDB] Cashless 0x10: Invio SETUP...
[MDB] Cashless 0x10 SETUP: Feature Level 3
[MDB] Cashless 0x10: Expansion Request ID...
[MDB] Cashless 0x10: Manuf Code = MHI
[MDB] Cashless 0x10: Invio ENABLE...
[MDB] Cashless 0x10: Abilitato!
[MDB] Cashless 0x10: Evento ricevuto (11 byte)
[!] Cashless 0x00: BEGIN_SESSION | Feature=3, Session=0x0100
```

---

## Implementation Checklist for test_wave

### Phase 1: Component Setup
- [ ] Copy or reference `mdb.h` from test_mdb
- [ ] Integrate `mdb.c` with all device state management
- [ ] Add `test/mdb_test.h` and `test/mdb_test.c` for unit testing
- [ ] Ensure `CMakeLists.txt` registers component with dependencies

### Phase 2: Device Configuration
- [ ] Create/update `device_config.h` with MDB settings
- [ ] Define UART GPIO pins (TX/RX) matching hardware
- [ ] Set baudrate, stop bits, buffer sizes
- [ ] Implement `device_config_get()`

### Phase 3: Initialization
- [ ] Call `mdb_init()` in `init.c` or startup sequence
- [ ] Verify UART driver installation
- [ ] Check GPIO configuration

### Phase 4: Task Integration
- [ ] Add `mdb_engine_run()` entry to `tasks.c`
- [ ] Set priority and stack size appropriately
- [ ] Ensure startup after `device_config_init()`

### Phase 5: FSM Integration
- [ ] Integrate `mdb_cashless_sm()` calls into main polling loop
- [ ] Integrate `mdb_coin_sm()` if coin acceptor needed
- [ ] Add timeout handling for error states

### Phase 6: Message Handling
- [ ] Define application-specific vend request logic
- [ ] Implement session state management
- [ ] Add HTTP or WebUI endpoints for MDB control (if needed)

### Phase 7: Testing & Logging
- [ ] Verify RESET/SETUP/EXPANSION/ENABLE sequence
- [ ] Test POLL cycle and event reception
- [ ] Validate checksum calculations
- [ ] Check timeout handling and recovery

### Phase 8: Documentation
- [ ] Update component README
- [ ] Document GPIO pinout in project docs
- [ ] Add troubleshooting guide
- [ ] Create integration examples

---

## Key Implementation Notes for test_wave

### 1. Mock vs. Real Implementation
```c
// Flag to enable/disable actual MDB communication:
#ifndef DNA_MDB
#define DNA_MDB 0  // 0 = real, 1 = mock
#endif

#if DNA_MDB == 0
    // Real UART implementation
#else
    // Mock stubs (for development without hardware)
#endif
```

### 2. Logging Prefixes (per project conventions)
```c
[M]  // Main app
[F]  // Factory mode
[C]  // Common/Component
[!]  // Important/Event
```

### 3. Error Handling Strategy
- Timeouts on SETUP/EXPANSION → Retry 3x
- Consecutive timeouts on POLL → Switch to error state (2500ms intervals)
- Error count >= 20 → Full reinitialization

### 4. Credit Management
- Store credit in **cents** (uint32_t)
- Display as: `credit_cents / 100.0` EUR
- Validate never goes negative

### 5. Session Timeout
- Implement `session_start_time` tracking
- Define reasonable timeout (e.g., 60 seconds of inactivity)
- Auto-cancel session if timeout exceeded

### 6. Multi-Device Support
- Maintain separate state for device 0x10 and 0x60
- Poll both in same cycle (500ms interval)
- Allow enabling/disabling via configuration

---

## Files to Reference

From test_mdb:
- [mdb.h](file:///home/mauro/1P/MicroHard/test_mdb/components/mdb/include/mdb.h) - All definitions
- [mdb.c](file:///home/mauro/1P/MicroHard/test_mdb/components/mdb/mdb.c) - Core implementation (740 lines)
- [mdb_test.c](file:///home/mauro/1P/MicroHard/test_mdb/components/mdb/test/mdb_test.c) - Testing
- [main.c](file:///home/mauro/1P/MicroHard/test_mdb/main/main.c) - Full integration example (~900 lines)
- [device_config.h](file:///home/mauro/1P/MicroHard/test_mdb/components/device_config/include/device_config.h) - Config template
- [PK4_MDB_TEST.md](file:///home/mauro/1P/MicroHard/test_mdb/docs/PK4_MDB_TEST.md) - Hardware setup

---

## Quick Reference: MDB Packet Format

| Component | Size | Description |
|-----------|------|-------------|
| **Address/Command** | 1 byte | `(device_addr << 3) \| (cmd_offset)` with 9b=1 |
| **Payload** | variable | Optional data bytes with 9b=0 |
| **Checksum** | 1 byte | Sum of all bytes (address + payload) with 9b=0 |

**Example (RESET to cashless 0x10):**
```
TX: [0x10(9b=1) | 0x10(9b=0)]
    ↑ Address   ↑ Checksum (same when no payload)
    
RX: [0x00(9b=0) | 0x00(9b=0)]
    ↑ "Just Reset" response
```

---

**Document Version:** 1.0  
**Updated:** 2026-04-08  
**Reference Implementation:** test_mdb/components/mdb  
**Target Application:** test_wave
