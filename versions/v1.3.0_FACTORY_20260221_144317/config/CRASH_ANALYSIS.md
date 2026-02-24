# CRASH_ANALYSIS

Guida operativa completa per analizzare crash, stack trace e backtrace nel progetto `test_wave`.

## 1) Obiettivo

Questo documento descrive:
- come viene generato un crash intenzionale;
- dove finiscono i dati del crash;
- come ottenere lo stack chiamate completo;
- come diagnosticare i problemi più comuni del coredump;
- come validare il comportamento del boot guard (reboot consecutivi).

## 2) Architettura del flusso crash

Flusso attuale implementato:
1. Il panic avviene (crash reale, ad esempio fault memoria).
2. ESP-IDF salva il coredump nella partizione `coredump` (se presente e abilitata).
3. Al boot successivo, l'app prova a trasferire il coredump su SD (`/sdcard/coredump_*.elf`).
4. Se il trasferimento SD riesce, il coredump in partizione viene cancellato.
5. Nel file `error_*.log` viene scritto un marker che indica esplicitamente dove trovare lo stack completo.

Nota importante:
- `error_*.log` **non** contiene lo stack completo del panic.
- Lo stack completo è nel file ELF del coredump su SD.

## 3) Configurazione necessaria

## 3.1 Partizioni

La tabella partizioni deve includere una riga `coredump`, ad esempio:

```csv
coredump, data, coredump, 0xE50000, 0x2E000
```

File coinvolti:
- `partitions.csv`
- `partition_table/partitionTable.csv`

## 3.2 sdkconfig

Per coredump in flash (consigliato):
- `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`
- `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y`

## 4) Come generare un crash controllato

Opzioni disponibili:

1. **Compile-time** (`DEBUG_FORCE_CRASH_AT_BOOT`) in `main/main.c`.
2. **Runtime da UI** (Factory): bottone `Crash` in home (`/`), sezione Reboot.

Consiglio pratico:
- usare il bottone UI per evitare ricompilazioni durante test ripetuti.

## 5) Dove trovare i file di crash

Dopo un crash + reboot:
- file coredump: `/sdcard/coredump_YYYYMMDD_HHMMSS_MODE_vVERSION.elf`
- report testuale: `/sdcard/coredump_YYYYMMDD_HHMMSS_MODE_vVERSION.txt`
- marker applicativi: `/sdcard/error_*.log`

## 6) Come vedere lo stack chiamate completo

Dal root progetto, usare ESP-IDF toolchain attiva:

```bash
source ~/esp/esp-idf/export.sh
idf.py coredump-info -c /percorso/al/coredump_xxx.elf
```

Output atteso:
- registri CPU;
- task list;
- call stack/backtrace simbolizzato.

Per analisi interattiva (gdb):

```bash
source ~/esp/esp-idf/export.sh
idf.py coredump-debug -c /percorso/al/coredump_xxx.elf
```

Prerequisito:
- il firmware (`build/test_wave.elf`) deve essere coerente con il coredump acquisito.

## 7) Problemi noti e diagnosi

## 7.1 `No core dump partition found!`

Causa:
- partizione `coredump` mancante o tabella partizioni non flashata completamente.

Risoluzione:
- fare flash completo (inclusa partition table), non solo OTA app.

## 7.2 `Incorrect size of core dump image: ...`

Causa tipica:
- dati vecchi/sporchi in partizione `coredump` dopo cambio layout.

Stato attuale del codice:
- esiste un pre-check dell'header;
- se invalido, la partizione viene pulita automaticamente;
- il messaggio è gestito lato applicazione per evitare rumore e file inutili.

## 7.3 Nessun file coredump su SD

Possibili cause:
- SD non montata al boot successivo;
- spazio SD insufficiente;
- nessun coredump valido presente in partizione;
- firmware non coerente con configurazione coredump.

Verifiche rapide:
- log mount SD;
- spazio libero SD;
- presenza partizione `coredump`;
- `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`.

## 8) Boot guard e crash counter

Il sistema usa NVS (`boot_guard`) per tracciare reboot consecutivi.

