# Note Hardware - ESP32-P4

## Boot Button - NON DISPONIBILE

### Motivo
Su questa scheda, **il boot button non può essere utilizzato** per il seguente motivo:

- **GPIO 35** è riservato per **TXD1 (Dati Trasmissione 1) della Ethernet RMII**
- Configurare GPIO 35 come input (anche con pull-up disabilitato) interferirebbe con la trasmissione dei dati.
- Il MAC Ethernet non potrebbe trasmettere dati correttamente.

### Implementazione Rimossa
- Funzione `init_boot_button()` rimossa da `main/init.c`
- Configurazione `CONFIG_APP_BOOT_BUTTON_GPIO` rimossa da sdkconfig

### Alternativa
Se hai necessità di un boot button per il futuro, considera:
1. Usare un GPIO non riservato da Ethernet (es: GPIO 43, 44, 45, 46, 48)
2. Implementare una funzione di recupero via HTTP (`/reset` endpoint)

## Pinout Ethernet RMII - ESP32-P4 (Waveshare)

```
GPIO 31:  MDC (Management Data Clock)
GPIO 52:  MDIO (Management Data I/O)
GPIO 49:  TX_EN (Transmit Enable)
GPIO 34:  TXD0 (Dati Trasmissione 0)
GPIO 35:  TXD1 (Dati Trasmissione 1) -> Conflitto Boot Button
GPIO 29:  RXD0 (Dati Ricezione 0)
GPIO 30:  RXD1 (Dati Ricezione 1)
GPIO 28:  CRS_DV (Carrier Sense / Data Valid)
GPIO 50:  50M_CLK (Clock di riferimento 50MHz)
GPIO 32:  PHY RESET
GPIO 51:  PHY INTERRUPT (Opzionale)
```


