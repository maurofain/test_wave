# Piano: Sintesi vocale e analisi Riconoscimento Vocale (ESP32P4)

Data: 2026-03-16
Autore: Copilot (bozza tecnica)

Sommario
--------
Piano per introdurre la riproduzione di messaggi vocali (TTS/annunci) su ESP32P4 e analisi delle opzioni per il riconoscimento vocale (ASR). Include architettura proposta, vincoli HW/SW, percorsi di implementazione alternativi, rischi e una lista di task eseguibili.

1) Obiettivi
-----------
- Permettere la riproduzione di messaggi vocali in punti specifici del flusso (es. fine init, avvio programma, errore, conferma azioni).
- Minimizzare impatto su UI: playback non-bloquante, integrato via FSM (event queue).
- Offrire percorso per messaggi statici (pre-registrati) e dinamici (TTS on-demand).
- Analizzare opzioni per il riconoscimento vocale e proporre un piano di prototipazione (local vs cloud).

2) Vincoli e presupposti
------------------------
- Dispositivo: ESP32P4 (risorse RAM/FLASH limitate rispetto a CPU host).
- Disponibilità di SPIFFS/SD per storage audio (log indica SPIFFS presente).
- Connessione di rete (Wi‑Fi) opzionale: utile per TTS/ASR cloud.
- Output audio richiesto per speaker esterno: scelta tra I2S DAC esterno o codec con amplificatore.
- Input microfono opzionale: I2S MEMS o ADC + preamplificatore.

3) Requisiti funzionali
-----------------------
- Riprodurre WAV PCM16 (8/16 kHz) direttamente da SPIFFS/SD.
- API runtime: voice_play(id | filename), voice_stop(), voice_set_volume(), voice_is_playing().
- Integrazione con FSM: ACTION_ID_VOICE_PLAY, ACTION_ID_VOICE_STOP, ACTION_ID_VOICE_SET_VOLUME.
- Priorità/Preemption: messaggi di alto livello possono interrompere/attenuare playback in corso.
- Logging e monitor (web UI / /test) per diagnostica.

4) Requisiti non funzionali
---------------------------
- Playback non-bloccante (eseguito in task dedicato).
- Uso minimo di heap durante streaming (DMA + buffer circolare).
- Robustezza: fallback se file mancante o catalogo TTS non raggiungibile.

5) Opzioni TTS (valutazione)
---------------------------
Opzione A — Pre-registrati (raccomandata come base)
- Come: file WAV PCM16 in SPIFFS/SD, riproduzione tramite I2S.
- Pro: semplice, affidabile, bassa CPU, qualità ottima se registrati bene.
- Contro: non adatto per messaggi dinamici.

Opzione B — On-device TTS (Flite / eSpeak / picoTTS)
- Come: cross‑compile engine TTS leggero e sintetizzare PCM al volo.
- Pro: funziona offline, immediatezza.
- Contro: dimensione binario e RAM significative; qualità robotica; integrazione complessa.
- Nota: valutare Flite (più "leggero") prima di eSpeak.

Opzione C — Cloud TTS (raccomandato per messaggi dinamici)
- Come: invio testo a server TTS (locale o cloud), ricezione MP3/WAV, caching su SPIFFS, riproduzione.
- Pro: voce naturale, multi-lingua, gestione centralizzata.
- Contro: richiede connettività, latenza di rete, considerazioni privacy.

Consiglio generale: partire con Opzione A (pre-registrati) per coprire tutti gli annunci critici; aggiungere Opzione C per testo dinamico (server interno o servizio cloud) con caching.

6) Hardware consigliato
-----------------------
- Output: I2S DAC + amplificatore o I2S DAC/amp integrato (es. MAX98357A, PCM5102A, ES8388, WM8960). MAX98357A è economico e comune.
- Speaker: 0.5-3W con amp adeguato.
- Input (se riconoscimento): MEMS I2S mic (INMP441, SPH0645, ICS43434) o array con preamp.
- Alimentazione & EMI: prevedere decoupling e filtro per evitare rumore durante playback.

