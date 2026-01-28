# Note Hardware - ESP32-P4

## Boot Button - NON DISPONIBILE

### Motivo
Su questa scheda, **il boot button non può essere utilizzato** per il seguente motivo:

- **GPIO 35** è riservato per **TX_EN (Transmit Enable) della Ethernet RMII**
- Configurare GPIO 35 come input (anche con pull-up disabilitato) interferirebbe con il segnale TX_EN
- Il MAC Ethernet non potrebbe trasmettere dati correttamente

### Implementazione Rimossa
- Funzione `init_boot_button()` rimossa da `main/init.c`
- Configurazione `CONFIG_APP_BOOT_BUTTON_GPIO` rimossa da sdkconfig

### Alternativa
Se hai necessità di un boot button per il futuro, considera:
1. Usare un GPIO non riservato da Ethernet (es: GPIO 43, 44, 45, 46, 48, 49)
2. Implementare una funzione di recupero via HTTP (`/reset` endpoint)

## Pinout Ethernet RMII - ESP32-P4

```
GPIO 31:  MDC (Management Data Clock)
GPIO 52:  MDIO (Management Data I/O)
GPIO 35:  TX_EN (Transmit Enable) ⚠️ Non configurare
GPIO 36:  RXD0 (Receive Data 0)
GPIO 37:  RXD1 (Receive Data 1)
GPIO 38:  TXD0 (Transmit Data 0)
GPIO 39:  TXD1 (Transmit Data 1)
GPIO 40:  RX_CLK (Receive Clock)
GPIO 41:  TX_CLK (Transmit Clock)
GPIO 51:  PHY RESET
```

## GPIO Disponibili per Uso Generico
```
GPIO 0, 1, 2, 43, 44, 45, 46, 48, 49
```
(Elenco non esaustivo - verificare il datasheet ESP32-P4 per conferma)
