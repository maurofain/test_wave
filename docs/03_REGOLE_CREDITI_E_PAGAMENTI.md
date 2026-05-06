# Regole crediti e pagamenti (2026-05)

## Obiettivo

Definire regole **semplici e senza vincoli di sequenza** per l'accettazione e l'utilizzo del credito, indipendentemente dal canale di pagamento.

## Definizioni

- **Credito**: unità “coin” (1 coin = 1 €). Nel firmware viene gestito in unità intere; gli importi in centesimi vengono convertiti con residuo.
- **ECD**: credito da **monete** (coin acceptor / CCtalk).
- **VCD**: credito “virtuale” da **NFC/CARD (MDB cashless)** e **QR**.

## Regole di aggiunta credito

- **Sono sempre accettate ricariche** da tutti i sistemi disponibili: **monete**, **NFC/CARD**, **QR**.
- **Credito massimo complessivo**: **99 coin** (99 €).
  - Qualsiasi aggiunta viene **clampata** per non superare il massimale.
- **ECD (monete)**:
  - È possibile aggiungere monete **un numero illimitato di volte**, finché non si raggiunge il massimale.
- **VCD (NFC/CARD e QR)**:
  - Le ricariche VCD sono permesse **fino al raggiungimento del massimale** (stesso cap totale).

## Regole di consumo credito

Quando viene addebitato un ciclo/programma, il consumo segue questa priorità:

1. **ECD (monete)**
2. **VCD NFC/CARD**
3. **VCD QR**

Questa priorità vale per ogni addebito, fino ad esaurimento credito.