6b) Board-specific: Waveshare ESP32-P4 WIFI6-DEVKIT
--------------------------------------------------
- On-board codec: ES8311 (config via I2C) connected to I2S and to the 3.5mm audio jack. The BSP exposes audio capability macros: BSP_CAPS_AUDIO, BSP_CAPS_AUDIO_SPEAKER, BSP_CAPS_AUDIO_MIC.
- I2S / audio pins (from components/waveshare__esp32_p4_nano/include/bsp/esp32_p4_nano.h):
  - BSP_I2S_SCLK = GPIO12
  - BSP_I2S_MCLK = GPIO13 (MCLK required by ES8311)
  - BSP_I2S_LCLK = GPIO10 (LRCK)
  - BSP_I2S_DOUT = GPIO9 (Data to codec)
  - BSP_I2S_DSIN = GPIO11 (Data from codec)
  - BSP_POWER_AMP_IO = GPIO53 (amplifier enable)
- BSP audio APIs (recommended):
  - bsp_audio_init(const i2s_std_config_t *i2s_config): initialize I2S. Pass NULL to use default (mono, duplex, 16-bit, 22050 Hz).
  - bsp_audio_codec_speaker_init(), bsp_audio_codec_microphone_init(): return esp_codec_dev_handle_t for playback/record.
  - Use esp_codec_dev_* helpers to control and stream audio: esp_codec_dev_set_out_vol(), esp_codec_dev_open(), esp_codec_dev_write(), esp_codec_dev_close().
- Example playback flow (pseudocode):

  bsp_i2c_init(); // codec configuration over I2C (BSP may already call this)
  bsp_audio_init(NULL); // or pass custom i2s_std_config_t for 16kHz
  spk = bsp_audio_codec_speaker_init();
  esp_codec_dev_set_out_vol(spk, volume);
  esp_codec_dev_open(spk, &fs); // fs = audio params (sample rate, bits, channels)
  // stream WAV PCM16 bytes read from SPIFFS
  esp_codec_dev_write(spk, buf, buf_len);
  esp_codec_dev_close(spk);

- Notes and recommendations:
  - The BSP default sample rate is 22050 Hz. For ASR/TTS pipelines 16000 Hz is commonly used; pass a custom i2s_config and open esp_codec_dev with matching fs if required.
  - The ES8311 codec in BSP is configured with use_mclk = true; MCLK pin is defined and handled by the BSP.
  - The power amplifier control pin (BSP_POWER_AMP_IO) is available; BSP codec init typically controls it but verify on hardware and in code.
  - Prefer using the BSP audio stack (esp_codec_dev) for playback/recording to avoid low-level I2S handling.
  - For initial implementation use SPIFFS WAV files and esp_codec_dev_write; add MP3/OGG decoding later if storage is constrained.

- Suggested immediate action:
  - Implement components/voice to call bsp_audio_init() and bsp_audio_codec_speaker_init(), then stream a test WAV from /spiffs/voice/ to verify playback through the 3.5mm jack and amplifier control.
  - If quality or power is insufficient, consider an external I2S DAC/amp.

7) Software architecture proposta
--------------------------------
- Nuovo componente: components/voice/
  - voice_player.c/.h: gestione task di playback, API pubbliche.
  - Decoders: wav_reader (obbligatorio), modular loader per MP3 (minimp3) o OGG se richiesto.
  - i2s_audio_out.c: wrapper su driver I2S (config, DMA, volume).
  - voice_catalog.json: mapping message_id -> filename + metadata (priority, volume, loop).
- FSM integration
  - Nuove action: ACTION_ID_VOICE_PLAY, ACTION_ID_VOICE_PLAY_TEXT (per TTS cloud), ACTION_ID_VOICE_STOP, ACTION_ID_VOICE_SET_VOLUME.
  - Uso fsm_event_publish(...) per chiedere riproduzione — evitiamo chiamate dirette da LVGL/HTTP.
- Web UI / LVGL
  - Pagine di test: riproduci messaggi, upload audio, registro riproduzioni.
  - Configurazione: abilitare/disabilitare voice, volume globale.
- Cloud TTS
  - Implementare client HTTP con endpoint di sintesi; caching di risultati su SPIFFS con hash(text,voice,lang).

8) Formati e parametri
----------------------
- Formato consigliato (semplice): PCM16 LE WAV, 16 kHz (voce chiara, occupa ~32 KB/s). Usare 16 kHz/16-bit o 8 kHz per messaggi brevissimi.
- Compressione opzionale: MP3/OGG per risparmiare spazio (richiede decoder: libmp3/minimp3 o esp-adf).