Comportamento importante:
- reset “unclean” incrementano il contatore;
- crash intenzionale usa marker NVS dedicato per incremento affidabile anche se reset reason non è classificato come panic.

Chiavi principali:
- `consecutive`
- `crash_pending`
- `crash_reason`
- `force_crash`

## 9) Checklist operativa (rapida)

1. Verifica partizione `coredump` in `partitions.csv`.
2. Verifica `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`.
3. Flash completo firmware + partition table.
4. Genera crash (UI o flag compile-time).
5. Riavvia e controlla trasferimento su SD.
6. Apri `coredump_*.elf` con `idf.py coredump-info -c ...`.
7. Conferma incremento boot counter nel log `boot_guard`.

## 10) Comandi utili

Build:

```bash
source ~/esp/esp-idf/export.sh
idf.py build
```

Flash completo:

```bash
source ~/esp/esp-idf/export.sh
idf.py flash
```

Monitor:

```bash
source ~/esp/esp-idf/export.sh
idf.py monitor
```

Decode coredump:

```bash
source ~/esp/esp-idf/export.sh
idf.py coredump-info -c /sdcard/coredump_xxx.elf
```

## 11) Note finali

- `error_*.log` va usato come indice/marker, non come sorgente completa di stack.
- Il file autorevole per analisi post-mortem è `coredump_*.elf`.
- In caso di cambi tabella partizioni, è raccomandato almeno un flash completo per riallineare stato flash e metadata.

## 12) File da mantenere per decodificare un crash

Per poter decodificare un crash in modo affidabile (anche mesi dopo), conservare almeno:

### Minimo indispensabile
- `build/test_wave.elf` (simboli firmware)
- `sdkconfig`
- `partitions.csv` (o `partition_table/partitionTable.csv` equivalente)
- `coredump_*.elf`

### Fortemente consigliato
- `build/test_wave.bin`
- `build/test_wave.map`
- `build/partition_table/partition-table.bin`
- `build/project_description.json`
- `build/flasher_args.json`
- `build/bootloader/bootloader.elf`
- `build/bootloader/bootloader.bin`
- `build/ota_data_initial.bin`
- `build/storage.bin`
- `build/flash_args` (o `build/flash_project_args`)
- `build/flash_app_args`
- `sdkconfig.defaults`
- `app_version.h` e `main/app_version.h`
- `docs/COMPILE_FLAGS.md`

## 13) Bundle automatico in `./versions`

È disponibile lo script:
- `scripts/Version_bundle.sh`

Scopo:
- creare una cartella versionata in `./crash` con tutti i file utili alla post-analisi;
- includere anche i file necessari per riflashare lo stesso firmware (APP o flash completo);
- includere metadata (`versione`, `mode`, `branch`, `commit`, `describe`) in `manifest.txt`;
- opzionalmente copiare anche un `coredump_*.elf` specifico passato come argomento.

Esempi:

```bash
# crea bundle senza coredump esplicito
./scripts/Version_bundle.sh

# crea bundle includendo un coredump specifico
./scripts/Version_bundle.sh /percorso/coredump_YYYYMMDD_HHMMSS_MODE_vX.Y.Z.elf

# flash dal bundle (dentro la cartella creata in ./versions)
cd versions/vX.Y.Z_MODE_YYYYMMDD_HHMMSS
./flash_from_bundle.sh -p /dev/ttyACM0           # solo APP
./flash_from_bundle.sh -p /dev/ttyACM0 --all     # flash completo
```

Output:
- `./versions/v<version>_<MODE>_<timestamp>/...`

## 14) Strategia branch per versioni in produzione

Per mantenere tracciabilità completa dei dispositivi già installati:

1. creare un branch dedicato per ogni versione rilasciata in produzione (es. `release/v1.3.0`);
2. generare e committare il bundle in `./crash` su quel branch;
3. evitare modifiche retroattive al contenuto del bundle già pubblicato;
4. usare sempre il branch/versione corrispondente quando si decodifica un coredump storico.

In questo modo, per qualsiasi crash storico, si può risalire ai simboli e alla configurazione esatti della build installata.