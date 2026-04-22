# Analisi Ricezione Dati Porte Seriali

> Data: 4 aprile 2026  
> Progetto: MicroHard / test_wave

## RS232 & RS485 — Interrupt-driven (con lettura bloccante)

Entrambi usano il driver UART di ESP-IDF (`uart_driver_install()` + `uart_read_bytes()`).

- **Hardware:** la ricezione è **interrupt-driven** — l'ISR della UART riempie automaticamente un ring buffer in RAM ad ogni byte ricevuto.
- **Applicazione:** `uart_read_bytes()` **non fa polling attivo** — si mette in attesa su un semaforo FreeRTOS con timeout. La task si sblocca solo quando l'ISR segnala che ci sono dati nel buffer.
- **Timeout hardware RX:** entrambi configurano `uart_set_rx_timeout()` a ~10ms (calcolato dal baud rate) per rilevare la fine trama.

## USB CDC Scanner — Callback-driven (event-driven)

Usa il driver `usb_host_cdc_acm`. La ricezione avviene tramite la callback `cdc_data_cb()` registrata sul driver USB host.

- La task USB host gestisce i trasferimenti USB internamente e **chiama la callback** quando arrivano dati — completamente event-driven, nessun polling.

## serial_test — Test/monitor (usa uart_read_bytes)

Componente ausiliario per il testing, usa `uart_read_bytes()` come RS232/RS485 — stesso meccanismo interrupt-driven.

## Riepilogo

| Componente      | Meccanismo RX                                              |
|-----------------|------------------------------------------------------------|
| RS232           | Interrupt (ISR → ring buffer) + attesa bloccante FreeRTOS |
| RS485           | Interrupt (ISR → ring buffer) + attesa bloccante FreeRTOS |
| USB CDC Scanner | Callback event-driven (USB host stack)                     |
| serial_test     | Interrupt + attesa bloccante FreeRTOS                      |

**Nessun componente usa polling attivo (busy-wait).**