9) Flusso eventi e priorità
---------------------------
- Event published: {to: AGN_VOICE, action: ACTION_ID_VOICE_PLAY, value_u32: message_id, data_ptr: filename}
- Voice task: queue prioritizzata, se arriva messaggio con priorità maggiore: attenua/interrompe playback corrente (policy configurabile).

10) Testing e criteri di accettazione
-------------------------------------
- Test HW: playback di WAV di test senza click/pop.
- Test integrazione: chiamata da FSM (web UI/LVGL) genera evento, voice task riproduce audio.
- Latency: tempo tra publish evento e audio start < 300 ms per file cached nel device (meno per file da SPIFFS locale).
- Robustezza: ripetuti playback senza leak di memoria.

11) Analisi Riconoscimento Vocale (ASR)
--------------------------------------
Obiettivi possibili:
- Comandi limitati ("start","stop","sospendi", etc.)
- ASR generico per trascrizione (più esigente)

Opzioni ASR
A) Cloud ASR (raccomandato per accuratezza)
  - Flusso: VAD -> invio chunk audio -> server restituisce testo.
  - Pro: alta accuratezza, multi-lingua, modelli aggiornabili.
  - Contro: richiede rete; latenza variabile; privacy.

B) Ibrido: KWS locale + cloud ASR
  - Esempio: modello KWS (wake-word) su device, dopo wake prelevare frase e inviare a cloud.
  - Pro: riduce consumo rete e latenza su wake-only; migliora privacy per non-continui streaming.

C) On-device ASR (solo per vocabolario ristretto)
  - Tecnologie: PocketSphinx, TensorFlow Lite (keyword spotting/grammars), Vosk (più pesante).
  - Pro: funziona offline.
  - Contro: modelli pesanti, lavoro di tuning, riconoscimento limitato.

Hardware microfono e acquisizione
- Mic I2S + buffer circolare, WebRTC VAD o webrtcvad port per riconoscere attività vocale.
- Campionamento: 16 kHz mono PCM sufficiente per lingua.

Raccomandazione iniziale per ASR
- Prototipazione rapida: KWS locale (tensorflow-lite-micro KWS o Porcupine) + streaming a server ASR per la trascrizione completa. Questo bilancia risorse, privacy e accuratezza.

12) Piano di implementazione (milestones)
-----------------------------------------
1. Valutazione (T0): scegliere strategia TTS (pre-registrati + cloud fallback raccomandato). (TODO: voice-eval-tts)
2. HW prototipo (T1): collegare I2S DAC (MAX98357A) e test playback di un WAV da SPIFFS. (TODO: voice-hardware-select)
3. Voice player (T2): implementare components/voice con WAV streamer su I2S e API. (TODO: voice-audio-driver)
4. FSM integration (T3): definire ACTION_ID_*, aggiornare LVGL/HTTP per publish eventi. (TODO: voice-fsm-integration)
5. WebUI & test pages (T4): test endpoints /test per riproduzione, upload e gestione messaggi. (TODO: voice-webui-lvgl)
6. Cloud TTS (T5, opzionale): implementare client synthesis + cache. (TODO: voice-cloud-tts)
7. ASR prototype (T6): implementare KWS locale + streaming to cloud ASR. (TODOs: speech-recognition-eval, speech-recognition-prototype)
8. QA & deployment (T7): test integrate, stress, documentazione. (TODO: voice-tests, voice-docs)

13) Rischi e mitigazioni
------------------------
- Uso memoria e flash: mitigare usando WAV non compressi per decodifica semplice; usare mp3 solo se necessario con decoder esterno.
- Interferenze audio/UI: eseguire playback in task con priorità controllata, usare DMA/I2S per evitare jitter CPU.
- Privacy (ASR cloud): offrire opt-out e log non inviati a terzi; prevedere server locale se necessario.

14) Esempio mappatura messaggi
-----------------------------
voice_catalog.json (esempio):
{
  "1": {"file":"/spiffs/voice/init_done.wav", "priority": 10, "vol": 80},
  "2": {"file":"/spiffs/voice/program_start.wav", "priority": 8, "vol": 80}
}

15) Prossimi passi immediati proposti
-----------------------------------
- Conferma strategia (pre-registrati + cloud TTS fallback) o preferisci tentare TTS on-device?
- Se OK: procedere con TODO "voice-hardware-select" e "voice-audio-driver" (collegamento HW + play WAV).

---
Fine del piano (bozza). Modifiche su richiesta.
