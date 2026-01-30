# Documento di Configurazione PINOUT - ESP32-P4 (test_wave)

Questo documento riassume l'assegnazione di tutti i GPIO utilizzati nel progetto, basandosi sulla configurazione `sdkconfig` e sugli ultimi schemi hardware forniti (MicroSD).

## 🔌 Alimentazione e Sistema
| Pin | Funzione | Note |
|---|---|---|
| **GPIO 45** | SD Power Enable | Gate P-MOS (Q1). **LOW = ON**. |
| **GPIO 32** | Ethernet PHY Reset | Pin di reset hardware per il PHY Ethernet. |
| **GPIO 51** | Ethernet PHY Interrupt | Interrupt (opzionale). |

## 💾 Memoria: MicroSD (Slot 0 - 4-bit Mode)
| Pin | Funzione SD | Direzione | Note |
|---|---|---|---|
| **GPIO 44** | CMD | I/O | Protocollo SDMMC CMD |
| **GPIO 43** | CLK | Output | Clock SDMMC |
| **GPIO 39** | D0 | I/O | Data line 0 |
| **GPIO 40** | D1 | I/O | Data line 1 |
| **GPIO 41** | D2 | I/O | Data line 2 |
| **GPIO 42** | D3 | I/O | Data line 3 |

## 🌐 Connettività Ethernet (RMII - Waveshare)

| Segnale | GPIO | Descrizione |
|:---|:---:|:---|
| **TX_EN** | **49** | Abilitazione Trasmissione |
| **TXD0** | **34** | Dati Trasmissione 0 |
| **TXD1** | **35** | Dati Trasmissione 1 |
| **REF_CLK** | **50** | Reference Clock (50MHz) |
| **RXD0** | **29** | Dati Ricezione 0 |
| **RXD1** | **30** | Dati Ricezione 1 |
| **CRS_DV** | **28** | Carrier Sense / Data Valid |
| **MDC** | **31** | Management Data Clock |
| **MDIO** | **52** | Management Data Input/Output |
| **PHY_RST** | **32** | Reset del chip PHY |
| **PHY_INT** | **51** | Interrupt (opzionale) |

## 📟 Comunicazione Seriale
| Periferica | Pin TX | Pin RX | Altri Pin | Note |
|---|---|---|---|---|
| **RS232** | GPIO 36 | GPIO 46 | - | UART 1 |
| **RS485** | GPIO 4 | GPIO 19 | GPIO 21 (DE) | DE = Driver Enable |
| **MDB** | GPIO 22 | GPIO 23 | - | Protocollo MDB |

## 🚌 Bus Sensori e I/O Expander (I2C 0)
| Pin | Funzione | Note |
|---|---|---|
| **GPIO 27** | SDA | Data line |
| **GPIO 26** | SCL | Clock line |

## 💡 Indicatori e Output
| Pin | Funzione | Note |
|---|---|---|
| **GPIO 5** | WS2812 LED | Segnale Data per striscia LED RGB |
| **GPIO 47** | PWM 1 (LCD) | Utilizzato per il controllo luminosità display |
| **GPIO 48** | PWM 2 | Disponibile per altri usi |

## 🛠️ Note Importanti
1. **Conflitto Pin 35**: GPIO 35 è mappato come `TXD1` per Ethernet. Poiché questo pin è collegato al pulsante di boot sulla Waveshare, il pulsante NON DEVE ESSERE USATO per evitare disturbi alla trasmissione Ethernet.
2. **SD Mode**: Configurato in **4-bit mode** (GPIO 39-42) per massime prestazioni. La nuova mappatura Ethernet RMII libera questi pin rispetto alle versioni precedenti del progetto.
3. **GPIO 36**: Usato per **RS232 TX** come da schema finale.

## 🟢 Mappatura Completa GPIO ESP32P4 ed Expansion Header (40-pin)
*La seguente tabella elenca tutti i GPIO del SoC ESP32-P4, il loro utilizzo sulla scheda Waveshare e il corrispondente pin sul connettore di espansione 40-pin.*

