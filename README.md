# ESP-IDF starter for ESP32-P4 Waveshare module

This project targets the Waveshare ESP32-P4 Module Dev Kit and an expansion board wired as described below. It initializes the basic peripherals (I2C bus, WS2812 strip, RS232/RS485/MDB UARTs, PWM outputs, BOOT button) and adds a **factory app** that exposes Wi-Fi AP (+optional STA), optional Ethernet, HTTP endpoints, and OTA to recover/refresh the main firmware in `ota_0`.

[Specifiche](docs/specifiche.md)

[RULES](docs/RULES.md)

## Hardware summary (per HARDWARE_SETUP)
- I2C bus (SDA GPIO27, SCL GPIO26) for 26LC16 EEPROM, two FXL6408 IO expanders, SHT40 sensor.
- WS2812 RGB strip on GPIO5.
- RS232: TX GPIO36, RX GPIO46.
- RS485: TX GPIO4, RX GPIO19, DE GPIO21.
- PWM outputs: OUT1 GPIO47, OUT2 GPIO48.
- MDB: RX GPIO23, TX GPIO22.
- BOOT button: GPIO35.

## Partitions
The provided custom table in [partitions.csv](partitions.csv) is used by default:
- `factory` (3 MB): recovery firmware (this app).
- `ota_0` (10 MB): main application slot.
- `nvs`, `phy_init`, `otadata`, `storage` (SPIFFS) as support partitions.

## Factory networking
- Wi-Fi AP always on: SSID `factory-setup`, password `factory123` (open if empty), channel configurable.
- Wi-Fi STA optional (enable and set credentials in `idf.py menuconfig`).
- Ethernet optional (RMII PHY pins configurable; disabled by default).
- HTTP server on port 80 (configurable).

HTTP endpoints:
- `GET /status` → JSON with running/boot partition and IPs (AP/STA/Ethernet).
- `POST /ota?url=<http(s)://...>` → triggers OTA to `ota_0` and reboots on success. If no query is provided, uses `APP_OTA_DEFAULT_URL` when set.

## Build
1. Install ESP-IDF (v5.1+ recommended for ESP32-P4) and set `IDF_PATH`/`idf.py` in PATH.
2. Configure (adjust pins if needed):
   ```
   idf.py menuconfig
   ```
3. Build and flash:
   ```
   idf.py set-target esp32p4
   idf.py build
   idf.py -p <PORT> flash monitor
   ```

## Notes
- WS2812 uses the RMT-based led_strip helper and blinks the first LED to indicate the app is alive.
- RS485 and MDB UARTs are only initialized if the target exposes enough UART controllers; warnings are logged otherwise.
- Replace the SHT40 stub with the actual driver when available.
- Ethernet driver uses the internal EMAC + RMII (LAN8720 by default); adjust pins/PHY type as needed or disable via Kconfig if not present.
- OTA over plain HTTP is allowed by default for lab convenience; change to HTTPS in production and provide the server certificate in Kconfig.

## Documentazione Hardware
Per note specifiche su limitazioni hardware e pinout, vedi [docs/NOTES_HW.md](docs/NOTES_HW.md).
