# Analisi minimizzazione config JSON per EEPROM

Sorgente analizzato: `docs/config.json`.

## Nota importante

Nel file è presente un errore di sintassi JSON:

- `"ulang": "it",l`

Per l'analisi è stata applicata una sanitizzazione minima (`...,l` -> `...,`).

## Misure byte (UTF-8)

- Originale formattato: **2790 B**
- Solo minify (stessa semantica): **1609 B**
- Minify + pruning campi vuoti/default: **1278 B**
- Minify + pruning + remap chiavi corte: **999 B**

Risparmio vs originale:

- Minify: **-1181 B**
- Pruning: **-1512 B**
- Remap: **-1791 B**

## File generati

- `docs/config.min.json` (semantica invariata)
- `docs/config.pruned.min.json` (drop campi vuoti/default)

## Strategia consigliata (ordine)

1. **Minify sempre** prima del salvataggio EEPROM.
2. **Pruning controllato**: non salvare campi uguali ai default firmware.
3. **Schema compatto con dizionario chiavi** solo se necessario (richiede encoder/decoder compatibili).

## Ulteriori ottimizzazioni ad alto impatto

- Convertire stringhe esadecimali (`"0x1EAB"`) in numeri (`7851`) per ridurre byte.
- Evitare memorizzazione di IP vuoti e flag false quando il default firmware è equivalente.
- Per blocchi RGB LED, valutare array invece di chiavi verbose (`[r,g,b]`) se puoi cambiare schema.
- Valutare formato binario (CBOR/MessagePack) se il budget EEPROM è molto stretto.

## Compatibilità

- `config.min.json`: retrocompatibile senza modifiche firmware.
- `config.pruned.min.json`: richiede che il loader applichi i default mancanti in modo robusto (nel progetto è già in buona parte così).
- Schema remap chiavi: richiede modifica firmware esplicita.