| GPIO | PIN 40-pin | Nome / Funzione | Descrizione / Progetto | Stato |
|:---:|:---:|---|---|:---:|
| **0** | **24** | GPIO 0 | Digital I/O (Ex RS232 TX) | LIBERO |
| **1** | **21** | GPIO 1 | Digital I/O | LIBERO |
| **2** | **22** | GPIO 2 | Digital I/O | LIBERO |
| **3** | **20** | GPIO 3 | Digital I/O | LIBERO |
| **4** | **17** | **RS485 TX** | Trasmissione dati RS485 | **USATO** |
| **5** | **15** | **WS2812** | Segnale per LED RGB indirizzabili | **USATO** |
| **6** | **16** | GPIO 6 | Digital I/O | LIBERO |
| **7** | **4** | GPIO 7 | Digital I/O | LIBERO |
| **8** | **6** | GPIO 8 | Digital I/O | LIBERO |
| 9 | - | GPIO 9 | Digital I/O | LIBERO |
| 10 | - | GPIO 10 | Non riportato su header | - |
| 11 | - | GPIO 11 | Non riportato su header | - |
| 12 | - | GPIO 12 | Non riportato su header | - |
| 13 | - | GPIO 13 | Non riportato su header | - |
| 14 | - | GPIO 14 | Digital I/O | LIBERO |
| 15 | - | GPIO 15 | Digital I/O | LIBERO |
| 16 | - | GPIO 16 | Digital I/O | LIBERO |
| 17 | - | GPIO 17 | Non riportato su header | - |
| 18 | - | GPIO 18 | Non riportato su header | - |
| **19** | - | **RS485 RX** | Ricezione dati RS485 | **USATO** |
| **20** | **14** | GPIO 20 | Digital I/O | LIBERO |
| **21** | **12** | **RS485 DE** | Driver Enable (Controllo direzione 485) | **USATO** |
| **22** | **11** | **MDB TX** | Trasmissione dati Master MDB | **USATO** |
| **23** | **8** | **MDB RX** | Ricezione dati Master MDB | **USATO** |
| **24** | **28** | GPIO 24 / USB_P | Condiviso USB | LIBERO |
| **25** | **27** | GPIO 25 / USB_N | Condiviso USB | LIBERO |
| **26** | **32** | **I2C SCL** | Clock Bus I2C (EEPROM / Expander) | **USATO** |
| **27** | **37** | **I2C SDA** | Dati Bus I2C (EEPROM / Expander) | **USATO** |
| **28** | - | **ETH CRS_DV** | Carrier Sense / Data Valid | **USATO** |
| **29** | - | **ETH RXD0** | Dati Ricezione Ethernet 0 | **USATO** |
| **30** | - | **ETH RXD1** | Dati Ricezione Ethernet 1 | **USATO** |
| **31** | - | **ETH MDC** | Management Data Clock | **USATO** |
| **32** | **25** | **ETH RESET** | Reset Hardware PHY Ethernet | **USATO** |
| **33** | **30** | **GPIO 33** | Digital Configurable I/O | **CONFIG** |
| **34** | - | **ETH TXD0** | Dati Trasmissione Ethernet 0 | **USATO** |
| **35** | - | **ETH TXD1** | Dati Trasmissione Ethernet 1 | **USATO** |
| **36** | **23** | **RS232 TX** | Trasmissione dati RS232 | **USATO** |
| **37** | **7** | GPIO 37 | Digital I/O | LIBERO |
| **38** | **9** | GPIO 38 | Digital I/O | LIBERO |
| **39** | - | **SD D0** | Linea Dati MicroSD (4-bit Mode) | **USATO** |
| **40** | - | **SD D1** | Linea Dati MicroSD (4-bit Mode) | **USATO** |
| **41** | - | **SD D2** | Linea Dati MicroSD (4-bit Mode) | **USATO** |
| **42** | - | **SD D3** | Linea Dati MicroSD (4-bit Mode) | **USATO** |
| **43** | - | **SD CLK** | Clock per MicroSD | **USATO** |
| **44** | - | **SD CMD** | Segnale Command MicroSD | **USATO** |
| **45** | **39** | **SD_PWR** | Controllo Gate P-MOS Alimentazione SD | **USATO** |
| **46** | **35** | **RS232 RX** | Ricezione dati RS232 | **USATO** |
| **47** | **38** | **PWM 1** | Luminosità Backlight LCD | **USATO** |
| **48** | **34** | **PWM 2** | Canale PWM ausiliario | **USATO** |
| **49** | - | **ETH TX_EN** | Trasmissione Enable Ethernet | **USATO** |
| **50** | - | **ETH REF_CLK** | Clock 50MHz (RMII) | **USATO** |
| **51** | - | **ETH PHY INT** | Interrupt PHY Ethernet (Opzionale) | **USATO** |
| **52** | - | **ETH MDIO** | Management Data I/O | **USATO** |
| **53** | **36** | GPIO 53 | Digital I/O | LIBERO |
| **54** | **31** | GPIO 54 | Digital I/O | LIBERO |

*Nota: La numerazione dei PIN si riferisce alla posizione fisica sul connettore maschio 2x20.*

## ️ Waveshare Interface (Tabella P6 dallo Schema)

| PIN | Segnale (Schema) | PIN | Segnale (Schema) |
|:---:|:---|:---:|:---|
| **1** | **VCC_5V** | **2** | **ESP_3V3** |
| **3** | **VCC_5V** | **4** | **GPIO 7** |
| **5** | **GND** | **6** | **GPIO 8** |
| **7** | **GPIO 37** | **8** | **GPIO 23** |
| **9** | **GPIO 38** | **10** | **GND** |
| **11** | **GPIO 22** | **12** | **GPIO 21** |
| **13** | **GND** | **14** | **GPIO 20** |
| **15** | **GPIO 5** (WS2812) | **16** | **GPIO 6** |
| **17** | **GPIO 4** | **18** | **ESP_3V3** |
| **19** | **GND** | **20** | **GPIO 3** |
| **21** | **GPIO 1** | **22** | **GPIO 2** |
| **23** | **GPIO 36** (RS232 TX) | **24** | **GPIO 0** |
| **25** | **GPIO 32** (ETH RST) | **26** | **GND** |
| **27** | **GPIO 25** / USB_P | **28** | **GPIO 24** / USB_N |
| **29** | **GND** | **30** | **GPIO 33** |
| **31** | **GPIO 54** | **32** | **GPIO 26** (I2C SCL) |
| **33** | **GND** | **34** | **GPIO 48** (PWM 2) |
| **35** | **GPIO 46** (RS232 RX) | **36** | **GPIO 53** |
| **37** | **GPIO 27** (I2C SDA) | **38** | **GPIO 47** (PWM 1) |
| **39** | **GPIO 45** (SD_PWR) | **40** | **GND** |

## 🔌 Pin di Alimentazione (Header 40-pin)

| PIN | Funzione | Note |
|:---:|:---:|---|
| 2, 18 | **+3.3V** | Alimentazione sistema (ESP_3V3) |
| 1, 3 | **+5V** | Alimentazione periferiche (VCC_5V) |
| 5, 10, 13, 19, 26, 29, 33, 40 | **GND** | Riferimento massa |

