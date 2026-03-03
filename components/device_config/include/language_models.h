#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * FILE AUTO-GENERATO: non modificare manualmente.
 * Sorgente: scripts/generate_language_models.py
 * Map di riferimento: data/i18n_en.map.json
 */

#define I18N_LANG_COUNT 5
#define I18N_LANG_IT 0
#define I18N_LANG_EN 1
#define I18N_LANG_DE 2
#define I18N_LANG_FR 3
#define I18N_LANG_ES 4

static const char *const I18N_LANG_CODES[I18N_LANG_COUNT] = {"it", "en", "de", "fr", "es"};

/* Scope IDs */
#define I18N_SCOPE_HEADER 1
#define I18N_SCOPE_LVGL 2
#define I18N_SCOPE_NAV 3
#define I18N_SCOPE_P_CONFIG 4
#define I18N_SCOPE_P_EMULATOR 5
#define I18N_SCOPE_P_LOGS 6
#define I18N_SCOPE_P_PROGRAMS 7
#define I18N_SCOPE_P_RUNTIME 8
#define I18N_SCOPE_P_TEST 9

/* Key IDs */
#define I18N_KEY_TIME_NOT_SET 1
#define I18N_KEY_TIME_NOT_AVAILABLE 2
#define I18N_KEY_CONFIG 3
#define I18N_KEY_EMULATOR 4
#define I18N_KEY_HOME 5
#define I18N_KEY_LOGS 6
#define I18N_KEY_OTA 7
#define I18N_KEY_STATS 8
#define I18N_KEY_TASKS 9
#define I18N_KEY_TEST 10
#define I18N_KEY_ABILITATO 11
#define I18N_KEY_AGGIORNA 12
#define I18N_KEY_AGGIORNA_DATI 13
#define I18N_KEY_APRI_EDITOR_PROGRAMMI 14
#define I18N_KEY_ATTENDERE_IL_RIAVVIO 15
#define I18N_KEY_BACKUP_CONFIG 16
#define I18N_KEY_BASE_SERVER_URL 17
#define I18N_KEY_BAUDRATE 18
#define I18N_KEY_BUFFER_RX 19
#define I18N_KEY_BUFFER_TX 20
#define I18N_KEY_CARICA_FIRMWARE 21
#define I18N_KEY_CONFERMA_NUOVA_PASSWORD 22
#define I18N_KEY_DATA_BITS 23
#define I18N_KEY_DHCP 24
#define I18N_KEY_DISPLAY 25
#define I18N_KEY_DISPLAY_ABILITATO 26
#define I18N_KEY_DUAL_PID_OPZIONALE 27
#define I18N_KEY_ETHERNET 28
#define I18N_KEY_GATEWAY 29
#define I18N_KEY_GESTIONE_PREZZI_DURATA_E_RELAY_PER_PROGRAMMA 30
#define I18N_KEY_GPIO_33 31
#define I18N_KEY_GPIO_AUSILIARIO_GPIO33 32
#define I18N_KEY_I_O_EXPANDER 33
#define I18N_KEY_IDENTITA_DISPOSITIVO 34
#define I18N_KEY_INDIRIZZO_IP 35
#define I18N_KEY_INPUT_FLOAT 36
#define I18N_KEY_INPUT_PULL_DOWN 37
#define I18N_KEY_INPUT_PULL_UP 38
#define I18N_KEY_ITALIANO_IT 39
#define I18N_KEY_LED_STRIP_WS2812 40
#define I18N_KEY_LINGUA_UI 41
#define I18N_KEY_LOGGING_REMOTO 42
#define I18N_KEY_LUMINOSITA_LCD 43
#define I18N_KEY_MDB_CONFIGURATION 44
#define I18N_KEY_MDB_ENGINE 45
#define I18N_KEY_MODALITA 46
#define I18N_KEY_MODIFICA_PASSWORD_RICHIESTA_PER_EMULATOR_E_REBOOT_FACTORY 47
#define I18N_KEY_NOME_DISPOSITIVO 48
#define I18N_KEY_NTP 49
#define I18N_KEY_NTP_ABILITATO 50
#define I18N_KEY_NUMERO_DI_LED 51
#define I18N_KEY_NUOVA_PASSWORD 52
#define I18N_KEY_OFFSET_FUSO_ORARIO_ORE 53
#define I18N_KEY_OUTPUT 54
#define I18N_KEY_PARITA_0_NONE_1_ODD_2_EVEN 55
#define I18N_KEY_PASSWORD 56
#define I18N_KEY_PASSWORD_ATTUALE 57
#define I18N_KEY_PASSWORD_BOOT_EMULATORE 58
#define I18N_KEY_PERIFERICHE_HARDWARE 59
#define I18N_KEY_PID 60
#define I18N_KEY_PORTA_UDP 61
#define I18N_KEY_PWM_CANALE_1 62
#define I18N_KEY_PWM_CANALE_2 63
#define I18N_KEY_REBOOT_IN_APP_LAST 64
#define I18N_KEY_REBOOT_IN_FACTORY_MODE 65
#define I18N_KEY_REBOOT_IN_OTA0 66
#define I18N_KEY_REBOOT_IN_OTA1 67
#define I18N_KEY_REBOOT_IN_PRODUCTION_MODE 68
#define I18N_KEY_RESET_FABBRICA 69
#define I18N_KEY_RS232_CONFIGURATION 70
#define I18N_KEY_RS485_CONFIGURATION 71
#define I18N_KEY_SALVA_CONFIGURAZIONE 72
#define I18N_KEY_SALVA_PASSWORD_BOOT 73
#define I18N_KEY_SCANNER_USB_CDC 74
#define I18N_KEY_SCHEDA_SD 75
#define I18N_KEY_SENSORE_TEMPERATURA 76
#define I18N_KEY_SERVER_ABILITATO 77
#define I18N_KEY_SERVER_NTP_1 78
#define I18N_KEY_SERVER_NTP_2 79
#define I18N_KEY_SERVER_PASSWORD_MD5 80
#define I18N_KEY_SERVER_REMOTO 81
#define I18N_KEY_SERVER_SERIAL 82
#define I18N_KEY_SSID 83
#define I18N_KEY_STATO_INIZIALE_SOLO_OUT 84
#define I18N_KEY_STOP_BITS_1_2 85
#define I18N_KEY_SUBNET_MASK 86
#define I18N_KEY_TABELLA_PROGRAMMI 87
#define I18N_KEY_TAG_PORTE_SERIALI 88
#define I18N_KEY_UART_RS232 89
#define I18N_KEY_UART_RS485 90
#define I18N_KEY_USA_BROADCAST_UDP 91
#define I18N_KEY_VID 92
#define I18N_KEY_WIFI_ABILITATO 93
#define I18N_KEY_WIFI_STA 94
#define I18N_KEY_ANNULLA 95
#define I18N_KEY_CONTINUA 96
#define I18N_KEY_CREDITO 97
#define I18N_KEY_IN_PAUSA_00_00 98
#define I18N_KEY_INSERIRE_LA_PASSWORD_PER_CONTINUARE 99
#define I18N_KEY_LAYOUT_OPERATIVO_PANNELLO_UTENTE_800X1280_A_SINISTRA_QUADRO_ELET 100
#define I18N_KEY_NESSUN_EVENTO_IN_CODA 101
#define I18N_KEY_PASSWORD_RICHIESTA 102
#define I18N_KEY_PROGRAMMA_1 103
#define I18N_KEY_PROGRAMMA_2 104
#define I18N_KEY_PROGRAMMA_3 105
#define I18N_KEY_PROGRAMMA_4 106
#define I18N_KEY_PROGRAMMA_5 107
#define I18N_KEY_PROGRAMMA_6 108
#define I18N_KEY_PROGRAMMA_7 109
#define I18N_KEY_PROGRAMMA_8 110
#define I18N_KEY_QUADRO_ELETTRICO 111
#define I18N_KEY_R1 112
#define I18N_KEY_R10 113
#define I18N_KEY_R2 114
#define I18N_KEY_R3 115
#define I18N_KEY_R4 116
#define I18N_KEY_R5 117
#define I18N_KEY_R6 118
#define I18N_KEY_R7 119
#define I18N_KEY_R8 120
#define I18N_KEY_R9 121
#define I18N_KEY_RELAY 122
#define I18N_KEY_RICARICA_COIN 123
#define I18N_KEY_STATO_CREDITO 124
#define I18N_KEY_TEMPO_00_00 125
#define I18N_KEY_APPLICA_FILTRO 126
#define I18N_KEY_CONFIGURA_IL_LOGGING_REMOTO_NELLA_PAGINA 127
#define I18N_KEY_CONFIGURAZIONE 128
#define I18N_KEY_DERE_I_NUOVI_LOG_APPLICA_LIVELLI 129
#define I18N_KEY_HTML 130
#define I18N_KEY_I_LOG_VENGONO_RICEVUTI_VIA_UDP_DAL_SERVER_CONFIGURATO_AGGIORNA_L 131
#define I18N_KEY_IN_ATTESA_DI_LOG 132
#define I18N_KEY_ITEM_TAG 133
#define I18N_KEY_L_T 134
#define I18N_KEY_LOG_REMOTO_RICEVUTI 135
#define I18N_KEY_NESSUN_TAG_DISPONIBILE 136
#define I18N_KEY_PER_INIZIARE_A_RICEVERE_LOG 137
#define I18N_KEY_PULISCI_FILTRO 138
#define I18N_KEY_400_BAD_REQUEST 139
#define I18N_KEY_403_FORBIDDEN 140
#define I18N_KEY_404_NON_TROVATO 141
#define I18N_KEY_405_METHOD_NOT_ALLOWED 142
#define I18N_KEY_BUTTON_CLASS_BTN_SECONDARY_ONCLICK_LOADPROGRAMS_RICARICA_BUTTON 143
#define I18N_KEY_BUTTON_ONCLICK_SAVEPROGRAMS_SALVA_TABELLA_BUTTON 144
#define I18N_KEY_C_GET_CONFIG_PROGRAMS 145
#define I18N_KEY_CAMPI_CURRENT_PASSWORD_NEW_PASSWORD_OBBLIGATORI 146
#define I18N_KEY_CAMPI_RELAY_NUMBER_STATUS_DURATION_OBBLIGATORI 147
#define I18N_KEY_DIV 148
#define I18N_KEY_DIV_CLASS_CONTAINER_DIV_CLASS_SECTION 149
#define I18N_KEY_DIV_DIV 150
#define I18N_KEY_DIV_ID_STATUS_CLASS_STATUS_DIV 151
#define I18N_KEY_DURATA_S 152
#define I18N_KEY_EDITOR_TABELLA_PROGRAMMI_FACTORY 153
#define I18N_KEY_ERRORE_LETTURA_PAYLOAD 154
#define I18N_KEY_ERRORE_VALIDAZIONE 155
#define I18N_KEY_H2_EDITOR_TABELLA_PROGRAMMI_FACTORY_H2 156
#define I18N_KEY_ID 157
#define I18N_KEY_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI_PR 158
#define I18N_KEY_JSON_NON_VALIDO 159
#define I18N_KEY_METHOD_NOT_ALLOWED 160
#define I18N_KEY_NOME 161
#define I18N_KEY_NUOVA_PASSWORD_NON_VALIDA_O_ERRORE_SALVATAGGIO 162
#define I18N_KEY_P_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI 163
#define I18N_KEY_PASSWORD_ATTUALE_NON_VALIDA 164
#define I18N_KEY_PAUSA_MAX_S 165
#define I18N_KEY_PAUSE_ALL_SECONDS 166
#define I18N_KEY_SET_PAUSE_ALL 167
#define I18N_KEY_PAYLOAD_NON_VALIDO 168
#define I18N_KEY_PREZZO 169
#define I18N_KEY_RELAY_MASK 170
#define I18N_KEY_RELAY_NUMBER_FUORI_RANGE 171
#define I18N_KEY_RETURN 172
#define I18N_KEY_RETURN_TR 173
#define I18N_KEY_RICARICA 174
#define I18N_KEY_SALVA_TABELLA 175
#define I18N_KEY_SCRIPT_BODY_HTML 176
#define I18N_KEY_TABLE_THEAD_TR_TH_ID_TH_TH_NOME_TH_TH_ABILITATO_TH_TH_PREZZO_TH 177
#define I18N_KEY_WEB_UI_PROGRAMS_PAGE 178
#define I18N_KEY_ACTIVITY 179
#define I18N_KEY_AGGIORNA_PAGINA 180
#define I18N_KEY_AGGIUNGI_TASK 181
#define I18N_KEY_AMBIENTE 182
#define I18N_KEY_API 183
#define I18N_KEY_API_ENDPOINTS 184
#define I18N_KEY_APP_LAST 185
#define I18N_KEY_APPLICA 186
#define I18N_KEY_APPLY_TASKS 187
#define I18N_KEY_AUTHENTICATE_REMOTE 188
#define I18N_KEY_BACKUP_CONFIGURATION 189
#define I18N_KEY_BENVENUTI_NELL_INTERFACCIA_DI_CONFIGURAZIONE_E_TEST 190
#define I18N_KEY_CARICAMENTO 191
#define I18N_KEY_CONFIGURAZIONE_TASK 192
#define I18N_KEY_COPIA 193
#define I18N_KEY_CORE 194
#define I18N_KEY_CREDITO_ACCUMULATO 195
#define I18N_KEY_CURRENT_CONFIGURATION 196
#define I18N_KEY_DESCRIPTION 197
#define I18N_KEY_DEVICE_STATUS_JSON 198
#define I18N_KEY_EDITOR_CSV 199
#define I18N_KEY_EMULATORE 200
#define I18N_KEY_FACTORY 201
#define I18N_KEY_FACTORY_RESET 202
#define I18N_KEY_FETCH_FIRMWARE 203
#define I18N_KEY_FETCH_IMAGES 204
#define I18N_KEY_FETCH_TRANSLATIONS 205
#define I18N_KEY_FIRMWARE 206
#define I18N_KEY_FORCE_NTP_SYNC 207
#define I18N_KEY_GET 208
#define I18N_KEY_GET_CUSTOMERS 209
#define I18N_KEY_GET_OPERATORS 210
#define I18N_KEY_GET_REMOTE_CONFIG 211
#define I18N_KEY_GETTONIERA 212
#define I18N_KEY_HEADER 213
#define I18N_KEY_HTTP_SERVICES 214
#define I18N_KEY_INDIRIZZO_IP_ETHERNET 215
#define I18N_KEY_INDIRIZZO_IP_WIFI_AP 216
#define I18N_KEY_INDIRIZZO_IP_WIFI_STA 217
#define I18N_KEY_INFORMAZIONI 218
#define I18N_KEY_INVIA 219
#define I18N_KEY_IS_JWT 220
#define I18N_KEY_KEEPALIVE 221
#define I18N_KEY_LED_WS2812 222
#define I18N_KEY_LOG_LEVELS 223
#define I18N_KEY_LOGIN_CHIAMATE_HTTP 224
#define I18N_KEY_MDB_STATUS 225
#define I18N_KEY_METHOD 226
#define I18N_KEY_OFFLINE_PAYMENT 227
#define I18N_KEY_OTA0 228
#define I18N_KEY_OTA1 229
#define I18N_KEY_PARTIZIONE_AL_BOOT 230
#define I18N_KEY_PARTIZIONE_CORRENTE 231
#define I18N_KEY_PASSWORD_MD5 232
#define I18N_KEY_PAYLOAD 233
#define I18N_KEY_PAYMENT_REQUEST 234
#define I18N_KEY_PERIODO_MS 235
#define I18N_KEY_POST 236
#define I18N_KEY_POST_API_ACTIVITY 237
#define I18N_KEY_POST_API_GETCONFIG 238
#define I18N_KEY_POST_API_GETCUSTOMERS 239
#define I18N_KEY_POST_API_GETFIRMWARE 240
#define I18N_KEY_POST_API_GETIMAGES 241
#define I18N_KEY_POST_API_GETOPERATORS 242
#define I18N_KEY_POST_API_GETTRANSLATIONS 243
#define I18N_KEY_POST_API_KEEPALIVE 244
#define I18N_KEY_POST_API_PAYMENT 245
#define I18N_KEY_POST_API_PAYMENTOFFLINE 246
#define I18N_KEY_POST_API_SERVICEUSED 247
#define I18N_KEY_PRIORITA 248
#define I18N_KEY_PROFILO 249
#define I18N_KEY_PWM_CHANNEL_1_2 250
#define I18N_KEY_REBOOT 251
#define I18N_KEY_RESTART_USB_HOST 252
#define I18N_KEY_RETE 253
#define I18N_KEY_RICHIESTA 254
#define I18N_KEY_RIMUOVI 255
#define I18N_KEY_RISPOSTA 256
#define I18N_KEY_RUN_INTERNAL_TESTS 257
#define I18N_KEY_SALVA 258
#define I18N_KEY_SAVE_CONFIGURATION 259
#define I18N_KEY_SAVE_TASKS 260
#define I18N_KEY_SD_CARD 261
#define I18N_KEY_SERIAL 262
#define I18N_KEY_SERVICE_USED 263
#define I18N_KEY_SET_LOG_LEVEL 264
#define I18N_KEY_SPAZIO_TOTALE 265
#define I18N_KEY_SPAZIO_USATO 266
#define I18N_KEY_STACK_WORDS 267
#define I18N_KEY_STATISTICHE 268
#define I18N_KEY_STATO 269
#define I18N_KEY_STATO_DRIVER 270
#define I18N_KEY_STATO_LOGICO_SM 271
#define I18N_KEY_STORED_LOGS 272
#define I18N_KEY_TASKS_CSV 273
#define I18N_KEY_TEMPERATURA 274
#define I18N_KEY_TEST_HARDWARE 275
#define I18N_KEY_TOKEN 276
#define I18N_KEY_UMIDITA 277
#define I18N_KEY_UPDATE_OTA 278
#define I18N_KEY_URI 279
#define I18N_KEY_USB_ENUMERATE 280
#define I18N_KEY_4800_BAUD_PIN_21_RX_20_TX 281
#define I18N_KEY_AVVIA_AGGIORNAMENTI_PERIODICI_PER_LA_SEZIONE_SCANNER 282
#define I18N_KEY_BACKUP 283
#define I18N_KEY_BACKUP_JSON_CONFIG 284
#define I18N_KEY_BLINK_TUTTE_LE_USCITE_1HZ 285
#define I18N_KEY_BUTTON_ID_BTN_LED_MANUAL_ONCLICK_TOGGLELEDMANUAL_STYLE_BACKGROUN 286
#define I18N_KEY_BUTTON_ONCLICK_REFRESHSDSTATUS_STYLE_BACKGROUND_3498DB_AGGIORNA 287
#define I18N_KEY_BUTTON_ONCLICK_RUNTEST_SD_INIT_STYLE_BACKGROUND_27AE60_INIT_BUTT 288
#define I18N_KEY_BUTTON_ONCLICK_RUNTEST_SHT_READ_STYLE_BACKGROUND_F39C12_FORZA_TE 289
#define I18N_KEY_BUTTON_ONCLICK_SETPWM_STYLE_BACKGROUND_2980B9_APPLICA_BUTTON 290
#define I18N_KEY_BUTTON_ONCLICK_TESTEEPROM_READ_JSON_STYLE_BACKGROUND_9B59B6_LEGG 291
#define I18N_KEY_BUTTON_ONCLICK_TESTEEPROM_READ_STYLE_BACKGROUND_3498DB_LEGGI_BUT 292
#define I18N_KEY_BUTTON_ONCLICK_TESTEEPROM_WRITE_SCRIVI_BUTTON 293
#define I18N_KEY_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_OFF_CLASS_B 294
#define I18N_KEY_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_ON_STYLE_BA 295
#define I18N_KEY_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_SETUP_STYLE 296
#define I18N_KEY_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_STATE_STYLE 297
#define I18N_KEY_C 298
#define I18N_KEY_C_GET_TEST 299
#define I18N_KEY_CANALE 300
#define I18N_KEY_CCTALK 301
#define I18N_KEY_CLEAR 302
#define I18N_KEY_COLLAPSIBLE_SECTIONS_SUPPORT 303
#define I18N_KEY_CONFIG_JSON 304
#define I18N_KEY_CONTROLLO_MANUALE 305
#define I18N_KEY_CONTROLLO_MANUALE_PWM 306
#define I18N_KEY_COPIATO 307
#define I18N_KEY_DATO_BYTE_0_255 308
#define I18N_KEY_DISPLAY_BRIGHTNESS_SLIDER_HANDLER_DEBOUNCED_PERSIST_AFTER_IDLE 309
#define I18N_KEY_DIV_CLASS_CONTAINER 310
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS 311
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_CCTALK_ONCLICK_TOGGLESIMPL 312
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_IOEXP_ONCLICK_TOGGLESIMPLE 313
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_LED_RAINBOW_ONCLICK_TOGGLE 314
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_MDB_ONCLICK_TOGGLESIMPLETE 315
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_PWM1_ONCLICK_TOGGLESIMPLET 316
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_PWM2_ONCLICK_TOGGLESIMPLET 317
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_RS232_ONCLICK_TOGGLESIMPLE 318
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_RS485_ONCLICK_TOGGLESIMPLE 319
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_IF_CONFIRM_CANCELLARE_TUT 320
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_RUNCONFIGBACKUP_STYLE_BAC 321
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_RUNTEST_SD_LIST_ELENCA_BU 322
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_NUMBER_ID_EEPROM_ADDR_VALUE_0 323
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_NUMBER_ID_EEPROM_VAL_VALUE_12 324
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_CCTALK_INPUT_PLACEHOL 325
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_MDB_INPUT_PLACEHOLDER 326
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_RS232_INPUT_PLACEHOLD 327
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_RS485_INPUT_PLACEHOLD 328
#define I18N_KEY_DIV_CLASS_TEST_ITEM 329
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_BACKUP_JSON_CONFIG_SPAN 330
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_BLINK_TUTTE_LE_USCITE 331
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PATTERN_RAINBOW_SPAN 332
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM1_DUTY_CYCLE_SWEEP 333
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM2_DUTY_CYCLE_SWEEP 334
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_STATO_MONTAGGIO_SPAN 335
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_0X55_0XA 336
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_ECHO_SPA 337
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_INVIA_PA 338
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_ULTIME_LETTURE_SPAN 339
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_ULTIMO_ERRORE_SPAN 340
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_DATO_BYTE_0_255_SPAN 341
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_ELENCO_FILE_ROOT_SPAN 342
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_INDIRIZZO_0_2047_SPAN 343
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_INVIA_PACCHETTO_ES_0X01_0X02_SPAN 344
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_ES_0X55_0XAA_R_SPAN 345
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_HEX_ES_08_00_SPAN 346
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_SPAN 347
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_SELEZIONA_COLORE_SPAN 348
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_VALORI_CORRENTI_SPAN 349
#define I18N_KEY_DIV_ID_CCTALK_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV 350
#define I18N_KEY_DIV_ID_EEPROM_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_EEPROM_DIV 351
#define I18N_KEY_DIV_ID_GPIOS_STATUS_CLASS_STATUS_BOX_STATO_GPIO_IN_LETTURA_DIV_D 352
#define I18N_KEY_DIV_ID_GPIOS_TEST_GRID_CARICAMENTO_DIV 353
#define I18N_KEY_DIV_ID_IOEXP_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_I_O_EXPANDE 354
#define I18N_KEY_DIV_ID_LED_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_LED_DIV_DIV 355
#define I18N_KEY_DIV_ID_MDB_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_MDB_DIV_DIV 356
#define I18N_KEY_DIV_ID_PWM_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_PWM_DIV_DIV 357
#define I18N_KEY_DIV_ID_RS232_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV 358
#define I18N_KEY_DIV_ID_RS485_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV 359
#define I18N_KEY_DIV_ID_SCANNER_STATUS_CLASS_STATUS_BOX_NESSUNA_LETTURA_ANCORA_DI 360
#define I18N_KEY_DIV_ID_SD_STATUS_CLASS_STATUS_BOX_STYLE_DISPLAY_NONE_DIV_DIV 361
#define I18N_KEY_DIV_ID_SECTION_GPIO33_CLASS_SECTION_COLLAPSED_H2_GPIO_AUSILIARI 362
#define I18N_KEY_DIV_ID_SECTION_SCANNER_CLASS_SECTION_COLLAPSED_H2_SCANNER_USB_BA 363
#define I18N_KEY_DIV_ID_SHT_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_SHT40_DIV_DIV 364
#define I18N_KEY_DUTY 365
#define I18N_KEY_EEPROM_24LC16 366
#define I18N_KEY_ELENCA 367
#define I18N_KEY_ELENCO_FILE_ROOT 368
#define I18N_KEY_ELSE_OUT 369
#define I18N_KEY_ERRORE 370
#define I18N_KEY_ERRORE_COMANDO_SCANNER 371
#define I18N_KEY_FORMATTAZIONE_FAT32 372
#define I18N_KEY_FORZA_TEST_LETTURA 373
#define I18N_KEY_FREQ_HZ 374
#define I18N_KEY_GPIO_AUSILIARI_GPIO33 375
#define I18N_KEY_H 376
#define I18N_KEY_H3_CONTROLLO_MANUALE_H3 377
#define I18N_KEY_H3_CONTROLLO_MANUALE_PWM_H3 378
#define I18N_KEY_HEX 379
#define I18N_KEY_HEX_2 380
#define I18N_KEY_HIDE_TEST_SECTIONS_WHEN_CORRESPONDING_PERIPHERAL_IS_DISABLED_IN 381
#define I18N_KEY_I2C_0X45 382
#define I18N_KEY_IF_ISOUT_H 383
#define I18N_KEY_INDIRIZZO_0_2047 384
#define I18N_KEY_INGRESSI_CHIP_0X44 385
#define I18N_KEY_INIT 386
#define I18N_KEY_INIZIA_BLINK 387
#define I18N_KEY_INIZIA_MANUALE 388
#define I18N_KEY_INIZIA_RAINBOW 389
#define I18N_KEY_INIZIA_SWEEP 390
#define I18N_KEY_INIZIA_TEST 391
#define I18N_KEY_INVIA_PACCHETTO_ES_0X01_0X02 392
#define I18N_KEY_INVIA_STRINGA 393
#define I18N_KEY_INVIA_STRINGA_ES_0X55_0XAA 394
#define I18N_KEY_INVIA_STRINGA_HEX_ES_08_00 395
#define I18N_KEY_INVIO_BACKUP 396
#define I18N_KEY_LEGGI 397
#define I18N_KEY_LEGGI_JSON 398
#define I18N_KEY_LETTURA_OK 399
#define I18N_KEY_LIVE_UPDATE_TO_HARDWARE_SHORT_DEBOUNCE 400
#define I18N_KEY_LOG_OPERAZIONI_SD 401
#define I18N_KEY_LUMINOSITA 402
#define I18N_KEY_MDB_MULTI_DROP_BUS 403
#define I18N_KEY_MONITOR 404
#define I18N_KEY_NESSUNA_LETTURA_ANCORA 405
#define I18N_KEY_OFF 406
#define I18N_KEY_OK 407
#define I18N_KEY_ON 408
#define I18N_KEY_OUT1_GPIO47 409
#define I18N_KEY_OUT2_GPIO48 410
#define I18N_KEY_PATTERN_RAINBOW 411
#define I18N_KEY_PIN_12_A_17_B 412
#define I18N_KEY_PIN_15_GPIO_5 413
#define I18N_KEY_PIN_23_TX_35_RX 414
#define I18N_KEY_PIN_32_37_I2C 415
#define I18N_KEY_PIN_34_38 416
#define I18N_KEY_PIN_39 417
#define I18N_KEY_PIN_8_11 418
#define I18N_KEY_POI_PULISCE_LATO_SERVER 419
#define I18N_KEY_PRONTO_PER_TEST_EEPROM 420
#define I18N_KEY_PRONTO_PER_TEST_I_O_EXPANDER 421
#define I18N_KEY_PRONTO_PER_TEST_LED 422
#define I18N_KEY_PRONTO_PER_TEST_MDB 423
#define I18N_KEY_PRONTO_PER_TEST_PWM 424
#define I18N_KEY_PRONTO_PER_TEST_SHT40 425
#define I18N_KEY_PULISCE_IMMEDIATAMENTE_L_AREA_DI_TESTO_LATO_CLIENT 426
#define I18N_KEY_PULISCI_FORMATTA 427
#define I18N_KEY_PWM 428
#define I18N_KEY_PWM1_DUTY_CYCLE_SWEEP 429
#define I18N_KEY_PWM2_DUTY_CYCLE_SWEEP 430
#define I18N_KEY_SCANNER_OFF 431
#define I18N_KEY_SCANNER_ON 432
#define I18N_KEY_SCANNER_SETUP 433
#define I18N_KEY_SCANNER_STATE 434
#define I18N_KEY_SCANNER_UI_MOSTRA_LE_ULTIME_LETTURE_TAG_SCANNER_E_COMANDI_ON_OFF 435
#define I18N_KEY_SCANNER_USB_BARCODE_QR 436
#define I18N_KEY_SCHEDA_MICROSD 437
#define I18N_KEY_SCHEDULE_PERSISTENCE_AFTER_5S_OF_INACTIVITY 438
#define I18N_KEY_SCRIVI 439
#define I18N_KEY_SELECT_ID_CCTALK_MODE_ONCHANGE_CLEARSERIAL_CCTALK_OPTION_VALUE_H 440
#define I18N_KEY_SELECT_ID_MDB_MODE_ONCHANGE_CLEARSERIAL_MDB_OPTION_VALUE_HEX_HEX 441
#define I18N_KEY_SELECT_ID_RS232_MODE_ONCHANGE_CLEARSERIAL_RS232_OPTION_VALUE_HEX 442
#define I18N_KEY_SELECT_ID_RS485_MODE_ONCHANGE_CLEARSERIAL_RS485_OPTION_VALUE_HEX 443
#define I18N_KEY_SELEZIONA_COLORE 444
#define I18N_KEY_SENSORE_SHT40 445
#define I18N_KEY_SENSORS 446
#define I18N_KEY_SERIALE_RS232 447
#define I18N_KEY_SERIALE_RS485 448
#define I18N_KEY_SPAN_CANALE_SPAN 449
#define I18N_KEY_SPAN_DUTY_SPAN_INPUT_TYPE_NUMBER_ID_PWM_DUTY_VALUE_50_MIN_0_MAX 450
#define I18N_KEY_SPAN_FREQ_HZ_SPAN_INPUT_TYPE_NUMBER_ID_PWM_FREQ_VALUE_1000_MIN_1 451
#define I18N_KEY_SPAN_ID_SD_MOUNTED_STATUS_STYLE_FONT_WEIGHT_BOLD_SPAN_DIV 452
#define I18N_KEY_SPAN_LUMINOSITA_SPAN_INPUT_TYPE_RANGE_ID_LED_BRIGHT_MIN_0_MAX_10 453
#define I18N_KEY_STATO_GPIO_IN_LETTURA 454
#define I18N_KEY_STATO_MONTAGGIO 455
#define I18N_KEY_STRISCIA_LED_WS2812 456
#define I18N_KEY_TEST_LOOPBACK_0X55_0XAA_0X01_0X07 457
#define I18N_KEY_TEST_LOOPBACK_ECHO 458
#define I18N_KEY_TEST_LOOPBACK_INVIA_PACCHETTO_DI_PROVA 459
#define I18N_KEY_TEXT 460
#define I18N_KEY_TEXT_2 461
#define I18N_KEY_ULTIME_LETTURE 462
#define I18N_KEY_ULTIMO_ERRORE 463
#define I18N_KEY_USCITE_CHIP_0X43 464
#define I18N_KEY_VALORI_CORRENTI 465
#define I18N_KEY_WEB_UI_TEST_PAGE 466
#define I18N_KEY_OUT_OF_SERVICE_TITLE 467
#define I18N_KEY_OUT_OF_SERVICE_SUB 468
#define I18N_KEY_CREDIT_LABEL 469
#define I18N_KEY_ELAPSED_FMT 470
#define I18N_KEY_PAUSE_FMT 471
#define I18N_KEY_PROGRAM_BTN_FMT 472
#define I18N_KEY_PROGRAMMA_9 473
#define I18N_KEY_PROGRAMMA_10 474

typedef struct {
    const char *symbol;
    uint8_t scope_id;
    uint16_t key_id;
    const char *scope_text;
    const char *key_text;
    const char *values[I18N_LANG_COUNT];
} i18n_language_model_t;

#define I18N_MODEL_COUNT 495
/*
 * Dataset i18n completo usato dagli script di generazione JSON.
 * Non viene compilato come array C per evitare uso memoria nel firmware.
 */
/* I18N_LANGUAGE_MODELS_DATA_START
 * {
 *   "entries": [
 *     {
 *       "key_id": 1,
 *       "key_text": "time_not_set",
 *       "scope_id": 1,
 *       "scope_text": "header",
 *       "symbol": "HEADER_TIME_NOT_SET",
 *       "values": [
 *         "Ora non impostata",
 *         "Time not set",
 *         "Zeit nicht eingestellt",
 *         "Heure non définie",
 *         "Hora no configurada"
 *       ]
 *     },
 *     {
 *       "key_id": 2,
 *       "key_text": "time_not_available",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_TIME_NOT_AVAILABLE",
 *       "values": [
 *         "Ora non disponibile",
 *         "Time not available",
 *         "Zeit nicht verfügbar",
 *         "Heure non disponible",
 *         "Hora no disponible"
 *       ]
 *     },
 *     {
 *       "key_id": 467,
 *       "key_text": "out_of_service_title",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_OUT_OF_SERVICE_TITLE",
 *       "values": [
 *         "Fuori servizio",
 *         "Out of service",
 *         "Out of service",
 *         "Out of service",
 *         "Out of service"
 *       ]
 *     },
 *     {
 *       "key_id": 468,
 *       "key_text": "out_of_service_sub",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_OUT_OF_SERVICE_SUB",
 *       "values": [
 *         "Reboot consecutivi: %lu\nContattare l'assistenza",
 *         "Consecutive reboots: %lu\nContact support",
 *         "Consecutive reboots: %lu\nContact support",
 *         "Consecutive reboots: %lu\nContact support",
 *         "Consecutive reboots: %lu\nContact support"
 *       ]
 *     },
 *     {
 *       "key_id": 469,
 *       "key_text": "credit_label",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_CREDIT_LABEL",
 *       "values": [
 *         "Credito",
 *         "Credit",
 *         "Credit",
 *         "Credit",
 *         "Credit"
 *       ]
 *     },
 *     {
 *       "key_id": 470,
 *       "key_text": "elapsed_fmt",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_ELAPSED_FMT",
 *       "values": [
 *         "Secondi   %s",
 *         "Seconds   %s",
 *         "Seconds   %s",
 *         "Seconds   %s",
 *         "Seconds   %s"
 *       ]
 *     },
 *     {
 *       "key_id": 471,
 *       "key_text": "pause_fmt",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_PAUSE_FMT",
 *       "values": [
 *         "Pausa: %s",
 *         "Pause: %s",
 *         "Pause: %s",
 *         "Pause: %s",
 *         "Pause: %s"
 *       ]
 *     },
 *     {
 *       "key_id": 472,
 *       "key_text": "program_btn_fmt",
 *       "scope_id": 2,
 *       "scope_text": "lvgl",
 *       "symbol": "LVGL_PROGRAM_BTN_FMT",
 *       "values": [
 *         "Programma %d",
 *         "Program %d",
 *         "Program %d",
 *         "Program %d",
 *         "Program %d"
 *       ]
 *     },
 *     {
 *       "key_id": 3,
 *       "key_text": "config",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_CONFIG",
 *       "values": [
 *         "Config",
 *         "Config",
 *         "Konfiguration",
 *         "Configuration",
 *         "Configuración"
 *       ]
 *     },
 *     {
 *       "key_id": 4,
 *       "key_text": "emulator",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_EMULATOR",
 *       "values": [
 *         "Emulatore",
 *         "Emulator",
 *         "Emulator",
 *         "Émulateur",
 *         "Emulador"
 *       ]
 *     },
 *     {
 *       "key_id": 5,
 *       "key_text": "home",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_HOME",
 *       "values": [
 *         "Home",
 *         "Home",
 *         "Startseite",
 *         "Accueil",
 *         "Inicio"
 *       ]
 *     },
 *     {
 *       "key_id": 6,
 *       "key_text": "logs",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_LOGS",
 *       "values": [
 *         "Log",
 *         "Logs",
 *         "Protokolle",
 *         "Journaux",
 *         "Registros"
 *       ]
 *     },
 *     {
 *       "key_id": 7,
 *       "key_text": "ota",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_OTA",
 *       "values": [
 *         "OTA",
 *         "OTA",
 *         "OTA",
 *         "OTA",
 *         "OTA"
 *       ]
 *     },
 *     {
 *       "key_id": 8,
 *       "key_text": "stats",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_STATS",
 *       "values": [
 *         "Statistiche",
 *         "Statistics",
 *         "Statistiken",
 *         "Statistiques",
 *         "Estadísticas"
 *       ]
 *     },
 *     {
 *       "key_id": 9,
 *       "key_text": "tasks",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_TASKS",
 *       "values": [
 *         "Task",
 *         "Tasks",
 *         "Aufgaben",
 *         "Tâches",
 *         "Tareas"
 *       ]
 *     },
 *     {
 *       "key_id": 10,
 *       "key_text": "test",
 *       "scope_id": 3,
 *       "scope_text": "nav",
 *       "symbol": "NAV_TEST",
 *       "values": [
 *         "Test",
 *         "Test",
 *         "Test",
 *         "Test",
 *         "Prueba"
 *       ]
 *     },
 *     {
 *       "key_id": 11,
 *       "key_text": "abilitato",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_ABILITATO",
 *       "values": [
 *         "Abilitato",
 *         "Enabled",
 *         "Aktiviert",
 *         "Activé",
 *         "Habilitado"
 *       ]
 *     },
 *     {
 *       "key_id": 12,
 *       "key_text": "aggiorna",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_AGGIORNA",
 *       "values": [
 *         "Aggiorna",
 *         "Update",
 *         "Aktualisieren",
 *         "Mettre à jour",
 *         "Actualizar"
 *       ]
 *     },
 *     {
 *       "key_id": 13,
 *       "key_text": "aggiorna_dati",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_AGGIORNA_DATI",
 *       "values": [
 *         "🔄 Aggiorna Dati",
 *         "🔄 Update Data",
 *         "🔄 Update Data",
 *         "🔄 Update Data",
 *         "🔄 Update Data"
 *       ]
 *     },
 *     {
 *       "key_id": 14,
 *       "key_text": "apri_editor_programmi",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_APRI_EDITOR_PROGRAMMI",
 *       "values": [
 *         "Apri editor programmi",
 *         "Open program editor",
 *         "Open program editor",
 *         "Open program editor",
 *         "Open program editor"
 *       ]
 *     },
 *     {
 *       "key_id": 15,
 *       "key_text": "attendere_il_riavvio",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_ATTENDERE_IL_RIAVVIO",
 *       "values": [
 *         "Attendere il riavvio.",
 *         "Wait for reboot.",
 *         "Wait for reboot.",
 *         "Wait for reboot.",
 *         "Wait for reboot."
 *       ]
 *     },
 *     {
 *       "key_id": 16,
 *       "key_text": "backup_config",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_BACKUP_CONFIG",
 *       "values": [
 *         "📥 Backup Config",
 *         "📥 Backup Config",
 *         "📥 Backup Config",
 *         "📥 Backup Config",
 *         "📥 Backup Config"
 *       ]
 *     },
 *     {
 *       "key_id": 17,
 *       "key_text": "base_server_url",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_BASE_SERVER_URL",
 *       "values": [
 *         "Base server URL",
 *         "Base server URL",
 *         "Base server URL",
 *         "Base server URL",
 *         "Base server URL"
 *       ]
 *     },
 *     {
 *       "key_id": 18,
 *       "key_text": "baudrate",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_BAUDRATE",
 *       "values": [
 *         "Baudrate",
 *         "Baudrate",
 *         "Baudrate",
 *         "Baudrate",
 *         "Baudrate"
 *       ]
 *     },
 *     {
 *       "key_id": 19,
 *       "key_text": "buffer_rx",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_BUFFER_RX",
 *       "values": [
 *         "Buffer RX",
 *         "Buffer RX",
 *         "Buffer RX",
 *         "Buffer RX",
 *         "Buffer RX"
 *       ]
 *     },
 *     {
 *       "key_id": 20,
 *       "key_text": "buffer_tx",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_BUFFER_TX",
 *       "values": [
 *         "Buffer TX",
 *         "Buffer TX",
 *         "Buffer TX",
 *         "Buffer TX",
 *         "Buffer TX"
 *       ]
 *     },
 *     {
 *       "key_id": 21,
 *       "key_text": "carica_firmware",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_CARICA_FIRMWARE",
 *       "values": [
 *         "⬆️ Carica Firmware",
 *         "⬆️ Upload Firmware",
 *         "⬆️ Upload Firmware",
 *         "⬆️ Upload Firmware",
 *         "⬆️ Upload Firmware"
 *       ]
 *     },
 *     {
 *       "key_id": 22,
 *       "key_text": "conferma_nuova_password",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_CONFERMA_NUOVA_PASSWORD",
 *       "values": [
 *         "Conferma nuova password",
 *         "Confirm new password",
 *         "Confirm new password",
 *         "Confirm new password",
 *         "Confirm new password"
 *       ]
 *     },
 *     {
 *       "key_id": 23,
 *       "key_text": "data_bits",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_DATA_BITS",
 *       "values": [
 *         "Data Bits",
 *         "Data Bits",
 *         "Data Bits",
 *         "Data Bits",
 *         "Data Bits"
 *       ]
 *     },
 *     {
 *       "key_id": 24,
 *       "key_text": "dhcp",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_DHCP",
 *       "values": [
 *         "DHCP",
 *         "DHCP",
 *         "DHCP",
 *         "DHCP",
 *         "DHCP"
 *       ]
 *     },
 *     {
 *       "key_id": 25,
 *       "key_text": "display",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_DISPLAY",
 *       "values": [
 *         "📺 Display",
 *         "📺 Display",
 *         "📺 Display",
 *         "📺 Display",
 *         "📺 Display"
 *       ]
 *     },
 *     {
 *       "key_id": 26,
 *       "key_text": "display_abilitato",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_DISPLAY_ABILITATO",
 *       "values": [
 *         "Display abilitato",
 *         "Display enabled",
 *         "Display enabled",
 *         "Display enabled",
 *         "Display enabled"
 *       ]
 *     },
 *     {
 *       "key_id": 27,
 *       "key_text": "dual_pid_opzionale",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_DUAL_PID_OPZIONALE",
 *       "values": [
 *         "Dual PID (opzionale)",
 *         "Dual PID (optional)",
 *         "Dual PID (optional)",
 *         "Dual PID (optional)",
 *         "Dual PID (optional)"
 *       ]
 *     },
 *     {
 *       "key_id": 28,
 *       "key_text": "ethernet",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_ETHERNET",
 *       "values": [
 *         "🌐 Ethernet",
 *         "🌐 Ethernet",
 *         "🌐 Ethernet",
 *         "🌐 Ethernet",
 *         "🌐 Ethernet"
 *       ]
 *     },
 *     {
 *       "key_id": 29,
 *       "key_text": "gateway",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_GATEWAY",
 *       "values": [
 *         "Gateway",
 *         "Gateway",
 *         "Gateway",
 *         "Gateway",
 *         "Gateway"
 *       ]
 *     },
 *     {
 *       "key_id": 30,
 *       "key_text": "gestione_prezzi_durata_e_relay_per_programma",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_GESTIONE_PREZZI_DURATA_E_RELAY_PER_PROGRAMMA",
 *       "values": [
 *         "Gestione prezzi, durata e relay per programma.",
 *         "Manage prices, duration and relay per program.",
 *         "Manage prices, duration and relay per program.",
 *         "Manage prices, duration and relay per program.",
 *         "Manage prices, duration and relay per program."
 *       ]
 *     },
 *     {
 *       "key_id": 31,
 *       "key_text": "gpio_33",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_GPIO_33",
 *       "values": [
 *         "GPIO 33",
 *         "GPIO 33",
 *         "GPIO 33",
 *         "GPIO 33",
 *         "GPIO 33"
 *       ]
 *     },
 *     {
 *       "key_id": 32,
 *       "key_text": "gpio_ausiliario_gpio33",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_GPIO_AUSILIARIO_GPIO33",
 *       "values": [
 *         "🔘 GPIO Ausiliario (GPIO33)",
 *         "🔘 Auxiliary GPIO (GPIO33)",
 *         "🔘 Auxiliary GPIO (GPIO33)",
 *         "🔘 Auxiliary GPIO (GPIO33)",
 *         "🔘 Auxiliary GPIO (GPIO33)"
 *       ]
 *     },
 *     {
 *       "key_id": 33,
 *       "key_text": "i_o_expander",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_I_O_EXPANDER",
 *       "values": [
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander"
 *       ]
 *     },
 *     {
 *       "key_id": 34,
 *       "key_text": "identita_dispositivo",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_IDENTITA_DISPOSITIVO",
 *       "values": [
 *         "🆔 Identità Dispositivo",
 *         "🆔 Device Identity",
 *         "🆔 Device Identity",
 *         "🆔 Device Identity",
 *         "🆔 Device Identity"
 *       ]
 *     },
 *     {
 *       "key_id": 35,
 *       "key_text": "indirizzo_ip",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_INDIRIZZO_IP",
 *       "values": [
 *         "Indirizzo IP",
 *         "IP Address",
 *         "IP Address",
 *         "IP Address",
 *         "IP Address"
 *       ]
 *     },
 *     {
 *       "key_id": 36,
 *       "key_text": "input_float",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_INPUT_FLOAT",
 *       "values": [
 *         "Input (Float)",
 *         "Input (Float)",
 *         "Input (Float)",
 *         "Input (Float)",
 *         "Input (Float)"
 *       ]
 *     },
 *     {
 *       "key_id": 37,
 *       "key_text": "input_pull_down",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_INPUT_PULL_DOWN",
 *       "values": [
 *         "Input (Pull-down)",
 *         "Input (Pull-down)",
 *         "Input (Pull-down)",
 *         "Input (Pull-down)",
 *         "Input (Pull-down)"
 *       ]
 *     },
 *     {
 *       "key_id": 38,
 *       "key_text": "input_pull_up",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_INPUT_PULL_UP",
 *       "values": [
 *         "Input (Pull-up)",
 *         "Input (Pull-up)",
 *         "Input (Pull-up)",
 *         "Input (Pull-up)",
 *         "Input (Pull-up)"
 *       ]
 *     },
 *     {
 *       "key_id": 39,
 *       "key_text": "italiano_it",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_ITALIANO_IT",
 *       "values": [
 *         "Italiano (IT)",
 *         "Italian (IT)",
 *         "Italian (IT)",
 *         "Italian (IT)",
 *         "Italian (IT)"
 *       ]
 *     },
 *     {
 *       "key_id": 40,
 *       "key_text": "led_strip_ws2812",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_LED_STRIP_WS2812",
 *       "values": [
 *         "LED Strip (WS2812)",
 *         "LED Strip (WS2812)",
 *         "LED Strip (WS2812)",
 *         "LED Strip (WS2812)",
 *         "LED Strip (WS2812)"
 *       ]
 *     },
 *     {
 *       "key_id": 41,
 *       "key_text": "lingua_ui",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_LINGUA_UI",
 *       "values": [
 *         "Lingua UI",
 *         "UI Language",
 *         "UI Language",
 *         "UI Language",
 *         "UI Language"
 *       ]
 *     },
 *     {
 *       "key_id": 42,
 *       "key_text": "logging_remoto",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_LOGGING_REMOTO",
 *       "values": [
 *         "📊 Logging Remoto",
 *         "📊 Remote Logging",
 *         "📊 Remote Logging",
 *         "📊 Remote Logging",
 *         "📊 Remote Logging"
 *       ]
 *     },
 *     {
 *       "key_id": 43,
 *       "key_text": "luminosita_lcd",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_LUMINOSITA_LCD",
 *       "values": [
 *         "Luminosità LCD (",
 *         "LCD Brightness (",
 *         "LCD Brightness (",
 *         "LCD Brightness (",
 *         "LCD Brightness ("
 *       ]
 *     },
 *     {
 *       "key_id": 44,
 *       "key_text": "mdb_configuration",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_MDB_CONFIGURATION",
 *       "values": [
 *         "MDB Configuration",
 *         "MDB Configuration",
 *         "MDB Configuration",
 *         "MDB Configuration",
 *         "MDB Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 45,
 *       "key_text": "mdb_engine",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_MDB_ENGINE",
 *       "values": [
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine"
 *       ]
 *     },
 *     {
 *       "key_id": 46,
 *       "key_text": "modalita",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_MODALITA",
 *       "values": [
 *         "Modalità",
 *         "Mode",
 *         "Mode",
 *         "Mode",
 *         "Mode"
 *       ]
 *     },
 *     {
 *       "key_id": 47,
 *       "key_text": "modifica_password_richiesta_per_emulator_e_reboot_factory",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_MODIFICA_PASSWORD_RICHIESTA_PER_EMULATOR_E_REBOOT_FACTORY",
 *       "values": [
 *         "Modifica password richiesta per /emulator e reboot FACTORY.",
 *         "Password change required for /emulator and FACTORY reboot.",
 *         "Password change required for /emulator and FACTORY reboot.",
 *         "Password change required for /emulator and FACTORY reboot.",
 *         "Password change required for /emulator and FACTORY reboot."
 *       ]
 *     },
 *     {
 *       "key_id": 48,
 *       "key_text": "nome_dispositivo",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_NOME_DISPOSITIVO",
 *       "values": [
 *         "Nome Dispositivo",
 *         "Device Name",
 *         "Device Name",
 *         "Device Name",
 *         "Device Name"
 *       ]
 *     },
 *     {
 *       "key_id": 49,
 *       "key_text": "ntp",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_NTP",
 *       "values": [
 *         "NTP",
 *         "NTP",
 *         "NTP",
 *         "NTP",
 *         "NTP"
 *       ]
 *     },
 *     {
 *       "key_id": 50,
 *       "key_text": "ntp_abilitato",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_NTP_ABILITATO",
 *       "values": [
 *         "NTP Abilitato",
 *         "NTP Enabled",
 *         "NTP Enabled",
 *         "NTP Enabled",
 *         "NTP Enabled"
 *       ]
 *     },
 *     {
 *       "key_id": 51,
 *       "key_text": "numero_di_led",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_NUMERO_DI_LED",
 *       "values": [
 *         "Numero di LED",
 *         "Number of LEDs",
 *         "Number of LEDs",
 *         "Number of LEDs",
 *         "Number of LEDs"
 *       ]
 *     },
 *     {
 *       "key_id": 52,
 *       "key_text": "nuova_password",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_NUOVA_PASSWORD",
 *       "values": [
 *         "Nuova password",
 *         "New password",
 *         "New password",
 *         "New password",
 *         "New password"
 *       ]
 *     },
 *     {
 *       "key_id": 53,
 *       "key_text": "offset_fuso_orario_ore",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_OFFSET_FUSO_ORARIO_ORE",
 *       "values": [
 *         "Offset Fuso Orario (ore)",
 *         "Timezone Offset (hours)",
 *         "Timezone Offset (hours)",
 *         "Timezone Offset (hours)",
 *         "Timezone Offset (hours)"
 *       ]
 *     },
 *     {
 *       "key_id": 54,
 *       "key_text": "output",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_OUTPUT",
 *       "values": [
 *         "Output",
 *         "Output",
 *         "Output",
 *         "Output",
 *         "Output"
 *       ]
 *     },
 *     {
 *       "key_id": 55,
 *       "key_text": "parita_0_none_1_odd_2_even",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PARITA_0_NONE_1_ODD_2_EVEN",
 *       "values": [
 *         "Parità (0:None, 1:Odd, 2:Even)",
 *         "Parity (0:None, 1:Odd, 2:Even)",
 *         "Parity (0:None, 1:Odd, 2:Even)",
 *         "Parity (0:None, 1:Odd, 2:Even)",
 *         "Parity (0:None, 1:Odd, 2:Even)"
 *       ]
 *     },
 *     {
 *       "key_id": 56,
 *       "key_text": "password",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PASSWORD",
 *       "values": [
 *         "Password",
 *         "Password",
 *         "Passwort",
 *         "Mot de passe",
 *         "Contraseña"
 *       ]
 *     },
 *     {
 *       "key_id": 57,
 *       "key_text": "password_attuale",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PASSWORD_ATTUALE",
 *       "values": [
 *         "Password attuale",
 *         "Current password",
 *         "Current password",
 *         "Current password",
 *         "Current password"
 *       ]
 *     },
 *     {
 *       "key_id": 58,
 *       "key_text": "password_boot_emulatore",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PASSWORD_BOOT_EMULATORE",
 *       "values": [
 *         "🔐 Password boot/emulatore",
 *         "🔐 Boot/emulator password",
 *         "🔐 Boot/emulator password",
 *         "🔐 Boot/emulator password",
 *         "🔐 Boot/emulator password"
 *       ]
 *     },
 *     {
 *       "key_id": 59,
 *       "key_text": "periferiche_hardware",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PERIFERICHE_HARDWARE",
 *       "values": [
 *         "🔌 Periferiche Hardware",
 *         "🔌 Hardware Peripherals",
 *         "🔌 Hardware Peripherals",
 *         "🔌 Hardware Peripherals",
 *         "🔌 Hardware Peripherals"
 *       ]
 *     },
 *     {
 *       "key_id": 60,
 *       "key_text": "pid",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PID",
 *       "values": [
 *         "PID",
 *         "PID",
 *         "PID",
 *         "PID",
 *         "PID"
 *       ]
 *     },
 *     {
 *       "key_id": 61,
 *       "key_text": "porta_udp",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PORTA_UDP",
 *       "values": [
 *         "Porta UDP",
 *         "UDP Port",
 *         "UDP Port",
 *         "UDP Port",
 *         "UDP Port"
 *       ]
 *     },
 *     {
 *       "key_id": 62,
 *       "key_text": "pwm_canale_1",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PWM_CANALE_1",
 *       "values": [
 *         "PWM Canale 1",
 *         "PWM Channel 1",
 *         "PWM Channel 1",
 *         "PWM Channel 1",
 *         "PWM Channel 1"
 *       ]
 *     },
 *     {
 *       "key_id": 63,
 *       "key_text": "pwm_canale_2",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_PWM_CANALE_2",
 *       "values": [
 *         "PWM Canale 2",
 *         "PWM Channel 2",
 *         "PWM Channel 2",
 *         "PWM Channel 2",
 *         "PWM Channel 2"
 *       ]
 *     },
 *     {
 *       "key_id": 64,
 *       "key_text": "reboot_in_app_last",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_REBOOT_IN_APP_LAST",
 *       "values": [
 *         "Reboot in App Last...",
 *         "Reboot in App Last...",
 *         "Reboot in App Last...",
 *         "Reboot in App Last...",
 *         "Reboot in App Last..."
 *       ]
 *     },
 *     {
 *       "key_id": 65,
 *       "key_text": "reboot_in_factory_mode",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_REBOOT_IN_FACTORY_MODE",
 *       "values": [
 *         "Reboot in Factory Mode...",
 *         "Reboot in Factory Mode...",
 *         "Reboot in Factory Mode...",
 *         "Reboot in Factory Mode...",
 *         "Reboot in Factory Mode..."
 *       ]
 *     },
 *     {
 *       "key_id": 66,
 *       "key_text": "reboot_in_ota0",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_REBOOT_IN_OTA0",
 *       "values": [
 *         "Reboot in OTA0...",
 *         "Reboot in OTA0...",
 *         "Reboot in OTA0...",
 *         "Reboot in OTA0...",
 *         "Reboot in OTA0..."
 *       ]
 *     },
 *     {
 *       "key_id": 67,
 *       "key_text": "reboot_in_ota1",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_REBOOT_IN_OTA1",
 *       "values": [
 *         "Reboot in OTA1...",
 *         "Reboot in OTA1...",
 *         "Reboot in OTA1...",
 *         "Reboot in OTA1...",
 *         "Reboot in OTA1..."
 *       ]
 *     },
 *     {
 *       "key_id": 68,
 *       "key_text": "reboot_in_production_mode",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_REBOOT_IN_PRODUCTION_MODE",
 *       "values": [
 *         "Reboot in Production Mode...",
 *         "Reboot in Production Mode...",
 *         "Reboot in Production Mode...",
 *         "Reboot in Production Mode...",
 *         "Reboot in Production Mode..."
 *       ]
 *     },
 *     {
 *       "key_id": 69,
 *       "key_text": "reset_fabbrica",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_RESET_FABBRICA",
 *       "values": [
 *         "⚠️ Reset Fabbrica",
 *         "⚠️ Factory Reset",
 *         "⚠️ Factory Reset",
 *         "⚠️ Factory Reset",
 *         "⚠️ Factory Reset"
 *       ]
 *     },
 *     {
 *       "key_id": 70,
 *       "key_text": "rs232_configuration",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_RS232_CONFIGURATION",
 *       "values": [
 *         "RS232 Configuration",
 *         "RS232 Configuration",
 *         "RS232 Configuration",
 *         "RS232 Configuration",
 *         "RS232 Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 71,
 *       "key_text": "rs485_configuration",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_RS485_CONFIGURATION",
 *       "values": [
 *         "RS485 Configuration",
 *         "RS485 Configuration",
 *         "RS485 Configuration",
 *         "RS485 Configuration",
 *         "RS485 Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 72,
 *       "key_text": "salva_configurazione",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SALVA_CONFIGURAZIONE",
 *       "values": [
 *         "💾 Salva Configurazione",
 *         "💾 Save Configuration",
 *         "💾 Save Configuration",
 *         "💾 Save Configuration",
 *         "💾 Save Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 73,
 *       "key_text": "salva_password_boot",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SALVA_PASSWORD_BOOT",
 *       "values": [
 *         "💾 Salva password boot",
 *         "💾 Save boot password",
 *         "💾 Save boot password",
 *         "💾 Save boot password",
 *         "💾 Save boot password"
 *       ]
 *     },
 *     {
 *       "key_id": 74,
 *       "key_text": "scanner_usb_cdc",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SCANNER_USB_CDC",
 *       "values": [
 *         "Scanner USB (CDC)",
 *         "USB Scanner (CDC)",
 *         "USB Scanner (CDC)",
 *         "USB Scanner (CDC)",
 *         "USB Scanner (CDC)"
 *       ]
 *     },
 *     {
 *       "key_id": 75,
 *       "key_text": "scheda_sd",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SCHEDA_SD",
 *       "values": [
 *         "Scheda SD",
 *         "SD Card",
 *         "SD Card",
 *         "SD Card",
 *         "SD Card"
 *       ]
 *     },
 *     {
 *       "key_id": 76,
 *       "key_text": "sensore_temperatura",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SENSORE_TEMPERATURA",
 *       "values": [
 *         "Sensore Temperatura",
 *         "Temperature Sensor",
 *         "Temperature Sensor",
 *         "Temperature Sensor",
 *         "Temperature Sensor"
 *       ]
 *     },
 *     {
 *       "key_id": 77,
 *       "key_text": "server_abilitato",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_ABILITATO",
 *       "values": [
 *         "Server abilitato",
 *         "Server enabled",
 *         "Server enabled",
 *         "Server enabled",
 *         "Server enabled"
 *       ]
 *     },
 *     {
 *       "key_id": 78,
 *       "key_text": "server_ntp_1",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_NTP_1",
 *       "values": [
 *         "Server NTP 1",
 *         "NTP Server 1",
 *         "NTP Server 1",
 *         "NTP Server 1",
 *         "NTP Server 1"
 *       ]
 *     },
 *     {
 *       "key_id": 79,
 *       "key_text": "server_ntp_2",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_NTP_2",
 *       "values": [
 *         "Server NTP 2",
 *         "NTP Server 2",
 *         "NTP Server 2",
 *         "NTP Server 2",
 *         "NTP Server 2"
 *       ]
 *     },
 *     {
 *       "key_id": 80,
 *       "key_text": "server_password_md5",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_PASSWORD_MD5",
 *       "values": [
 *         "Server password (MD5)",
 *         "Server password (MD5)",
 *         "Server password (MD5)",
 *         "Server password (MD5)",
 *         "Server password (MD5)"
 *       ]
 *     },
 *     {
 *       "key_id": 81,
 *       "key_text": "server_remoto",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_REMOTO",
 *       "values": [
 *         "🔁 Server Remoto",
 *         "🔁 Remote Server",
 *         "🔁 Remote Server",
 *         "🔁 Remote Server",
 *         "🔁 Remote Server"
 *       ]
 *     },
 *     {
 *       "key_id": 82,
 *       "key_text": "server_serial",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SERVER_SERIAL",
 *       "values": [
 *         "Server serial",
 *         "Serial Server",
 *         "Serial Server",
 *         "Serial Server",
 *         "Serial Server"
 *       ]
 *     },
 *     {
 *       "key_id": 83,
 *       "key_text": "ssid",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SSID",
 *       "values": [
 *         "SSID",
 *         "SSID",
 *         "SSID",
 *         "SSID",
 *         "SSID"
 *       ]
 *     },
 *     {
 *       "key_id": 84,
 *       "key_text": "stato_iniziale_solo_out",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_STATO_INIZIALE_SOLO_OUT",
 *       "values": [
 *         "Stato Iniziale (Solo Out)",
 *         "Initial State (Out Only)",
 *         "Initial State (Out Only)",
 *         "Initial State (Out Only)",
 *         "Initial State (Out Only)"
 *       ]
 *     },
 *     {
 *       "key_id": 85,
 *       "key_text": "stop_bits_1_2",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_STOP_BITS_1_2",
 *       "values": [
 *         "Stop Bits (1, 2)",
 *         "Stop Bits (1, 2)",
 *         "Stop Bits (1, 2)",
 *         "Stop Bits (1, 2)",
 *         "Stop Bits (1, 2)"
 *       ]
 *     },
 *     {
 *       "key_id": 86,
 *       "key_text": "subnet_mask",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_SUBNET_MASK",
 *       "values": [
 *         "Subnet Mask",
 *         "Subnet Mask",
 *         "Subnet Mask",
 *         "Subnet Mask",
 *         "Subnet Mask"
 *       ]
 *     },
 *     {
 *       "key_id": 87,
 *       "key_text": "tabella_programmi",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_TABELLA_PROGRAMMI",
 *       "values": [
 *         "📊 Tabella Programmi",
 *         "📊 Program Table",
 *         "📊 Program Table",
 *         "📊 Program Table",
 *         "📊 Program Table"
 *       ]
 *     },
 *     {
 *       "key_id": 88,
 *       "key_text": "tag_porte_seriali",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_TAG_PORTE_SERIALI",
 *       "values": [
 *         "(TAG) Porte Seriali",
 *         "(TAG) Serial Ports",
 *         "(TAG) Serial Ports",
 *         "(TAG) Serial Ports",
 *         "(TAG) Serial Ports"
 *       ]
 *     },
 *     {
 *       "key_id": 89,
 *       "key_text": "uart_rs232",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_UART_RS232",
 *       "values": [
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232"
 *       ]
 *     },
 *     {
 *       "key_id": 90,
 *       "key_text": "uart_rs485",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_UART_RS485",
 *       "values": [
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485"
 *       ]
 *     },
 *     {
 *       "key_id": 91,
 *       "key_text": "usa_broadcast_udp",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_USA_BROADCAST_UDP",
 *       "values": [
 *         "Usa broadcast UDP",
 *         "Use UDP broadcast",
 *         "Use UDP broadcast",
 *         "Use UDP broadcast",
 *         "Use UDP broadcast"
 *       ]
 *     },
 *     {
 *       "key_id": 92,
 *       "key_text": "vid",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_VID",
 *       "values": [
 *         "VID",
 *         "VID",
 *         "VID",
 *         "VID",
 *         "VID"
 *       ]
 *     },
 *     {
 *       "key_id": 93,
 *       "key_text": "wifi_abilitato",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_WIFI_ABILITATO",
 *       "values": [
 *         "WiFi Abilitato",
 *         "WiFi Enabled",
 *         "WiFi Enabled",
 *         "WiFi Enabled",
 *         "WiFi Enabled"
 *       ]
 *     },
 *     {
 *       "key_id": 94,
 *       "key_text": "wifi_sta",
 *       "scope_id": 4,
 *       "scope_text": "p_config",
 *       "symbol": "P_CONFIG_WIFI_STA",
 *       "values": [
 *         "📡 WiFi STA",
 *         "📡 WiFi STA",
 *         "📡 WiFi STA",
 *         "📡 WiFi STA",
 *         "📡 WiFi STA"
 *       ]
 *     },
 *     {
 *       "key_id": 95,
 *       "key_text": "annulla",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_ANNULLA",
 *       "values": [
 *         "Annulla",
 *         "Cancel",
 *         "Cancel",
 *         "Cancel",
 *         "Cancel"
 *       ]
 *     },
 *     {
 *       "key_id": 96,
 *       "key_text": "continua",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_CONTINUA",
 *       "values": [
 *         "Continua",
 *         "Continue",
 *         "Continue",
 *         "Continue",
 *         "Continue"
 *       ]
 *     },
 *     {
 *       "key_id": 97,
 *       "key_text": "credito",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_CREDITO",
 *       "values": [
 *         "Credito",
 *         "Credit",
 *         "Credit",
 *         "Credit",
 *         "Credit"
 *       ]
 *     },
 *     {
 *       "key_id": 98,
 *       "key_text": "in_pausa_00_00",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_IN_PAUSA_00_00",
 *       "values": [
 *         "In pausa: 00:00",
 *         "Paused: 00:00",
 *         "Paused: 00:00",
 *         "Paused: 00:00",
 *         "Paused: 00:00"
 *       ]
 *     },
 *     {
 *       "key_id": 99,
 *       "key_text": "inserire_la_password_per_continuare",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_INSERIRE_LA_PASSWORD_PER_CONTINUARE",
 *       "values": [
 *         "Inserire la password per continuare.",
 *         "Enter password to continue.",
 *         "Enter password to continue.",
 *         "Enter password to continue.",
 *         "Enter password to continue."
 *       ]
 *     },
 *     {
 *       "key_id": 100,
 *       "key_text": "layout_operativo_pannello_utente_800x1280_a_sinistra_quadro_elet",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_LAYOUT_OPERATIVO_PANNELLO_UTENTE_800X1280_A_SINISTRA_QUADRO_ELET",
 *       "values": [
 *         "Layout operativo: pannello utente 800x1280 a sinistra, quadro elettrico a destra",
 *         "Operating layout: user panel 800x1280 on left, electrical panel on right",
 *         "Operating layout: user panel 800x1280 on left, electrical panel on right",
 *         "Operating layout: user panel 800x1280 on left, electrical panel on right",
 *         "Operating layout: user panel 800x1280 on left, electrical panel on right"
 *       ]
 *     },
 *     {
 *       "key_id": 101,
 *       "key_text": "nessun_evento_in_coda",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_NESSUN_EVENTO_IN_CODA",
 *       "values": [
 *         "Nessun evento in coda",
 *         "No events in queue",
 *         "No events in queue",
 *         "No events in queue",
 *         "No events in queue"
 *       ]
 *     },
 *     {
 *       "key_id": 102,
 *       "key_text": "password_richiesta",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PASSWORD_RICHIESTA",
 *       "values": [
 *         "🔒 Password richiesta",
 *         "🔒 Password required",
 *         "🔒 Password required",
 *         "🔒 Password required",
 *         "🔒 Password required"
 *       ]
 *     },
 *     {
 *       "key_id": 103,
 *       "key_text": "programma_1",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_1",
 *       "values": [
 *         "Programma 1",
 *         "Program 1",
 *         "Program 1",
 *         "Program 1",
 *         "Program 1"
 *       ]
 *     },
 *     {
 *       "key_id": 104,
 *       "key_text": "programma_2",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_2",
 *       "values": [
 *         "Programma 2",
 *         "Program 2",
 *         "Program 2",
 *         "Program 2",
 *         "Program 2"
 *       ]
 *     },
 *     {
 *       "key_id": 105,
 *       "key_text": "programma_3",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_3",
 *       "values": [
 *         "Programma 3",
 *         "Program 3",
 *         "Program 3",
 *         "Program 3",
 *         "Program 3"
 *       ]
 *     },
 *     {
 *       "key_id": 106,
 *       "key_text": "programma_4",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_4",
 *       "values": [
 *         "Programma 4",
 *         "Program 4",
 *         "Program 4",
 *         "Program 4",
 *         "Program 4"
 *       ]
 *     },
 *     {
 *       "key_id": 107,
 *       "key_text": "programma_5",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_5",
 *       "values": [
 *         "Programma 5",
 *         "Program 5",
 *         "Program 5",
 *         "Program 5",
 *         "Program 5"
 *       ]
 *     },
 *     {
 *       "key_id": 108,
 *       "key_text": "programma_6",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_6",
 *       "values": [
 *         "Programma 6",
 *         "Program 6",
 *         "Program 6",
 *         "Program 6",
 *         "Program 6"
 *       ]
 *     },
 *     {
 *       "key_id": 109,
 *       "key_text": "programma_7",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_7",
 *       "values": [
 *         "Programma 7",
 *         "Program 7",
 *         "Program 7",
 *         "Program 7",
 *         "Program 7"
 *       ]
 *     },
 *     {
 *       "key_id": 110,
 *       "key_text": "programma_8",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_8",
 *       "values": [
 *         "Programma 8",
 *         "Program 8",
 *         "Program 8",
 *         "Program 8",
 *         "Program 8"
 *       ]
 *     },
 *     {
 *       "key_id": 111,
 *       "key_text": "quadro_elettrico",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_QUADRO_ELETTRICO",
 *       "values": [
 *         "Quadro elettrico",
 *         "Electrical panel",
 *         "Electrical panel",
 *         "Electrical panel",
 *         "Electrical panel"
 *       ]
 *     },
 *     {
 *       "key_id": 112,
 *       "key_text": "r1",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R1",
 *       "values": [
 *         "R1",
 *         "R1",
 *         "R1",
 *         "R1",
 *         "R1"
 *       ]
 *     },
 *     {
 *       "key_id": 113,
 *       "key_text": "r10",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R10",
 *       "values": [
 *         "R10",
 *         "R10",
 *         "R10",
 *         "R10",
 *         "R10"
 *       ]
 *     },
 *     {
 *       "key_id": 114,
 *       "key_text": "r2",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R2",
 *       "values": [
 *         "R2",
 *         "R2",
 *         "R2",
 *         "R2",
 *         "R2"
 *       ]
 *     },
 *     {
 *       "key_id": 115,
 *       "key_text": "r3",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R3",
 *       "values": [
 *         "R3",
 *         "R3",
 *         "R3",
 *         "R3",
 *         "R3"
 *       ]
 *     },
 *     {
 *       "key_id": 116,
 *       "key_text": "r4",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R4",
 *       "values": [
 *         "R4",
 *         "R4",
 *         "R4",
 *         "R4",
 *         "R4"
 *       ]
 *     },
 *     {
 *       "key_id": 117,
 *       "key_text": "r5",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R5",
 *       "values": [
 *         "R5",
 *         "R5",
 *         "R5",
 *         "R5",
 *         "R5"
 *       ]
 *     },
 *     {
 *       "key_id": 118,
 *       "key_text": "r6",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R6",
 *       "values": [
 *         "R6",
 *         "R6",
 *         "R6",
 *         "R6",
 *         "R6"
 *       ]
 *     },
 *     {
 *       "key_id": 119,
 *       "key_text": "r7",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R7",
 *       "values": [
 *         "R7",
 *         "R7",
 *         "R7",
 *         "R7",
 *         "R7"
 *       ]
 *     },
 *     {
 *       "key_id": 120,
 *       "key_text": "r8",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R8",
 *       "values": [
 *         "R8",
 *         "R8",
 *         "R8",
 *         "R8",
 *         "R8"
 *       ]
 *     },
 *     {
 *       "key_id": 121,
 *       "key_text": "r9",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_R9",
 *       "values": [
 *         "R9",
 *         "R9",
 *         "R9",
 *         "R9",
 *         "R9"
 *       ]
 *     },
 *     {
 *       "key_id": 122,
 *       "key_text": "relay",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_RELAY",
 *       "values": [
 *         "Relay",
 *         "Relay",
 *         "Relay",
 *         "Relay",
 *         "Relay"
 *       ]
 *     },
 *     {
 *       "key_id": 123,
 *       "key_text": "ricarica_coin",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_RICARICA_COIN",
 *       "values": [
 *         "Ricarica Coin",
 *         "Recharge Coin",
 *         "Recharge Coin",
 *         "Recharge Coin",
 *         "Recharge Coin"
 *       ]
 *     },
 *     {
 *       "key_id": 124,
 *       "key_text": "stato_credito",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_STATO_CREDITO",
 *       "values": [
 *         "Stato credito",
 *         "Credit status",
 *         "Credit status",
 *         "Credit status",
 *         "Credit status"
 *       ]
 *     },
 *     {
 *       "key_id": 125,
 *       "key_text": "tempo_00_00",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_TEMPO_00_00",
 *       "values": [
 *         "Tempo: 00:00",
 *         "Time: 00:00",
 *         "Time: 00:00",
 *         "Time: 00:00",
 *         "Time: 00:00"
 *       ]
 *     },
 *     {
 *       "key_id": 473,
 *       "key_text": "programma_9",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_9",
 *       "values": [
 *         "Programma 9",
 *         "Program 9",
 *         "Program 9",
 *         "Program 9",
 *         "Program 9"
 *       ]
 *     },
 *     {
 *       "key_id": 474,
 *       "key_text": "programma_10",
 *       "scope_id": 5,
 *       "scope_text": "p_emulator",
 *       "symbol": "P_EMULATOR_PROGRAMMA_10",
 *       "values": [
 *         "Programma 10",
 *         "Program 10",
 *         "Program 10",
 *         "Program 10",
 *         "Program 10"
 *       ]
 *     },
 *     {
 *       "key_id": 126,
 *       "key_text": "applica_filtro",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_APPLICA_FILTRO",
 *       "values": [
 *         "Applica filtro",
 *         "Apply filter",
 *         "Apply filter",
 *         "Apply filter",
 *         "Apply filter"
 *       ]
 *     },
 *     {
 *       "key_id": 127,
 *       "key_text": "configura_il_logging_remoto_nella_pagina",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_CONFIGURA_IL_LOGGING_REMOTO_NELLA_PAGINA",
 *       "values": [
 *         "Configura il logging remoto nella pagina",
 *         "Configure remote logging on the page",
 *         "Configure remote logging on the page",
 *         "Configure remote logging on the page",
 *         "Configure remote logging on the page"
 *       ]
 *     },
 *     {
 *       "key_id": 128,
 *       "key_text": "configurazione",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_CONFIGURAZIONE",
 *       "values": [
 *         "Configurazione",
 *         "Configuration",
 *         "Configuration",
 *         "Configuration",
 *         "Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 129,
 *       "key_text": "dere_i_nuovi_log_applica_livelli",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_DERE_I_NUOVI_LOG_APPLICA_LIVELLI",
 *       "values": [
 *         "dere i nuovi log.                                                               Applica livelli",
 *         "dere new logs.                                                               Apply levels",
 *         "dere new logs.                                                               Apply levels",
 *         "dere new logs.                                                               Apply levels",
 *         "dere new logs.                                                               Apply levels"
 *       ]
 *     },
 *     {
 *       "key_id": 130,
 *       "key_text": "html",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_HTML",
 *       "values": [
 *         "html += '",
 *         "html += '",
 *         "html += '",
 *         "html += '",
 *         "html += '"
 *       ]
 *     },
 *     {
 *       "key_id": 131,
 *       "key_text": "i_log_vengono_ricevuti_via_udp_dal_server_configurato_aggiorna_l",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_I_LOG_VENGONO_RICEVUTI_VIA_UDP_DAL_SERVER_CONFIGURATO_AGGIORNA_L",
 *       "values": [
 *         "I log vengono ricevuti via UDP dal server configurato. Aggiorna la pagina per ve",
 *         "Logs are received via UDP from the configured server. Refresh the page to ve",
 *         "Logs are received via UDP from the configured server. Refresh the page to ve",
 *         "Logs are received via UDP from the configured server. Refresh the page to ve",
 *         "Logs are received via UDP from the configured server. Refresh the page to ve"
 *       ]
 *     },
 *     {
 *       "key_id": 132,
 *       "key_text": "in_attesa_di_log",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_IN_ATTESA_DI_LOG",
 *       "values": [
 *         "In attesa di log...",
 *         "Waiting for logs...",
 *         "Waiting for logs...",
 *         "Waiting for logs...",
 *         "Waiting for logs..."
 *       ]
 *     },
 *     {
 *       "key_id": 133,
 *       "key_text": "item_tag",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_ITEM_TAG",
 *       "values": [
 *         "' + item.tag + '",
 *         "' + item.tag + '",
 *         "' + item.tag + '",
 *         "' + item.tag + '",
 *         "' + item.tag + '"
 *       ]
 *     },
 *     {
 *       "key_id": 134,
 *       "key_text": "l_t",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_L_T",
 *       "values": [
 *         "' + l.t + '",
 *         "' + l.t + '",
 *         "' + l.t + '",
 *         "' + l.t + '",
 *         "' + l.t + '"
 *       ]
 *     },
 *     {
 *       "key_id": 135,
 *       "key_text": "log_remoto_ricevuti",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_LOG_REMOTO_RICEVUTI",
 *       "values": [
 *         "📋 Log Remoto Ricevuti",
 *         "📋 Remote Logs Received",
 *         "📋 Remote Logs Received",
 *         "📋 Remote Logs Received",
 *         "📋 Remote Logs Received"
 *       ]
 *     },
 *     {
 *       "key_id": 136,
 *       "key_text": "nessun_tag_disponibile",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_NESSUN_TAG_DISPONIBILE",
 *       "values": [
 *         "Nessun tag disponibile",
 *         "No tags available",
 *         "No tags available",
 *         "No tags available",
 *         "No tags available"
 *       ]
 *     },
 *     {
 *       "key_id": 137,
 *       "key_text": "per_iniziare_a_ricevere_log",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_PER_INIZIARE_A_RICEVERE_LOG",
 *       "values": [
 *         "per iniziare a ricevere log.",
 *         "to start receiving logs.",
 *         "to start receiving logs.",
 *         "to start receiving logs.",
 *         "to start receiving logs."
 *       ]
 *     },
 *     {
 *       "key_id": 138,
 *       "key_text": "pulisci_filtro",
 *       "scope_id": 6,
 *       "scope_text": "p_logs",
 *       "symbol": "P_LOGS_PULISCI_FILTRO",
 *       "values": [
 *         "Pulisci filtro",
 *         "Clear filter",
 *         "Clear filter",
 *         "Clear filter",
 *         "Clear filter"
 *       ]
 *     },
 *     {
 *       "key_id": 11,
 *       "key_text": "abilitato",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_ABILITATO",
 *       "values": [
 *         "Abilitato",
 *         "Enabled",
 *         "Aktiviert",
 *         "Activé",
 *         "Habilitado"
 *       ]
 *     },
 *     {
 *       "key_id": 87,
 *       "key_text": "tabella_programmi",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_TABELLA_PROGRAMMI",
 *       "values": [
 *         "Tabella Programmi",
 *         "Program Table",
 *         "Program Table",
 *         "Program Table",
 *         "Program Table"
 *       ]
 *     },
 *     {
 *       "key_id": 139,
 *       "key_text": "400_bad_request",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_400_BAD_REQUEST",
 *       "values": [
 *         "400 Bad Request",
 *         "400 Bad Request",
 *         "400 Bad Request",
 *         "400 Bad Request",
 *         "400 Bad Request"
 *       ]
 *     },
 *     {
 *       "key_id": 140,
 *       "key_text": "403_forbidden",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_403_FORBIDDEN",
 *       "values": [
 *         "403 Forbidden",
 *         "403 Forbidden",
 *         "403 Forbidden",
 *         "403 Forbidden",
 *         "403 Forbidden"
 *       ]
 *     },
 *     {
 *       "key_id": 141,
 *       "key_text": "404_non_trovato",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_404_NON_TROVATO",
 *       "values": [
 *         "404 Non Trovato",
 *         "404 Not Found",
 *         "404 Not Found",
 *         "404 Not Found",
 *         "404 Not Found"
 *       ]
 *     },
 *     {
 *       "key_id": 142,
 *       "key_text": "405_method_not_allowed",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_405_METHOD_NOT_ALLOWED",
 *       "values": [
 *         "405 Method Not Allowed",
 *         "405 Method Not Allowed",
 *         "405 Method Not Allowed",
 *         "405 Method Not Allowed",
 *         "405 Method Not Allowed"
 *       ]
 *     },
 *     {
 *       "key_id": 143,
 *       "key_text": "button_class_btn_secondary_onclick_loadprograms_ricarica_button",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_BUTTON_CLASS_BTN_SECONDARY_ONCLICK_LOADPROGRAMS_RICARICA_BUTTON",
 *       "values": [
 *         "<button class='btn-secondary' onclick='loadPrograms()'>🔄 Ricarica</button>",
 *         " 🔄 Reload",
 *         " 🔄 Reload",
 *         " 🔄 Reload",
 *         " 🔄 Reload"
 *       ]
 *     },
 *     {
 *       "key_id": 144,
 *       "key_text": "button_onclick_saveprograms_salva_tabella_button",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_BUTTON_ONCLICK_SAVEPROGRAMS_SALVA_TABELLA_BUTTON",
 *       "values": [
 *         "<button onclick='savePrograms()'>💾 Salva tabella</button>",
 *         " 💾 Save table",
 *         " 💾 Save table",
 *         " 💾 Save table",
 *         " 💾 Save table"
 *       ]
 *     },
 *     {
 *       "key_id": 145,
 *       "key_text": "c_get_config_programs",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_C_GET_CONFIG_PROGRAMS",
 *       "values": [
 *         "[C] GET /config/programs",
 *         "[C] GET /config/programs",
 *         "[C] GET /config/programs",
 *         "[C] GET /config/programs",
 *         "[C] GET /config/programs"
 *       ]
 *     },
 *     {
 *       "key_id": 146,
 *       "key_text": "campi_current_password_new_password_obbligatori",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_CAMPI_CURRENT_PASSWORD_NEW_PASSWORD_OBBLIGATORI",
 *       "values": [
 *         "Campi current_password/new_password obbligatori",
 *         "Fields current_password/new_password required",
 *         "Fields current_password/new_password required",
 *         "Fields current_password/new_password required",
 *         "Fields current_password/new_password required"
 *       ]
 *     },
 *     {
 *       "key_id": 147,
 *       "key_text": "campi_relay_number_status_duration_obbligatori",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_CAMPI_RELAY_NUMBER_STATUS_DURATION_OBBLIGATORI",
 *       "values": [
 *         "Campi relay_number/status/duration obbligatori",
 *         "Fields relay_number/status/duration required",
 *         "Fields relay_number/status/duration required",
 *         "Fields relay_number/status/duration required",
 *         "Fields relay_number/status/duration required"
 *       ]
 *     },
 *     {
 *       "key_id": 148,
 *       "key_text": "div",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_DIV",
 *       "values": [
 *         "</div>",
 *         " ",
 *         " ",
 *         " ",
 *         " "
 *       ]
 *     },
 *     {
 *       "key_id": 149,
 *       "key_text": "div_class_container_div_class_section",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_DIV_CLASS_CONTAINER_DIV_CLASS_SECTION",
 *       "values": [
 *         "<div class='container'><div class='section'>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 150,
 *       "key_text": "div_div",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_DIV_DIV",
 *       "values": [
 *         "</div></div>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 151,
 *       "key_text": "div_id_status_class_status_div",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_DIV_ID_STATUS_CLASS_STATUS_DIV",
 *       "values": [
 *         "<div id='status' class='status'></div>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 152,
 *       "key_text": "durata_s",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_DURATA_S",
 *       "values": [
 *         "Durata (s)",
 *         "Duration (s)",
 *         "Duration (s)",
 *         "Duration (s)",
 *         "Duration (s)"
 *       ]
 *     },
 *     {
 *       "key_id": 153,
 *       "key_text": "editor_tabella_programmi_factory",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_EDITOR_TABELLA_PROGRAMMI_FACTORY",
 *       "values": [
 *         "📊 Editor Tabella Programmi (FACTORY)",
 *         "📊 Program Table Editor (FACTORY)",
 *         "📊 Program Table Editor (FACTORY)",
 *         "📊 Program Table Editor (FACTORY)",
 *         "📊 Program Table Editor (FACTORY)"
 *       ]
 *     },
 *     {
 *       "key_id": 154,
 *       "key_text": "errore_lettura_payload",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_ERRORE_LETTURA_PAYLOAD",
 *       "values": [
 *         "Errore lettura payload",
 *         "Error reading payload",
 *         "Error reading payload",
 *         "Error reading payload",
 *         "Error reading payload"
 *       ]
 *     },
 *     {
 *       "key_id": 155,
 *       "key_text": "errore_validazione",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_ERRORE_VALIDAZIONE",
 *       "values": [
 *         "Errore validazione",
 *         "Validation error",
 *         "Validation error",
 *         "Validation error",
 *         "Validation error"
 *       ]
 *     },
 *     {
 *       "key_id": 156,
 *       "key_text": "h2_editor_tabella_programmi_factory_h2",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_H2_EDITOR_TABELLA_PROGRAMMI_FACTORY_H2",
 *       "values": [
 *         "<h2>📊 Editor Tabella Programmi (FACTORY)</h2>",
 *         "\n📊 Program Table Editor (FACTORY)\n",
 *         "\n📊 Program Table Editor (FACTORY)\n",
 *         "\n📊 Program Table Editor (FACTORY)\n",
 *         "\n📊 Program Table Editor (FACTORY)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 157,
 *       "key_text": "id",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_ID",
 *       "values": [
 *         "ID",
 *         "ID",
 *         "ID",
 *         "ID",
 *         "ID"
 *       ]
 *     },
 *     {
 *       "key_id": 158,
 *       "key_text": "imposta_nome_abilitazione_prezzo_durata_e_relay_mask_per_ogni_pr",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI_PR",
 *       "values": [
 *         "Imposta nome, abilitazione, prezzo, durata e relay mask per ogni programma.",
 *         "Set name, enable, price, duration and relay mask for each program.",
 *         "Set name, enable, price, duration and relay mask for each program.",
 *         "Set name, enable, price, duration and relay mask for each program.",
 *         "Set name, enable, price, duration and relay mask for each program."
 *       ]
 *     },
 *     {
 *       "key_id": 159,
 *       "key_text": "json_non_valido",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_JSON_NON_VALIDO",
 *       "values": [
 *         "JSON non valido",
 *         "Invalid JSON",
 *         "Invalid JSON",
 *         "Invalid JSON",
 *         "Invalid JSON"
 *       ]
 *     },
 *     {
 *       "key_id": 160,
 *       "key_text": "method_not_allowed",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_METHOD_NOT_ALLOWED",
 *       "values": [
 *         "Method not allowed",
 *         "Method not allowed",
 *         "Method not allowed",
 *         "Method not allowed",
 *         "Method not allowed"
 *       ]
 *     },
 *     {
 *       "key_id": 161,
 *       "key_text": "nome",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_NOME",
 *       "values": [
 *         "Nome",
 *         "Name",
 *         "Name",
 *         "Name",
 *         "Name"
 *       ]
 *     },
 *     {
 *       "key_id": 162,
 *       "key_text": "nuova_password_non_valida_o_errore_salvataggio",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_NUOVA_PASSWORD_NON_VALIDA_O_ERRORE_SALVATAGGIO",
 *       "values": [
 *         "Nuova password non valida o errore salvataggio",
 *         "New password invalid or save error",
 *         "New password invalid or save error",
 *         "New password invalid or save error",
 *         "New password invalid or save error"
 *       ]
 *     },
 *     {
 *       "key_id": 163,
 *       "key_text": "p_imposta_nome_abilitazione_prezzo_durata_e_relay_mask_per_ogni_",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_P_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI",
 *       "values": [
 *         "<p>Imposta nome, abilitazione, prezzo, durata e relay mask per ogni programma.</p>",
 *         "\nSet name, enable, price, duration and relay mask for each program.\n",
 *         "\nSet name, enable, price, duration and relay mask for each program.\n",
 *         "\nSet name, enable, price, duration and relay mask for each program.\n",
 *         "\nSet name, enable, price, duration and relay mask for each program.\n"
 *       ]
 *     },
 *     {
 *       "key_id": 164,
 *       "key_text": "password_attuale_non_valida",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_PASSWORD_ATTUALE_NON_VALIDA",
 *       "values": [
 *         "Password attuale non valida",
 *         "Current password invalid",
 *         "Current password invalid",
 *         "Current password invalid",
 *         "Current password invalid"
 *       ]
 *     },
 *     {
 *       "key_id": 165,
 *       "key_text": "pausa_max_s",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_PAUSA_MAX_S",
 *       "values": [
 *         "Pausa Max (s)",
 *         "Max Pause (s)",
 *         "Max Pause (s)",
 *         "Max Pause (s)",
 *         "Max Pause (s)"
 *       ]
 *     },
 *     {
 *       "key_id": 166,
 *       "key_text": "pause_all_seconds",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_PAUSE_ALL_SECONDS",
 *       "values": [
 *         "Secondi pausa (tutti)",
 *         "Seconds pause (all)",
 *         "Seconds pause (all)",
 *         "Seconds pause (all)",
 *         "Seconds pause (all)"
 *       ]
 *     },
 *     {
 *       "key_id": 167,
 *       "key_text": "set_pause_all",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_SET_PAUSE_ALL",
 *       "values": [
 *         "Imposta",
 *         "Set",
 *         "Set",
 *         "Set",
 *         "Set"
 *       ]
 *     },
 *     {
 *       "key_id": 168,
 *       "key_text": "payload_non_valido",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_PAYLOAD_NON_VALIDO",
 *       "values": [
 *         "Payload non valido",
 *         "Invalid payload",
 *         "Invalid payload",
 *         "Invalid payload",
 *         "Invalid payload"
 *       ]
 *     },
 *     {
 *       "key_id": 169,
 *       "key_text": "prezzo",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_PREZZO",
 *       "values": [
 *         "Prezzo",
 *         "Price",
 *         "Price",
 *         "Price",
 *         "Price"
 *       ]
 *     },
 *     {
 *       "key_id": 170,
 *       "key_text": "relay_mask",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_RELAY_MASK",
 *       "values": [
 *         "Relay mask",
 *         "Relay mask",
 *         "Relay mask",
 *         "Relay mask",
 *         "Relay mask"
 *       ]
 *     },
 *     {
 *       "key_id": 171,
 *       "key_text": "relay_number_fuori_range",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_RELAY_NUMBER_FUORI_RANGE",
 *       "values": [
 *         "relay_number fuori range",
 *         "relay_number out of range",
 *         "relay_number out of range",
 *         "relay_number out of range",
 *         "relay_number out of range"
 *       ]
 *     },
 *     {
 *       "key_id": 172,
 *       "key_text": "return",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_RETURN",
 *       "values": [
 *         "return `",
 *         "return",
 *         "return",
 *         "return",
 *         "return"
 *       ]
 *     },
 *     {
 *       "key_id": 173,
 *       "key_text": "return_tr",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_RETURN_TR",
 *       "values": [
 *         "return `<tr>`+",
 *         "return",
 *         "return",
 *         "return",
 *         "return"
 *       ]
 *     },
 *     {
 *       "key_id": 174,
 *       "key_text": "ricarica",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_RICARICA",
 *       "values": [
 *         "🔄 Ricarica",
 *         "🔄 Reload",
 *         "🔄 Reload",
 *         "🔄 Reload",
 *         "🔄 Reload"
 *       ]
 *     },
 *     {
 *       "key_id": 175,
 *       "key_text": "salva_tabella",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_SALVA_TABELLA",
 *       "values": [
 *         "💾 Salva tabella",
 *         "💾 Save table",
 *         "💾 Save table",
 *         "💾 Save table",
 *         "💾 Save table"
 *       ]
 *     },
 *     {
 *       "key_id": 176,
 *       "key_text": "script_body_html",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_SCRIPT_BODY_HTML",
 *       "values": [
 *         "</script></body></html>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 177,
 *       "key_text": "table_thead_tr_th_id_th_th_nome_th_th_abilitato_th_th_prezzo_th_",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_TABLE_THEAD_TR_TH_ID_TH_TH_NOME_TH_TH_ABILITATO_TH_TH_PREZZO_TH",
 *       "values": [
 *         "<table><thead><tr><th>ID</th><th>Nome</th><th>Abilitato</th><th>Prezzo</th><th>Durata (s)</th><th>Pausa Max (s)</th><th>Relay mask</th></tr></thead><tbody id='programRows'></tbody></table>",
 *         "\n| ID\n|Name\n|Enabled\n|Price\n|Duration (s)\n|Max Pause (s)\n|Relay mask\n|\n| ---|---|---|---|---|---|---|\n",
 *         "\n| ID\n|Name\n|Enabled\n|Price\n|Duration (s)\n|Max Pause (s)\n|Relay mask\n|\n| ---|---|---|---|---|---|---|\n",
 *         "\n| ID\n|Name\n|Enabled\n|Price\n|Duration (s)\n|Max Pause (s)\n|Relay mask\n|\n| ---|---|---|---|---|---|---|\n",
 *         "\n| ID\n|Name\n|Enabled\n|Price\n|Duration (s)\n|Max Pause (s)\n|Relay mask\n|\n| ---|---|---|---|---|---|---|\n"
 *       ]
 *     },
 *     {
 *       "key_id": 178,
 *       "key_text": "web_ui_programs_page",
 *       "scope_id": 7,
 *       "scope_text": "p_programs",
 *       "symbol": "P_PROGRAMS_WEB_UI_PROGRAMS_PAGE",
 *       "values": [
 *         "WEB_UI_PROGRAMS_PAGE",
 *         "WEB_UI_PROGRAMS_PAGE",
 *         "WEB_UI_PROGRAMS_PAGE",
 *         "WEB_UI_PROGRAMS_PAGE",
 *         "WEB_UI_PROGRAMS_PAGE"
 *       ]
 *     },
 *     {
 *       "key_id": 13,
 *       "key_text": "aggiorna_dati",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_AGGIORNA_DATI",
 *       "values": [
 *         "🔄 Aggiorna Dati",
 *         "🔄 Update Data",
 *         "🔄 Update Data",
 *         "🔄 Update Data",
 *         "🔄 Update Data"
 *       ]
 *     },
 *     {
 *       "key_id": 33,
 *       "key_text": "i_o_expander",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_I_O_EXPANDER",
 *       "values": [
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander",
 *         "I/O Expander"
 *       ]
 *     },
 *     {
 *       "key_id": 45,
 *       "key_text": "mdb_engine",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_MDB_ENGINE",
 *       "values": [
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine",
 *         "MDB Engine"
 *       ]
 *     },
 *     {
 *       "key_id": 76,
 *       "key_text": "sensore_temperatura",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SENSORE_TEMPERATURA",
 *       "values": [
 *         "Sensore Temperatura",
 *         "Temperature Sensor",
 *         "Temperature Sensor",
 *         "Temperature Sensor",
 *         "Temperature Sensor"
 *       ]
 *     },
 *     {
 *       "key_id": 87,
 *       "key_text": "tabella_programmi",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_TABELLA_PROGRAMMI",
 *       "values": [
 *         "Tabella Programmi",
 *         "Program Table",
 *         "Program Table",
 *         "Program Table",
 *         "Program Table"
 *       ]
 *     },
 *     {
 *       "key_id": 89,
 *       "key_text": "uart_rs232",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_UART_RS232",
 *       "values": [
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232",
 *         "UART RS232"
 *       ]
 *     },
 *     {
 *       "key_id": 90,
 *       "key_text": "uart_rs485",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_UART_RS485",
 *       "values": [
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485",
 *         "UART RS485"
 *       ]
 *     },
 *     {
 *       "key_id": 128,
 *       "key_text": "configurazione",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CONFIGURAZIONE",
 *       "values": [
 *         "Configurazione",
 *         "Configuration",
 *         "Configuration",
 *         "Configuration",
 *         "Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 161,
 *       "key_text": "nome",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_NOME",
 *       "values": [
 *         "Nome",
 *         "Name",
 *         "Name",
 *         "Name",
 *         "Name"
 *       ]
 *     },
 *     {
 *       "key_id": 179,
 *       "key_text": "activity",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_ACTIVITY",
 *       "values": [
 *         "Activity",
 *         "Activity",
 *         "Activity",
 *         "Activity",
 *         "Activity"
 *       ]
 *     },
 *     {
 *       "key_id": 180,
 *       "key_text": "aggiorna_pagina",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_AGGIORNA_PAGINA",
 *       "values": [
 *         "🔄 Aggiorna Pagina",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page"
 *       ]
 *     },
 *     {
 *       "key_id": 181,
 *       "key_text": "aggiungi_task",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_AGGIUNGI_TASK",
 *       "values": [
 *         "➕ Aggiungi Task",
 *         "➕ Add Task",
 *         "➕ Add Task",
 *         "➕ Add Task",
 *         "➕ Add Task"
 *       ]
 *     },
 *     {
 *       "key_id": 182,
 *       "key_text": "ambiente",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_AMBIENTE",
 *       "values": [
 *         "️ Ambiente",
 *         "️ Environment",
 *         "️ Environment",
 *         "️ Environment",
 *         "️ Environment"
 *       ]
 *     },
 *     {
 *       "key_id": 183,
 *       "key_text": "api",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_API",
 *       "values": [
 *         "API",
 *         "API",
 *         "API",
 *         "API",
 *         "API"
 *       ]
 *     },
 *     {
 *       "key_id": 184,
 *       "key_text": "api_endpoints",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_API_ENDPOINTS",
 *       "values": [
 *         "📡 API Endpoints",
 *         "📡 API Endpoints",
 *         "📡 API Endpoints",
 *         "📡 API Endpoints",
 *         "📡 API Endpoints"
 *       ]
 *     },
 *     {
 *       "key_id": 185,
 *       "key_text": "app_last",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_APP_LAST",
 *       "values": [
 *         "APP LAST",
 *         "APP LAST",
 *         "APP LAST",
 *         "APP LAST",
 *         "APP LAST"
 *       ]
 *     },
 *     {
 *       "key_id": 186,
 *       "key_text": "applica",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_APPLICA",
 *       "values": [
 *         "🚀 Applica",
 *         "🚀 Apply",
 *         "🚀 Apply",
 *         "🚀 Apply",
 *         "🚀 Apply"
 *       ]
 *     },
 *     {
 *       "key_id": 187,
 *       "key_text": "apply_tasks",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_APPLY_TASKS",
 *       "values": [
 *         "Apply tasks",
 *         "Apply tasks",
 *         "Apply tasks",
 *         "Apply tasks",
 *         "Apply tasks"
 *       ]
 *     },
 *     {
 *       "key_id": 188,
 *       "key_text": "authenticate_remote",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_AUTHENTICATE_REMOTE",
 *       "values": [
 *         "Authenticate (remote)",
 *         "Authenticate (remote)",
 *         "Authenticate (remote)",
 *         "Authenticate (remote)",
 *         "Authenticate (remote)"
 *       ]
 *     },
 *     {
 *       "key_id": 189,
 *       "key_text": "backup_configuration",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_BACKUP_CONFIGURATION",
 *       "values": [
 *         "Backup configuration",
 *         "Backup configuration",
 *         "Backup configuration",
 *         "Backup configuration",
 *         "Backup configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 190,
 *       "key_text": "benvenuti_nell_interfaccia_di_configurazione_e_test",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_BENVENUTI_NELL_INTERFACCIA_DI_CONFIGURAZIONE_E_TEST",
 *       "values": [
 *         "Benvenuti nell'interfaccia di configurazione e test.",
 *         "Welcome to the configuration and test interface.",
 *         "Welcome to the configuration and test interface.",
 *         "Welcome to the configuration and test interface.",
 *         "Welcome to the configuration and test interface."
 *       ]
 *     },
 *     {
 *       "key_id": 191,
 *       "key_text": "caricamento",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CARICAMENTO",
 *       "values": [
 *         "Caricamento...",
 *         "Loading...",
 *         "Loading...",
 *         "Loading...",
 *         "Loading..."
 *       ]
 *     },
 *     {
 *       "key_id": 192,
 *       "key_text": "configurazione_task",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CONFIGURAZIONE_TASK",
 *       "values": [
 *         "📋 Configurazione Task",
 *         "📋 Task Configuration",
 *         "📋 Task Configuration",
 *         "📋 Task Configuration",
 *         "📋 Task Configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 193,
 *       "key_text": "copia",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_COPIA",
 *       "values": [
 *         "Copia",
 *         "Copy",
 *         "Copy",
 *         "Copy",
 *         "Copy"
 *       ]
 *     },
 *     {
 *       "key_id": 194,
 *       "key_text": "core",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CORE",
 *       "values": [
 *         "Core",
 *         "Core",
 *         "Core",
 *         "Core",
 *         "Core"
 *       ]
 *     },
 *     {
 *       "key_id": 195,
 *       "key_text": "credito_accumulato",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CREDITO_ACCUMULATO",
 *       "values": [
 *         "Credito Accumulato",
 *         "Accumulated Credit",
 *         "Accumulated Credit",
 *         "Accumulated Credit",
 *         "Accumulated Credit"
 *       ]
 *     },
 *     {
 *       "key_id": 196,
 *       "key_text": "current_configuration",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_CURRENT_CONFIGURATION",
 *       "values": [
 *         "Current configuration",
 *         "Current configuration",
 *         "Current configuration",
 *         "Current configuration",
 *         "Current configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 197,
 *       "key_text": "description",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_DESCRIPTION",
 *       "values": [
 *         "Description",
 *         "Description",
 *         "Description",
 *         "Description",
 *         "Description"
 *       ]
 *     },
 *     {
 *       "key_id": 198,
 *       "key_text": "device_status_json",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_DEVICE_STATUS_JSON",
 *       "values": [
 *         "Device status JSON",
 *         "Device status JSON",
 *         "Device status JSON",
 *         "Device status JSON",
 *         "Device status JSON"
 *       ]
 *     },
 *     {
 *       "key_id": 199,
 *       "key_text": "editor_csv",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_EDITOR_CSV",
 *       "values": [
 *         "Editor Task",
 *         "Task Editor",
 *         "Task Editor",
 *         "Task Editor",
 *         "Task Editor"
 *       ]
 *     },
 *     {
 *       "key_id": 200,
 *       "key_text": "emulatore",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_EMULATORE",
 *       "values": [
 *         "Emulatore",
 *         "Emulator",
 *         "Emulator",
 *         "Émulateur",
 *         "Emulador"
 *       ]
 *     },
 *     {
 *       "key_id": 201,
 *       "key_text": "factory",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FACTORY",
 *       "values": [
 *         "FACTORY",
 *         "FACTORY",
 *         "FACTORY",
 *         "FACTORY",
 *         "FACTORY"
 *       ]
 *     },
 *     {
 *       "key_id": 202,
 *       "key_text": "factory_reset",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FACTORY_RESET",
 *       "values": [
 *         "Factory reset",
 *         "Factory reset",
 *         "Factory reset",
 *         "Factory reset",
 *         "Factory reset"
 *       ]
 *     },
 *     {
 *       "key_id": 203,
 *       "key_text": "fetch_firmware",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FETCH_FIRMWARE",
 *       "values": [
 *         "Fetch firmware",
 *         "Fetch firmware",
 *         "Fetch firmware",
 *         "Fetch firmware",
 *         "Fetch firmware"
 *       ]
 *     },
 *     {
 *       "key_id": 204,
 *       "key_text": "fetch_images",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FETCH_IMAGES",
 *       "values": [
 *         "Fetch images",
 *         "Fetch images",
 *         "Fetch images",
 *         "Fetch images",
 *         "Fetch images"
 *       ]
 *     },
 *     {
 *       "key_id": 205,
 *       "key_text": "fetch_translations",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FETCH_TRANSLATIONS",
 *       "values": [
 *         "Fetch translations",
 *         "Fetch translations",
 *         "Fetch translations",
 *         "Fetch translations",
 *         "Fetch translations"
 *       ]
 *     },
 *     {
 *       "key_id": 206,
 *       "key_text": "firmware",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FIRMWARE",
 *       "values": [
 *         "💾 Firmware",
 *         "💾 Firmware",
 *         "💾 Firmware",
 *         "💾 Firmware",
 *         "💾 Firmware"
 *       ]
 *     },
 *     {
 *       "key_id": 207,
 *       "key_text": "force_ntp_sync",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_FORCE_NTP_SYNC",
 *       "values": [
 *         "Force NTP sync",
 *         "Force NTP sync",
 *         "Force NTP sync",
 *         "Force NTP sync",
 *         "Force NTP sync"
 *       ]
 *     },
 *     {
 *       "key_id": 208,
 *       "key_text": "get",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_GET",
 *       "values": [
 *         "GET",
 *         "GET",
 *         "GET",
 *         "GET",
 *         "GET"
 *       ]
 *     },
 *     {
 *       "key_id": 209,
 *       "key_text": "get_customers",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_GET_CUSTOMERS",
 *       "values": [
 *         "Get customers",
 *         "Get customers",
 *         "Get customers",
 *         "Get customers",
 *         "Get customers"
 *       ]
 *     },
 *     {
 *       "key_id": 210,
 *       "key_text": "get_operators",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_GET_OPERATORS",
 *       "values": [
 *         "Get operators",
 *         "Get operators",
 *         "Get operators",
 *         "Get operators",
 *         "Get operators"
 *       ]
 *     },
 *     {
 *       "key_id": 211,
 *       "key_text": "get_remote_config",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_GET_REMOTE_CONFIG",
 *       "values": [
 *         "Get remote config",
 *         "Get remote config",
 *         "Get remote config",
 *         "Get remote config",
 *         "Get remote config"
 *       ]
 *     },
 *     {
 *       "key_id": 212,
 *       "key_text": "gettoniera",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_GETTONIERA",
 *       "values": [
 *         "Gettoniera",
 *         "Coin Mechanism",
 *         "Coin Mechanism",
 *         "Coin Mechanism",
 *         "Coin Mechanism"
 *       ]
 *     },
 *     {
 *       "key_id": 213,
 *       "key_text": "header",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_HEADER",
 *       "values": [
 *         "Header",
 *         "Header",
 *         "Header",
 *         "Header",
 *         "Header"
 *       ]
 *     },
 *     {
 *       "key_id": 214,
 *       "key_text": "http_services",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_HTTP_SERVICES",
 *       "values": [
 *         "HTTP Services",
 *         "HTTP Services",
 *         "HTTP Services",
 *         "HTTP Services",
 *         "HTTP Services"
 *       ]
 *     },
 *     {
 *       "key_id": 215,
 *       "key_text": "indirizzo_ip_ethernet",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_INDIRIZZO_IP_ETHERNET",
 *       "values": [
 *         "Indirizzo IP Ethernet",
 *         "Ethernet IP Address",
 *         "Ethernet IP Address",
 *         "Ethernet IP Address",
 *         "Ethernet IP Address"
 *       ]
 *     },
 *     {
 *       "key_id": 216,
 *       "key_text": "indirizzo_ip_wifi_ap",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_INDIRIZZO_IP_WIFI_AP",
 *       "values": [
 *         "Indirizzo IP WiFi AP",
 *         "WiFi AP IP Address",
 *         "WiFi AP IP Address",
 *         "WiFi AP IP Address",
 *         "WiFi AP IP Address"
 *       ]
 *     },
 *     {
 *       "key_id": 217,
 *       "key_text": "indirizzo_ip_wifi_sta",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_INDIRIZZO_IP_WIFI_STA",
 *       "values": [
 *         "Indirizzo IP WiFi STA",
 *         "WiFi STA IP Address",
 *         "WiFi STA IP Address",
 *         "WiFi STA IP Address",
 *         "WiFi STA IP Address"
 *       ]
 *     },
 *     {
 *       "key_id": 218,
 *       "key_text": "informazioni",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_INFORMAZIONI",
 *       "values": [
 *         "ℹ️ Informazioni",
 *         "ℹ️ Information",
 *         "ℹ️ Information",
 *         "ℹ️ Information",
 *         "ℹ️ Information"
 *       ]
 *     },
 *     {
 *       "key_id": 219,
 *       "key_text": "invia",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_INVIA",
 *       "values": [
 *         "Invia",
 *         "Send",
 *         "Send",
 *         "Send",
 *         "Send"
 *       ]
 *     },
 *     {
 *       "key_id": 220,
 *       "key_text": "is_jwt",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_IS_JWT",
 *       "values": [
 *         "Is JWT",
 *         "Is JWT",
 *         "Is JWT",
 *         "Is JWT",
 *         "Is JWT"
 *       ]
 *     },
 *     {
 *       "key_id": 221,
 *       "key_text": "keepalive",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_KEEPALIVE",
 *       "values": [
 *         "Keepalive",
 *         "Keepalive",
 *         "Keepalive",
 *         "Keepalive",
 *         "Keepalive"
 *       ]
 *     },
 *     {
 *       "key_id": 222,
 *       "key_text": "led_ws2812",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_LED_WS2812",
 *       "values": [
 *         "LED WS2812",
 *         "LED WS2812",
 *         "LED WS2812",
 *         "LED WS2812",
 *         "LED WS2812"
 *       ]
 *     },
 *     {
 *       "key_id": 223,
 *       "key_text": "log_levels",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_LOG_LEVELS",
 *       "values": [
 *         "Log levels",
 *         "Log levels",
 *         "Log levels",
 *         "Log levels",
 *         "Log levels"
 *       ]
 *     },
 *     {
 *       "key_id": 224,
 *       "key_text": "login_chiamate_http",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_LOGIN_CHIAMATE_HTTP",
 *       "values": [
 *         "🔐 Login + Chiamate HTTP",
 *         "🔐 Login + HTTP Calls",
 *         "🔐 Login + HTTP Calls",
 *         "🔐 Login + HTTP Calls",
 *         "🔐 Login + HTTP Calls"
 *       ]
 *     },
 *     {
 *       "key_id": 225,
 *       "key_text": "mdb_status",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_MDB_STATUS",
 *       "values": [
 *         "🎰 MDB Status",
 *         "🎰 MDB Status",
 *         "🎰 MDB Status",
 *         "🎰 MDB Status",
 *         "🎰 MDB Status"
 *       ]
 *     },
 *     {
 *       "key_id": 226,
 *       "key_text": "method",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_METHOD",
 *       "values": [
 *         "Method",
 *         "Method",
 *         "Method",
 *         "Method",
 *         "Method"
 *       ]
 *     },
 *     {
 *       "key_id": 227,
 *       "key_text": "offline_payment",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_OFFLINE_PAYMENT",
 *       "values": [
 *         "Offline payment",
 *         "Offline payment",
 *         "Offline payment",
 *         "Offline payment",
 *         "Offline payment"
 *       ]
 *     },
 *     {
 *       "key_id": 228,
 *       "key_text": "ota0",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_OTA0",
 *       "values": [
 *         "OTA0",
 *         "OTA0",
 *         "OTA0",
 *         "OTA0",
 *         "OTA0"
 *       ]
 *     },
 *     {
 *       "key_id": 229,
 *       "key_text": "ota1",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_OTA1",
 *       "values": [
 *         "OTA1",
 *         "OTA1",
 *         "OTA1",
 *         "OTA1",
 *         "OTA1"
 *       ]
 *     },
 *     {
 *       "key_id": 230,
 *       "key_text": "partizione_al_boot",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PARTIZIONE_AL_BOOT",
 *       "values": [
 *         "Partizione al Boot",
 *         "Partition at Boot",
 *         "Partition at Boot",
 *         "Partition at Boot",
 *         "Partition at Boot"
 *       ]
 *     },
 *     {
 *       "key_id": 231,
 *       "key_text": "partizione_corrente",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PARTIZIONE_CORRENTE",
 *       "values": [
 *         "Partizione Corrente",
 *         "Current Partition",
 *         "Current Partition",
 *         "Current Partition",
 *         "Current Partition"
 *       ]
 *     },
 *     {
 *       "key_id": 232,
 *       "key_text": "password_md5",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PASSWORD_MD5",
 *       "values": [
 *         "Password (MD5):",
 *         "Password (MD5):",
 *         "Password (MD5):",
 *         "Password (MD5):",
 *         "Password (MD5):"
 *       ]
 *     },
 *     {
 *       "key_id": 233,
 *       "key_text": "payload",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PAYLOAD",
 *       "values": [
 *         "Payload",
 *         "Payload",
 *         "Payload",
 *         "Payload",
 *         "Payload"
 *       ]
 *     },
 *     {
 *       "key_id": 234,
 *       "key_text": "payment_request",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PAYMENT_REQUEST",
 *       "values": [
 *         "Payment request",
 *         "Payment request",
 *         "Payment request",
 *         "Payment request",
 *         "Payment request"
 *       ]
 *     },
 *     {
 *       "key_id": 235,
 *       "key_text": "periodo_ms",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PERIODO_MS",
 *       "values": [
 *         "Periodo (ms)",
 *         "Period (ms)",
 *         "Period (ms)",
 *         "Period (ms)",
 *         "Period (ms)"
 *       ]
 *     },
 *     {
 *       "key_id": 236,
 *       "key_text": "post",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST",
 *       "values": [
 *         "POST",
 *         "POST",
 *         "POST",
 *         "POST",
 *         "POST"
 *       ]
 *     },
 *     {
 *       "key_id": 237,
 *       "key_text": "post_api_activity",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_ACTIVITY",
 *       "values": [
 *         "POST /api/activity",
 *         "POST /api/activity",
 *         "POST /api/activity",
 *         "POST /api/activity",
 *         "POST /api/activity"
 *       ]
 *     },
 *     {
 *       "key_id": 238,
 *       "key_text": "post_api_getconfig",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETCONFIG",
 *       "values": [
 *         "POST /api/getconfig",
 *         "POST /api/getconfig",
 *         "POST /api/getconfig",
 *         "POST /api/getconfig",
 *         "POST /api/getconfig"
 *       ]
 *     },
 *     {
 *       "key_id": 239,
 *       "key_text": "post_api_getcustomers",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETCUSTOMERS",
 *       "values": [
 *         "POST /api/getcustomers",
 *         "POST /api/getcustomers",
 *         "POST /api/getcustomers",
 *         "POST /api/getcustomers",
 *         "POST /api/getcustomers"
 *       ]
 *     },
 *     {
 *       "key_id": 240,
 *       "key_text": "post_api_getfirmware",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETFIRMWARE",
 *       "values": [
 *         "POST /api/getfirmware",
 *         "POST /api/getfirmware",
 *         "POST /api/getfirmware",
 *         "POST /api/getfirmware",
 *         "POST /api/getfirmware"
 *       ]
 *     },
 *     {
 *       "key_id": 241,
 *       "key_text": "post_api_getimages",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETIMAGES",
 *       "values": [
 *         "POST /api/getimages",
 *         "POST /api/getimages",
 *         "POST /api/getimages",
 *         "POST /api/getimages",
 *         "POST /api/getimages"
 *       ]
 *     },
 *     {
 *       "key_id": 242,
 *       "key_text": "post_api_getoperators",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETOPERATORS",
 *       "values": [
 *         "POST /api/getoperators",
 *         "POST /api/getoperators",
 *         "POST /api/getoperators",
 *         "POST /api/getoperators",
 *         "POST /api/getoperators"
 *       ]
 *     },
 *     {
 *       "key_id": 243,
 *       "key_text": "post_api_gettranslations",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_GETTRANSLATIONS",
 *       "values": [
 *         "POST /api/gettranslations",
 *         "POST /api/gettranslations",
 *         "POST /api/gettranslations",
 *         "POST /api/gettranslations",
 *         "POST /api/gettranslations"
 *       ]
 *     },
 *     {
 *       "key_id": 244,
 *       "key_text": "post_api_keepalive",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_KEEPALIVE",
 *       "values": [
 *         "POST /api/keepalive",
 *         "POST /api/keepalive",
 *         "POST /api/keepalive",
 *         "POST /api/keepalive",
 *         "POST /api/keepalive"
 *       ]
 *     },
 *     {
 *       "key_id": 245,
 *       "key_text": "post_api_payment",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_PAYMENT",
 *       "values": [
 *         "POST /api/payment",
 *         "POST /api/payment",
 *         "POST /api/payment",
 *         "POST /api/payment",
 *         "POST /api/payment"
 *       ]
 *     },
 *     {
 *       "key_id": 246,
 *       "key_text": "post_api_paymentoffline",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_PAYMENTOFFLINE",
 *       "values": [
 *         "POST /api/paymentoffline",
 *         "POST /api/paymentoffline",
 *         "POST /api/paymentoffline",
 *         "POST /api/paymentoffline",
 *         "POST /api/paymentoffline"
 *       ]
 *     },
 *     {
 *       "key_id": 247,
 *       "key_text": "post_api_serviceused",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_POST_API_SERVICEUSED",
 *       "values": [
 *         "POST /api/serviceused",
 *         "POST /api/serviceused",
 *         "POST /api/serviceused",
 *         "POST /api/serviceused",
 *         "POST /api/serviceused"
 *       ]
 *     },
 *     {
 *       "key_id": 248,
 *       "key_text": "priorita",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PRIORITA",
 *       "values": [
 *         "Priorità",
 *         "Priority",
 *         "Priority",
 *         "Priority",
 *         "Priority"
 *       ]
 *     },
 *     {
 *       "key_id": 249,
 *       "key_text": "profilo",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PROFILO",
 *       "values": [
 *         "Profilo:",
 *         "Profile:",
 *         "Profile:",
 *         "Profile:",
 *         "Profile:"
 *       ]
 *     },
 *     {
 *       "key_id": 250,
 *       "key_text": "pwm_channel_1_2",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_PWM_CHANNEL_1_2",
 *       "values": [
 *         "PWM Channel 1/2",
 *         "PWM Channel 1/2",
 *         "PWM Channel 1/2",
 *         "PWM Channel 1/2",
 *         "PWM Channel 1/2"
 *       ]
 *     },
 *     {
 *       "key_id": 251,
 *       "key_text": "reboot",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_REBOOT",
 *       "values": [
 *         "Reboot",
 *         "Reboot",
 *         "Reboot",
 *         "Reboot",
 *         "Reboot"
 *       ]
 *     },
 *     {
 *       "key_id": 252,
 *       "key_text": "restart_usb_host",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RESTART_USB_HOST",
 *       "values": [
 *         "Restart USB host",
 *         "Restart USB host",
 *         "Restart USB host",
 *         "Restart USB host",
 *         "Restart USB host"
 *       ]
 *     },
 *     {
 *       "key_id": 253,
 *       "key_text": "rete",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RETE",
 *       "values": [
 *         "🌐 Rete",
 *         "🌐 Network",
 *         "🌐 Network",
 *         "🌐 Network",
 *         "🌐 Network"
 *       ]
 *     },
 *     {
 *       "key_id": 254,
 *       "key_text": "richiesta",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RICHIESTA",
 *       "values": [
 *         "Richiesta",
 *         "Request",
 *         "Request",
 *         "Request",
 *         "Request"
 *       ]
 *     },
 *     {
 *       "key_id": 255,
 *       "key_text": "rimuovi",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RIMUOVI",
 *       "values": [
 *         "Rimuovi",
 *         "Remove",
 *         "Remove",
 *         "Remove",
 *         "Remove"
 *       ]
 *     },
 *     {
 *       "key_id": 256,
 *       "key_text": "risposta",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RISPOSTA",
 *       "values": [
 *         "Risposta",
 *         "Response",
 *         "Response",
 *         "Response",
 *         "Response"
 *       ]
 *     },
 *     {
 *       "key_id": 257,
 *       "key_text": "run_internal_tests",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_RUN_INTERNAL_TESTS",
 *       "values": [
 *         "Run internal tests",
 *         "Run internal tests",
 *         "Run internal tests",
 *         "Run internal tests",
 *         "Run internal tests"
 *       ]
 *     },
 *     {
 *       "key_id": 258,
 *       "key_text": "salva",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SALVA",
 *       "values": [
 *         "💾 Salva",
 *         "💾 Save",
 *         "💾 Save",
 *         "💾 Save",
 *         "💾 Save"
 *       ]
 *     },
 *     {
 *       "key_id": 259,
 *       "key_text": "save_configuration",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SAVE_CONFIGURATION",
 *       "values": [
 *         "Save configuration",
 *         "Save configuration",
 *         "Save configuration",
 *         "Save configuration",
 *         "Save configuration"
 *       ]
 *     },
 *     {
 *       "key_id": 260,
 *       "key_text": "save_tasks",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SAVE_TASKS",
 *       "values": [
 *         "Save tasks",
 *         "Save tasks",
 *         "Save tasks",
 *         "Save tasks",
 *         "Save tasks"
 *       ]
 *     },
 *     {
 *       "key_id": 261,
 *       "key_text": "sd_card",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SD_CARD",
 *       "values": [
 *         "SD Card",
 *         "SD Card",
 *         "SD Card",
 *         "SD Card",
 *         "SD Card"
 *       ]
 *     },
 *     {
 *       "key_id": 262,
 *       "key_text": "serial",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SERIAL",
 *       "values": [
 *         "Serial:",
 *         "Serial:",
 *         "Serial:",
 *         "Serial:",
 *         "Serial:"
 *       ]
 *     },
 *     {
 *       "key_id": 263,
 *       "key_text": "service_used",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SERVICE_USED",
 *       "values": [
 *         "Service used",
 *         "Service used",
 *         "Service used",
 *         "Service used",
 *         "Service used"
 *       ]
 *     },
 *     {
 *       "key_id": 264,
 *       "key_text": "set_log_level",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SET_LOG_LEVEL",
 *       "values": [
 *         "Set log level",
 *         "Set log level",
 *         "Set log level",
 *         "Set log level",
 *         "Set log level"
 *       ]
 *     },
 *     {
 *       "key_id": 265,
 *       "key_text": "spazio_totale",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SPAZIO_TOTALE",
 *       "values": [
 *         "Spazio Totale",
 *         "Total Space",
 *         "Total Space",
 *         "Total Space",
 *         "Total Space"
 *       ]
 *     },
 *     {
 *       "key_id": 266,
 *       "key_text": "spazio_usato",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_SPAZIO_USATO",
 *       "values": [
 *         "Spazio Usato",
 *         "Used Space",
 *         "Used Space",
 *         "Used Space",
 *         "Used Space"
 *       ]
 *     },
 *     {
 *       "key_id": 267,
 *       "key_text": "stack_words",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STACK_WORDS",
 *       "values": [
 *         "Stack bytess",
 *         "Stack bytes",
 *         "Stack bytes",
 *         "Stack bytes",
 *         "Stack bytes"
 *       ]
 *     },
 *     {
 *       "key_id": 268,
 *       "key_text": "statistiche",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STATISTICHE",
 *       "values": [
 *         "Statistiche",
 *         "Statistics",
 *         "Statistiken",
 *         "Statistiques",
 *         "Estadísticas"
 *       ]
 *     },
 *     {
 *       "key_id": 269,
 *       "key_text": "stato",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STATO",
 *       "values": [
 *         "Stato",
 *         "Status",
 *         "Status",
 *         "Status",
 *         "Status"
 *       ]
 *     },
 *     {
 *       "key_id": 270,
 *       "key_text": "stato_driver",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STATO_DRIVER",
 *       "values": [
 *         "🔌 Stato Driver",
 *         "🔌 Driver Status",
 *         "🔌 Driver Status",
 *         "🔌 Driver Status",
 *         "🔌 Driver Status"
 *       ]
 *     },
 *     {
 *       "key_id": 271,
 *       "key_text": "stato_logico_sm",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STATO_LOGICO_SM",
 *       "values": [
 *         "Stato Logico (SM)",
 *         "Logical State (SM)",
 *         "Logical State (SM)",
 *         "Logical State (SM)",
 *         "Logical State (SM)"
 *       ]
 *     },
 *     {
 *       "key_id": 272,
 *       "key_text": "stored_logs",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_STORED_LOGS",
 *       "values": [
 *         "Stored logs",
 *         "Stored logs",
 *         "Stored logs",
 *         "Stored logs",
 *         "Stored logs"
 *       ]
 *     },
 *     {
 *       "key_id": 273,
 *       "key_text": "tasks_csv",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_TASKS_CSV",
 *       "values": [
 *         "Task",
 *         "Tasks",
 *         "Aufgaben",
 *         "Tâches",
 *         "Tareas"
 *       ]
 *     },
 *     {
 *       "key_id": 274,
 *       "key_text": "temperatura",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_TEMPERATURA",
 *       "values": [
 *         "Temperatura",
 *         "Temperature",
 *         "Temperature",
 *         "Temperature",
 *         "Temperature"
 *       ]
 *     },
 *     {
 *       "key_id": 275,
 *       "key_text": "test_hardware",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_TEST_HARDWARE",
 *       "values": [
 *         "Test Hardware",
 *         "Hardware Test",
 *         "Hardware Test",
 *         "Hardware Test",
 *         "Hardware Test"
 *       ]
 *     },
 *     {
 *       "key_id": 276,
 *       "key_text": "token",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_TOKEN",
 *       "values": [
 *         "Token:",
 *         "Token:",
 *         "Token:",
 *         "Token:",
 *         "Token:"
 *       ]
 *     },
 *     {
 *       "key_id": 277,
 *       "key_text": "umidita",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_UMIDITA",
 *       "values": [
 *         "Umidità",
 *         "Humidity",
 *         "Humidity",
 *         "Humidity",
 *         "Humidity"
 *       ]
 *     },
 *     {
 *       "key_id": 278,
 *       "key_text": "update_ota",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_UPDATE_OTA",
 *       "values": [
 *         "Update OTA",
 *         "Update OTA",
 *         "Update OTA",
 *         "Update OTA",
 *         "Update OTA"
 *       ]
 *     },
 *     {
 *       "key_id": 279,
 *       "key_text": "uri",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_URI",
 *       "values": [
 *         "URI",
 *         "URI",
 *         "URI",
 *         "URI",
 *         "URI"
 *       ]
 *     },
 *     {
 *       "key_id": 280,
 *       "key_text": "usb_enumerate",
 *       "scope_id": 8,
 *       "scope_text": "p_runtime",
 *       "symbol": "P_RUNTIME_USB_ENUMERATE",
 *       "values": [
 *         "USB enumerate",
 *         "USB enumerate",
 *         "USB enumerate",
 *         "USB enumerate",
 *         "USB enumerate"
 *       ]
 *     },
 *     {
 *       "key_id": 12,
 *       "key_text": "aggiorna",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_AGGIORNA",
 *       "values": [
 *         "🔄 Aggiorna",
 *         "🔄 Update",
 *         "🔄 Update",
 *         "🔄 Update",
 *         "🔄 Update"
 *       ]
 *     },
 *     {
 *       "key_id": 33,
 *       "key_text": "i_o_expander",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_I_O_EXPANDER",
 *       "values": [
 *         "🔌 I/O Expander",
 *         "🔌 I/O Expander",
 *         "🔌 I/O Expander",
 *         "🔌 I/O Expander",
 *         "🔌 I/O Expander"
 *       ]
 *     },
 *     {
 *       "key_id": 148,
 *       "key_text": "div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV",
 *       "values": [
 *         "</div>",
 *         " ",
 *         " ",
 *         " ",
 *         " "
 *       ]
 *     },
 *     {
 *       "key_id": 150,
 *       "key_text": "div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_DIV",
 *       "values": [
 *         "</div></div>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 176,
 *       "key_text": "script_body_html",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCRIPT_BODY_HTML",
 *       "values": [
 *         "</script></body></html>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 180,
 *       "key_text": "aggiorna_pagina",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_AGGIORNA_PAGINA",
 *       "values": [
 *         "🔄 Aggiorna Pagina",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page",
 *         "🔄 Refresh Page"
 *       ]
 *     },
 *     {
 *       "key_id": 186,
 *       "key_text": "applica",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_APPLICA",
 *       "values": [
 *         "🚀 Applica",
 *         "🚀 Apply",
 *         "🚀 Apply",
 *         "🚀 Apply",
 *         "🚀 Apply"
 *       ]
 *     },
 *     {
 *       "key_id": 191,
 *       "key_text": "caricamento",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CARICAMENTO",
 *       "values": [
 *         "Caricamento...",
 *         "Loading...",
 *         "Loading...",
 *         "Loading...",
 *         "Loading..."
 *       ]
 *     },
 *     {
 *       "key_id": 219,
 *       "key_text": "invia",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIA",
 *       "values": [
 *         "🚀 Invia",
 *         "🚀 Send",
 *         "🚀 Send",
 *         "🚀 Send",
 *         "🚀 Send"
 *       ]
 *     },
 *     {
 *       "key_id": 275,
 *       "key_text": "test_hardware",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEST_HARDWARE",
 *       "values": [
 *         "Test Hardware",
 *         "Hardware Test",
 *         "Hardware Test",
 *         "Hardware Test",
 *         "Hardware Test"
 *       ]
 *     },
 *     {
 *       "key_id": 281,
 *       "key_text": "4800_baud_pin_21_rx_20_tx",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_4800_BAUD_PIN_21_RX_20_TX",
 *       "values": [
 *         "(4800 baud — PIN 21 RX, 20 TX)",
 *         "(4800 baud — PIN 21 RX, 20 TX)",
 *         "(4800 baud — PIN 21 RX, 20 TX)",
 *         "(4800 baud — PIN 21 RX, 20 TX)",
 *         "(4800 baud — PIN 21 RX, 20 TX)"
 *       ]
 *     },
 *     {
 *       "key_id": 282,
 *       "key_text": "avvia_aggiornamenti_periodici_per_la_sezione_scanner",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_AVVIA_AGGIORNAMENTI_PERIODICI_PER_LA_SEZIONE_SCANNER",
 *       "values": [
 *         "// avvia aggiornamenti periodici per la sezione Scanner",
 *         "// start periodic updates for the Scanner section",
 *         "// start periodic updates for the Scanner section",
 *         "// start periodic updates for the Scanner section",
 *         "// start periodic updates for the Scanner section"
 *       ]
 *     },
 *     {
 *       "key_id": 283,
 *       "key_text": "backup",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BACKUP",
 *       "values": [
 *         "📥 Backup",
 *         "📥 Backup",
 *         "📥 Backup",
 *         "📥 Backup",
 *         "📥 Backup"
 *       ]
 *     },
 *     {
 *       "key_id": 284,
 *       "key_text": "backup_json_config",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BACKUP_JSON_CONFIG",
 *       "values": [
 *         "Backup JSON Config",
 *         "Backup JSON Config",
 *         "Backup JSON Config",
 *         "Backup JSON Config",
 *         "Backup JSON Config"
 *       ]
 *     },
 *     {
 *       "key_id": 285,
 *       "key_text": "blink_tutte_le_uscite_1hz",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BLINK_TUTTE_LE_USCITE_1HZ",
 *       "values": [
 *         "Blink Tutte le Uscite (1Hz)",
 *         "Blink All Outputs (1Hz)",
 *         "Blink All Outputs (1Hz)",
 *         "Blink All Outputs (1Hz)",
 *         "Blink All Outputs (1Hz)"
 *       ]
 *     },
 *     {
 *       "key_id": 286,
 *       "key_text": "button_id_btn_led_manual_onclick_toggleledmanual_style_backgroun",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ID_BTN_LED_MANUAL_ONCLICK_TOGGLELEDMANUAL_STYLE_BACKGROUN",
 *       "values": [
 *         "<button id='btn_led_manual' onclick=\"toggleLedManual()\" style='background:#2ecc71'>🚀 Inizia Manuale</button>",
 *         " <button id='btn_led_manual' onclick=\"toggleLedManual()\" style='background:#2ecc71'>🚀 Start Manual",
 *         " <button id='btn_led_manual' onclick=\"toggleLedManual()\" style='background:#2ecc71'>🚀 Start Manual",
 *         " <button id='btn_led_manual' onclick=\"toggleLedManual()\" style='background:#2ecc71'>🚀 Start Manual",
 *         " <button id='btn_led_manual' onclick=\"toggleLedManual()\" style='background:#2ecc71'>🚀 Start Manual"
 *       ]
 *     },
 *     {
 *       "key_id": 287,
 *       "key_text": "button_onclick_refreshsdstatus_style_background_3498db_aggiorna_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_REFRESHSDSTATUS_STYLE_BACKGROUND_3498DB_AGGIORNA",
 *       "values": [
 *         "<button onclick='refreshSDStatus()' style='background:#3498db'>🔄 Aggiorna</button>",
 *         "\n🔄 Update\n",
 *         "\n🔄 Update\n",
 *         "\n🔄 Update\n",
 *         "\n🔄 Update\n"
 *       ]
 *     },
 *     {
 *       "key_id": 288,
 *       "key_text": "button_onclick_runtest_sd_init_style_background_27ae60_init_butt",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_RUNTEST_SD_INIT_STYLE_BACKGROUND_27AE60_INIT_BUTT",
 *       "values": [
 *         "<button onclick=\"runTest('sd_init')\" style='background:#27ae60'>⚡ Init</button>",
 *         " <button onclick=\"runTest('sd_init')\" style='background:#27ae60'>⚡ Init",
 *         " <button onclick=\"runTest('sd_init')\" style='background:#27ae60'>⚡ Init",
 *         " <button onclick=\"runTest('sd_init')\" style='background:#27ae60'>⚡ Init",
 *         " <button onclick=\"runTest('sd_init')\" style='background:#27ae60'>⚡ Init"
 *       ]
 *     },
 *     {
 *       "key_id": 289,
 *       "key_text": "button_onclick_runtest_sht_read_style_background_f39c12_forza_te",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_RUNTEST_SHT_READ_STYLE_BACKGROUND_F39C12_FORZA_TE",
 *       "values": [
 *         "<button onclick=\"runTest('sht_read')\" style='background:#f39c12'>🔄 FORZA TEST LETTURA</button>",
 *         " <button onclick=\"runTest('sht_read')\" style='background:#f39c12'>🔄 FORCE READ TEST",
 *         " <button onclick=\"runTest('sht_read')\" style='background:#f39c12'>🔄 FORCE READ TEST",
 *         " <button onclick=\"runTest('sht_read')\" style='background:#f39c12'>🔄 FORCE READ TEST",
 *         " <button onclick=\"runTest('sht_read')\" style='background:#f39c12'>🔄 FORCE READ TEST"
 *       ]
 *     },
 *     {
 *       "key_id": 290,
 *       "key_text": "button_onclick_setpwm_style_background_2980b9_applica_button",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_SETPWM_STYLE_BACKGROUND_2980B9_APPLICA_BUTTON",
 *       "values": [
 *         "<button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Applica</button>",
 *         " <button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Apply",
 *         " <button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Apply",
 *         " <button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Apply",
 *         " <button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Apply"
 *       ]
 *     },
 *     {
 *       "key_id": 291,
 *       "key_text": "button_onclick_testeeprom_read_json_style_background_9b59b6_legg",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_TESTEEPROM_READ_JSON_STYLE_BACKGROUND_9B59B6_LEGG",
 *       "values": [
 *         "<button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Leggi JSON</button>",
 *         " <button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Read JSON",
 *         " <button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Read JSON",
 *         " <button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Read JSON",
 *         " <button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Read JSON"
 *       ]
 *     },
 *     {
 *       "key_id": 292,
 *       "key_text": "button_onclick_testeeprom_read_style_background_3498db_leggi_but",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_TESTEEPROM_READ_STYLE_BACKGROUND_3498DB_LEGGI_BUT",
 *       "values": [
 *         "<button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Leggi</button>",
 *         " <button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Read",
 *         " <button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Read",
 *         " <button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Read",
 *         " <button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Read"
 *       ]
 *     },
 *     {
 *       "key_id": 293,
 *       "key_text": "button_onclick_testeeprom_write_scrivi_button",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_ONCLICK_TESTEEPROM_WRITE_SCRIVI_BUTTON",
 *       "values": [
 *         "<button onclick=\"testEEPROM('write')\">✍️ Scrivi</button>",
 *         " <button onclick=\"testEEPROM('write')\">✍️ Write",
 *         " <button onclick=\"testEEPROM('write')\">✍️ Write",
 *         " <button onclick=\"testEEPROM('write')\">✍️ Write",
 *         " <button onclick=\"testEEPROM('write')\">✍️ Write"
 *       ]
 *     },
 *     {
 *       "key_id": 294,
 *       "key_text": "button_type_button_onclick_runscannercommand_scanner_off_class_b",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_OFF_CLASS_B",
 *       "values": [
 *         "<button type='button' onclick=\"runScannerCommand('scanner_off')\" class='btn-stop'>Scanner OFF</button>",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_off')\" class='btn-stop'>Scanner OFF",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_off')\" class='btn-stop'>Scanner OFF",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_off')\" class='btn-stop'>Scanner OFF",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_off')\" class='btn-stop'>Scanner OFF"
 *       ]
 *     },
 *     {
 *       "key_id": 295,
 *       "key_text": "button_type_button_onclick_runscannercommand_scanner_on_style_ba",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_ON_STYLE_BA",
 *       "values": [
 *         "<button type='button' onclick=\"runScannerCommand('scanner_on')\" style='background:#2ecc71'>Scanner ON</button>",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_on')\" style='background:#2ecc71'>Scanner ON",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_on')\" style='background:#2ecc71'>Scanner ON",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_on')\" style='background:#2ecc71'>Scanner ON",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_on')\" style='background:#2ecc71'>Scanner ON"
 *       ]
 *     },
 *     {
 *       "key_id": 296,
 *       "key_text": "button_type_button_onclick_runscannercommand_scanner_setup_style",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_SETUP_STYLE",
 *       "values": [
 *         "<button type='button' onclick=\"runScannerCommand('scanner_setup')\" style='background:#8e44ad'>Scanner SETUP</button>",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_setup')\" style='background:#8e44ad'>Scanner SETUP",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_setup')\" style='background:#8e44ad'>Scanner SETUP",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_setup')\" style='background:#8e44ad'>Scanner SETUP",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_setup')\" style='background:#8e44ad'>Scanner SETUP"
 *       ]
 *     },
 *     {
 *       "key_id": 297,
 *       "key_text": "button_type_button_onclick_runscannercommand_scanner_state_style",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_BUTTON_TYPE_BUTTON_ONCLICK_RUNSCANNERCOMMAND_SCANNER_STATE_STYLE",
 *       "values": [
 *         "<button type='button' onclick=\"runScannerCommand('scanner_state')\" style='background:#3498db'>Scanner STATE</button>",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_state')\" style='background:#3498db'>Scanner STATE",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_state')\" style='background:#3498db'>Scanner STATE",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_state')\" style='background:#3498db'>Scanner STATE",
 *         " <button type='button' onclick=\"runScannerCommand('scanner_state')\" style='background:#3498db'>Scanner STATE"
 *       ]
 *     },
 *     {
 *       "key_id": 298,
 *       "key_text": "c",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_C",
 *       "values": [
 *         "-- °C / -- %",
 *         "-- °C / -- %",
 *         "-- °C / -- %",
 *         "-- °C / -- %",
 *         "-- °C / -- %"
 *       ]
 *     },
 *     {
 *       "key_id": 299,
 *       "key_text": "c_get_test",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_C_GET_TEST",
 *       "values": [
 *         "[C] GET /test",
 *         "[C] GET /test",
 *         "[C] GET /test",
 *         "[C] GET /test",
 *         "[C] GET /test"
 *       ]
 *     },
 *     {
 *       "key_id": 300,
 *       "key_text": "canale",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CANALE",
 *       "values": [
 *         "Canale:",
 *         "Channel:",
 *         "Channel:",
 *         "Channel:",
 *         "Channel:"
 *       ]
 *     },
 *     {
 *       "key_id": 301,
 *       "key_text": "cctalk",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CCTALK",
 *       "values": [
 *         "📡 CCtalk",
 *         "📡 CCtalk",
 *         "📡 CCtalk",
 *         "📡 CCtalk",
 *         "📡 CCtalk"
 *       ]
 *     },
 *     {
 *       "key_id": 302,
 *       "key_text": "clear",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CLEAR",
 *       "values": [
 *         "🗑️ Clear",
 *         "🗑️ Clear",
 *         "🗑️ Clear",
 *         "🗑️ Clear",
 *         "🗑️ Clear"
 *       ]
 *     },
 *     {
 *       "key_id": 303,
 *       "key_text": "collapsible_sections_support",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_COLLAPSIBLE_SECTIONS_SUPPORT",
 *       "values": [
 *         "/* Collapsible sections support */",
 *         "/* Collapsible sections support \n/ ",
 *         "/* Collapsible sections support \n/ ",
 *         "/* Collapsible sections support \n/ ",
 *         "/* Collapsible sections support \n/ "
 *       ]
 *     },
 *     {
 *       "key_id": 304,
 *       "key_text": "config_json",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CONFIG_JSON",
 *       "values": [
 *         "📄 Config JSON:",
 *         "📄 Config JSON:",
 *         "📄 Config JSON:",
 *         "📄 Config JSON:",
 *         "📄 Config JSON:"
 *       ]
 *     },
 *     {
 *       "key_id": 305,
 *       "key_text": "controllo_manuale",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CONTROLLO_MANUALE",
 *       "values": [
 *         "Controllo Manuale",
 *         "Manual Control",
 *         "Manual Control",
 *         "Manual Control",
 *         "Manual Control"
 *       ]
 *     },
 *     {
 *       "key_id": 306,
 *       "key_text": "controllo_manuale_pwm",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_CONTROLLO_MANUALE_PWM",
 *       "values": [
 *         "Controllo Manuale PWM",
 *         "Manual PWM Control",
 *         "Manual PWM Control",
 *         "Manual PWM Control",
 *         "Manual PWM Control"
 *       ]
 *     },
 *     {
 *       "key_id": 307,
 *       "key_text": "copiato",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_COPIATO",
 *       "values": [
 *         "📋 Copiato",
 *         "📋 Copied",
 *         "📋 Copied",
 *         "📋 Copied",
 *         "📋 Copied"
 *       ]
 *     },
 *     {
 *       "key_id": 308,
 *       "key_text": "dato_byte_0_255",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DATO_BYTE_0_255",
 *       "values": [
 *         "Dato Byte (0-255)",
 *         "Byte Data (0-255)",
 *         "Byte Data (0-255)",
 *         "Byte Data (0-255)",
 *         "Byte Data (0-255)"
 *       ]
 *     },
 *     {
 *       "key_id": 309,
 *       "key_text": "display_brightness_slider_handler_debounced_persist_after_idle",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DISPLAY_BRIGHTNESS_SLIDER_HANDLER_DEBOUNCED_PERSIST_AFTER_IDLE",
 *       "values": [
 *         "// display brightness slider handler (debounced + persist after idle)",
 *         "// display brightness slider handler (debounced + persist after idle)",
 *         "// display brightness slider handler (debounced + persist after idle)",
 *         "// display brightness slider handler (debounced + persist after idle)",
 *         "// display brightness slider handler (debounced + persist after idle)"
 *       ]
 *     },
 *     {
 *       "key_id": 310,
 *       "key_text": "div_class_container",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_CONTAINER",
 *       "values": [
 *         "<div class='container'>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 311,
 *       "key_text": "div_class_test_controls",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS",
 *       "values": [
 *         "<div class='test-controls'>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 312,
 *       "key_text": "div_class_test_controls_button_id_btn_cctalk_onclick_togglesimpl",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_CCTALK_ONCLICK_TOGGLESIMPL",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_cctalk' onclick=\"toggleSimpleTest('cctalk', 'btn_cctalk', 'Test CCtalk')\">▶️ Inizia Test</button></div></div>",
 *         "\n<button id='btn_cctalk' onclick=\"toggleSimpleTest('cctalk', 'btn_cctalk', 'CCtalk Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_cctalk' onclick=\"toggleSimpleTest('cctalk', 'btn_cctalk', 'CCtalk Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_cctalk' onclick=\"toggleSimpleTest('cctalk', 'btn_cctalk', 'CCtalk Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_cctalk' onclick=\"toggleSimpleTest('cctalk', 'btn_cctalk', 'CCtalk Test')\">▶️ Start Test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 313,
 *       "key_text": "div_class_test_controls_button_id_btn_ioexp_onclick_togglesimple",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_IOEXP_ONCLICK_TOGGLESIMPLE",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_ioexp' onclick=\"toggleSimpleTest('ioexp', 'btn_ioexp', 'Blink')\">▶️ Inizia Blink</button></div></div>",
 *         "\n<button id='btn_ioexp' onclick=\"toggleSimpleTest('ioexp', 'btn_ioexp', 'Blink')\">▶️ Start Blink\n",
 *         "\n<button id='btn_ioexp' onclick=\"toggleSimpleTest('ioexp', 'btn_ioexp', 'Blink')\">▶️ Start Blink\n",
 *         "\n<button id='btn_ioexp' onclick=\"toggleSimpleTest('ioexp', 'btn_ioexp', 'Blink')\">▶️ Start Blink\n",
 *         "\n<button id='btn_ioexp' onclick=\"toggleSimpleTest('ioexp', 'btn_ioexp', 'Blink')\">▶️ Start Blink\n"
 *       ]
 *     },
 *     {
 *       "key_id": 314,
 *       "key_text": "div_class_test_controls_button_id_btn_led_rainbow_onclick_toggle",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_LED_RAINBOW_ONCLICK_TOGGLE",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_led_rainbow' onclick=\"toggleLedRainbow()\">▶️ Inizia Rainbow</button></div></div>",
 *         "\n<button id='btn_led_rainbow' onclick=\"toggleLedRainbow()\">▶️ Start Rainbow\n",
 *         "\n<button id='btn_led_rainbow' onclick=\"toggleLedRainbow()\">▶️ Start Rainbow\n",
 *         "\n<button id='btn_led_rainbow' onclick=\"toggleLedRainbow()\">▶️ Start Rainbow\n",
 *         "\n<button id='btn_led_rainbow' onclick=\"toggleLedRainbow()\">▶️ Start Rainbow\n"
 *       ]
 *     },
 *     {
 *       "key_id": 315,
 *       "key_text": "div_class_test_controls_button_id_btn_mdb_onclick_togglesimplete",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_MDB_ONCLICK_TOGGLESIMPLETE",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_mdb' onclick=\"toggleSimpleTest('mdb', 'btn_mdb', 'Test MDB')\">▶️ Inizia Test</button></div></div>",
 *         "\n<button id='btn_mdb' onclick=\"toggleSimpleTest('mdb', 'btn_mdb', 'MDB Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_mdb' onclick=\"toggleSimpleTest('mdb', 'btn_mdb', 'MDB Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_mdb' onclick=\"toggleSimpleTest('mdb', 'btn_mdb', 'MDB Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_mdb' onclick=\"toggleSimpleTest('mdb', 'btn_mdb', 'MDB Test')\">▶️ Start Test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 316,
 *       "key_text": "div_class_test_controls_button_id_btn_pwm1_onclick_togglesimplet",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_PWM1_ONCLICK_TOGGLESIMPLET",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_pwm1' onclick=\"toggleSimpleTest('pwm1', 'btn_pwm1', 'Sweep PWM1')\">▶️ Inizia Sweep</button></div></div>",
 *         "\n<button id='btn_pwm1' onclick=\"toggleSimpleTest('pwm1', 'btn_pwm1', 'Sweep PWM1')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm1' onclick=\"toggleSimpleTest('pwm1', 'btn_pwm1', 'Sweep PWM1')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm1' onclick=\"toggleSimpleTest('pwm1', 'btn_pwm1', 'Sweep PWM1')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm1' onclick=\"toggleSimpleTest('pwm1', 'btn_pwm1', 'Sweep PWM1')\">▶️ Start Sweep\n"
 *       ]
 *     },
 *     {
 *       "key_id": 317,
 *       "key_text": "div_class_test_controls_button_id_btn_pwm2_onclick_togglesimplet",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_PWM2_ONCLICK_TOGGLESIMPLET",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_pwm2' onclick=\"toggleSimpleTest('pwm2', 'btn_pwm2', 'Sweep PWM2')\">▶️ Inizia Sweep</button></div></div>",
 *         "\n<button id='btn_pwm2' onclick=\"toggleSimpleTest('pwm2', 'btn_pwm2', 'Sweep PWM2')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm2' onclick=\"toggleSimpleTest('pwm2', 'btn_pwm2', 'Sweep PWM2')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm2' onclick=\"toggleSimpleTest('pwm2', 'btn_pwm2', 'Sweep PWM2')\">▶️ Start Sweep\n",
 *         "\n<button id='btn_pwm2' onclick=\"toggleSimpleTest('pwm2', 'btn_pwm2', 'Sweep PWM2')\">▶️ Start Sweep\n"
 *       ]
 *     },
 *     {
 *       "key_id": 318,
 *       "key_text": "div_class_test_controls_button_id_btn_rs232_onclick_togglesimple",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_RS232_ONCLICK_TOGGLESIMPLE",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_rs232' onclick=\"toggleSimpleTest('rs232', 'btn_rs232', 'Test RS232')\">▶️ Inizia Test</button></div></div>",
 *         "\n<button id='btn_rs232' onclick=\"toggleSimpleTest('rs232', 'btn_rs232', 'RS232 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs232' onclick=\"toggleSimpleTest('rs232', 'btn_rs232', 'RS232 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs232' onclick=\"toggleSimpleTest('rs232', 'btn_rs232', 'RS232 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs232' onclick=\"toggleSimpleTest('rs232', 'btn_rs232', 'RS232 Test')\">▶️ Start Test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 319,
 *       "key_text": "div_class_test_controls_button_id_btn_rs485_onclick_togglesimple",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ID_BTN_RS485_ONCLICK_TOGGLESIMPLE",
 *       "values": [
 *         "<div class='test-controls'><button id='btn_rs485' onclick=\"toggleSimpleTest('rs485', 'btn_rs485', 'Test RS485')\">▶️ Inizia Test</button></div></div>",
 *         "\n<button id='btn_rs485' onclick=\"toggleSimpleTest('rs485', 'btn_rs485', 'RS485 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs485' onclick=\"toggleSimpleTest('rs485', 'btn_rs485', 'RS485 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs485' onclick=\"toggleSimpleTest('rs485', 'btn_rs485', 'RS485 Test')\">▶️ Start Test\n",
 *         "\n<button id='btn_rs485' onclick=\"toggleSimpleTest('rs485', 'btn_rs485', 'RS485 Test')\">▶️ Start Test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 320,
 *       "key_text": "div_class_test_controls_button_onclick_if_confirm_cancellare_tut",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_IF_CONFIRM_CANCELLARE_TUT",
 *       "values": [
 *         "<div class='test-controls'><button onclick=\"if(confirm('Cancellare TUTTI i dati per pulire la scheda?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Pulisci/Formatta</button></div>",
 *         "\n<button onclick=\"if(confirm('Delete ALL data to clear the card?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Clear/Format\n",
 *         "\n<button onclick=\"if(confirm('Delete ALL data to clear the card?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Clear/Format\n",
 *         "\n<button onclick=\"if(confirm('Delete ALL data to clear the card?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Clear/Format\n",
 *         "\n<button onclick=\"if(confirm('Delete ALL data to clear the card?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Clear/Format\n"
 *       ]
 *     },
 *     {
 *       "key_id": 321,
 *       "key_text": "div_class_test_controls_button_onclick_runconfigbackup_style_bac",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_RUNCONFIGBACKUP_STYLE_BAC",
 *       "values": [
 *         "<div class='test-controls'><button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup</button></div></div>",
 *         "\n<button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup\n",
 *         "\n<button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup\n",
 *         "\n<button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup\n",
 *         "\n<button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup\n"
 *       ]
 *     },
 *     {
 *       "key_id": 322,
 *       "key_text": "div_class_test_controls_button_onclick_runtest_sd_list_elenca_bu",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_BUTTON_ONCLICK_RUNTEST_SD_LIST_ELENCA_BU",
 *       "values": [
 *         "<div class='test-controls'><button onclick=\"runTest('sd_list')\">📂 Elenca</button></div></div>",
 *         "\n<button onclick=\"runTest('sd_list')\">📂 List\n",
 *         "\n<button onclick=\"runTest('sd_list')\">📂 List\n",
 *         "\n<button onclick=\"runTest('sd_list')\">📂 List\n",
 *         "\n<button onclick=\"runTest('sd_list')\">📂 List\n"
 *       ]
 *     },
 *     {
 *       "key_id": 323,
 *       "key_text": "div_class_test_controls_input_type_number_id_eeprom_addr_value_0",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_NUMBER_ID_EEPROM_ADDR_VALUE_0",
 *       "values": [
 *         "<div class='test-controls'><input type='number' id='eeprom_addr' value='0' min='0' max='2047' style='width:80px'></div></div>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 324,
 *       "key_text": "div_class_test_controls_input_type_number_id_eeprom_val_value_12",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_NUMBER_ID_EEPROM_VAL_VALUE_12",
 *       "values": [
 *         "<div class='test-controls'><input type='number' id='eeprom_val' value='123' min='0' max='255' style='width:80px'>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 325,
 *       "key_text": "div_class_test_controls_input_type_text_id_cctalk_input_placehol",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_CCTALK_INPUT_PLACEHOL",
 *       "values": [
 *         "<div class='test-controls'><input type='text' id='cctalk_input' placeholder='0x01 0x02...'><button onclick=\"sendSerial('cctalk')\">🚀 Invia</button></div></div>",
 *         "\n<button onclick=\"sendSerial('cctalk')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('cctalk')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('cctalk')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('cctalk')\">🚀 Send\n"
 *       ]
 *     },
 *     {
 *       "key_id": 326,
 *       "key_text": "div_class_test_controls_input_type_text_id_mdb_input_placeholder",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_MDB_INPUT_PLACEHOLDER",
 *       "values": [
 *         "<div class='test-controls'><input type='text' id='mdb_input' placeholder='08 00...'><button onclick=\"sendSerial('mdb')\">🚀 Invia</button></div></div>",
 *         "\n<button onclick=\"sendSerial('mdb')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('mdb')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('mdb')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('mdb')\">🚀 Send\n"
 *       ]
 *     },
 *     {
 *       "key_id": 327,
 *       "key_text": "div_class_test_controls_input_type_text_id_rs232_input_placehold",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_RS232_INPUT_PLACEHOLD",
 *       "values": [
 *         "<div class='test-controls'><input type='text' id='rs232_input' placeholder='\\\\0x55 Test...'><button onclick=\"sendSerial('rs232')\">🚀 Invia</button></div></div>",
 *         "\n<button onclick=\"sendSerial('rs232')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs232')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs232')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs232')\">🚀 Send\n"
 *       ]
 *     },
 *     {
 *       "key_id": 328,
 *       "key_text": "div_class_test_controls_input_type_text_id_rs485_input_placehold",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_CONTROLS_INPUT_TYPE_TEXT_ID_RS485_INPUT_PLACEHOLD",
 *       "values": [
 *         "<div class='test-controls'><input type='text' id='rs485_input' placeholder='Richiesta...'><button onclick=\"sendSerial('rs485')\">🚀 Invia</button></div></div>",
 *         "\n<button onclick=\"sendSerial('rs485')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs485')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs485')\">🚀 Send\n",
 *         "\n<button onclick=\"sendSerial('rs485')\">🚀 Send\n"
 *       ]
 *     },
 *     {
 *       "key_id": 329,
 *       "key_text": "div_class_test_item",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM",
 *       "values": [
 *         "<div class='test-item'>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 330,
 *       "key_text": "div_class_test_item_span_backup_json_config_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_BACKUP_JSON_CONFIG_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Backup JSON Config</span>",
 *         "\nBackup JSON Config\n",
 *         "\nBackup JSON Config\n",
 *         "\nBackup JSON Config\n",
 *         "\nBackup JSON Config\n"
 *       ]
 *     },
 *     {
 *       "key_id": 331,
 *       "key_text": "div_class_test_item_span_class_test_label_blink_tutte_le_uscite_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_BLINK_TUTTE_LE_USCITE",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Blink Tutte le Uscite (1Hz)</span>",
 *         "\nBlink All Outputs (1Hz)\n",
 *         "\nBlink All Outputs (1Hz)\n",
 *         "\nBlink All Outputs (1Hz)\n",
 *         "\nBlink All Outputs (1Hz)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 332,
 *       "key_text": "div_class_test_item_span_class_test_label_pattern_rainbow_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PATTERN_RAINBOW_SPAN",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Pattern Rainbow</span>",
 *         "\nRainbow Pattern\n",
 *         "\nRainbow Pattern\n",
 *         "\nRainbow Pattern\n",
 *         "\nRainbow Pattern\n"
 *       ]
 *     },
 *     {
 *       "key_id": 333,
 *       "key_text": "div_class_test_item_span_class_test_label_pwm1_duty_cycle_sweep_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM1_DUTY_CYCLE_SWEEP",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>PWM1 Duty Cycle Sweep</span>",
 *         "\nPWM1 Duty Cycle Sweep\n",
 *         "\nPWM1 Duty Cycle Sweep\n",
 *         "\nPWM1 Duty Cycle Sweep\n",
 *         "\nPWM1 Duty Cycle Sweep\n"
 *       ]
 *     },
 *     {
 *       "key_id": 334,
 *       "key_text": "div_class_test_item_span_class_test_label_pwm2_duty_cycle_sweep_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM2_DUTY_CYCLE_SWEEP",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>PWM2 Duty Cycle Sweep</span>",
 *         "\nPWM2 Duty Cycle Sweep\n",
 *         "\nPWM2 Duty Cycle Sweep\n",
 *         "\nPWM2 Duty Cycle Sweep\n",
 *         "\nPWM2 Duty Cycle Sweep\n"
 *       ]
 *     },
 *     {
 *       "key_id": 335,
 *       "key_text": "div_class_test_item_span_class_test_label_stato_montaggio_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_STATO_MONTAGGIO_SPAN",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Stato Montaggio:</span>",
 *         "\nMounting Status:\n",
 *         "\nMounting Status:\n",
 *         "\nMounting Status:\n",
 *         "\nMounting Status:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 336,
 *       "key_text": "div_class_test_item_span_class_test_label_test_loopback_0x55_0xa",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_0X55_0XA",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Test Loopback: 0x55, 0xAA, 0x01, 0x07</span>",
 *         "\nTest Loopback: 0x55, 0xAA, 0x01, 0x07\n",
 *         "\nTest Loopback: 0x55, 0xAA, 0x01, 0x07\n",
 *         "\nTest Loopback: 0x55, 0xAA, 0x01, 0x07\n",
 *         "\nTest Loopback: 0x55, 0xAA, 0x01, 0x07\n"
 *       ]
 *     },
 *     {
 *       "key_id": 337,
 *       "key_text": "div_class_test_item_span_class_test_label_test_loopback_echo_spa",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_ECHO_SPA",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Test Loopback/Echo</span>",
 *         "\nTest Loopback/Echo\n",
 *         "\nTest Loopback/Echo\n",
 *         "\nTest Loopback/Echo\n",
 *         "\nTest Loopback/Echo\n"
 *       ]
 *     },
 *     {
 *       "key_id": 338,
 *       "key_text": "div_class_test_item_span_class_test_label_test_loopback_invia_pa",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_TEST_LOOPBACK_INVIA_PA",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Test Loopback: invia pacchetto di prova</span>",
 *         "\nTest Loopback: send test packet\n",
 *         "\nTest Loopback: send test packet\n",
 *         "\nTest Loopback: send test packet\n",
 *         "\nTest Loopback: send test packet\n"
 *       ]
 *     },
 *     {
 *       "key_id": 339,
 *       "key_text": "div_class_test_item_span_class_test_label_ultime_letture_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_ULTIME_LETTURE_SPAN",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Ultime Letture</span>",
 *         "\nLast Readings\n",
 *         "\nLast Readings\n",
 *         "\nLast Readings\n",
 *         "\nLast Readings\n"
 *       ]
 *     },
 *     {
 *       "key_id": 340,
 *       "key_text": "div_class_test_item_span_class_test_label_ultimo_errore_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_ULTIMO_ERRORE_SPAN",
 *       "values": [
 *         "<div class='test-item'><span class='test-label'>Ultimo Errore:</span>",
 *         "\nLast Error:\n",
 *         "\nLast Error:\n",
 *         "\nLast Error:\n",
 *         "\nLast Error:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 341,
 *       "key_text": "div_class_test_item_span_dato_byte_0_255_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_DATO_BYTE_0_255_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Dato Byte (0-255)</span>",
 *         "\nByte Data (0-255)\n",
 *         "\nByte Data (0-255)\n",
 *         "\nByte Data (0-255)\n",
 *         "\nByte Data (0-255)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 342,
 *       "key_text": "div_class_test_item_span_elenco_file_root_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_ELENCO_FILE_ROOT_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Elenco File (Root)</span>",
 *         "\nFile List (Root)\n",
 *         "\nFile List (Root)\n",
 *         "\nFile List (Root)\n",
 *         "\nFile List (Root)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 343,
 *       "key_text": "div_class_test_item_span_indirizzo_0_2047_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_INDIRIZZO_0_2047_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Indirizzo (0-2047)</span>",
 *         "\nAddress (0-2047)\n",
 *         "\nAddress (0-2047)\n",
 *         "\nAddress (0-2047)\n",
 *         "\nAddress (0-2047)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 344,
 *       "key_text": "div_class_test_item_span_invia_pacchetto_es_0x01_0x02_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_INVIA_PACCHETTO_ES_0X01_0X02_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Invia Pacchetto (es: 0x01 0x02)</span>",
 *         "\nSend Packet (e.g: 0x01 0x02)\n",
 *         "\nSend Packet (e.g: 0x01 0x02)\n",
 *         "\nSend Packet (e.g: 0x01 0x02)\n",
 *         "\nSend Packet (e.g: 0x01 0x02)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 345,
 *       "key_text": "div_class_test_item_span_invia_stringa_es_0x55_0xaa_r_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_ES_0X55_0XAA_R_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Invia Stringa (es: \\\\0x55\\\\0xAA\\\\\r\\ )</span>",
 *         "\nSend String (e.g: \\0x55\\0xAA\\\r)\n",
 *         "\nSend String (e.g: \\0x55\\0xAA\\\r)\n",
 *         "\nSend String (e.g: \\0x55\\0xAA\\\r)\n",
 *         "\nSend String (e.g: \\0x55\\0xAA\\\r)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 346,
 *       "key_text": "div_class_test_item_span_invia_stringa_hex_es_08_00_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_HEX_ES_08_00_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Invia Stringa (Hex, es: 08 00)</span>",
 *         "\nSend String (Hex, e.g: 08 00)\n",
 *         "\nSend String (Hex, e.g: 08 00)\n",
 *         "\nSend String (Hex, e.g: 08 00)\n",
 *         "\nSend String (Hex, e.g: 08 00)\n"
 *       ]
 *     },
 *     {
 *       "key_id": 347,
 *       "key_text": "div_class_test_item_span_invia_stringa_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_INVIA_STRINGA_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Invia Stringa</span>",
 *         "\nSend String\n",
 *         "\nSend String\n",
 *         "\nSend String\n",
 *         "\nSend String\n"
 *       ]
 *     },
 *     {
 *       "key_id": 348,
 *       "key_text": "div_class_test_item_span_seleziona_colore_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_SELEZIONA_COLORE_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Seleziona Colore:</span>",
 *         "\nSelect Color:\n",
 *         "\nSelect Color:\n",
 *         "\nSelect Color:\n",
 *         "\nSelect Color:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 349,
 *       "key_text": "div_class_test_item_span_valori_correnti_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_CLASS_TEST_ITEM_SPAN_VALORI_CORRENTI_SPAN",
 *       "values": [
 *         "<div class='test-item'><span>Valori Correnti:</span>",
 *         "\nCurrent Values:\n",
 *         "\nCurrent Values:\n",
 *         "\nCurrent Values:\n",
 *         "\nCurrent Values:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 350,
 *       "key_text": "div_id_cctalk_status_class_status_box_monitor_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_CCTALK_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV",
 *       "values": [
 *         "<div id='cctalk_status' class='status-box'>Monitor:</div></div>",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 351,
 *       "key_text": "div_id_eeprom_status_class_status_box_pronto_per_test_eeprom_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_EEPROM_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_EEPROM_DIV",
 *       "values": [
 *         "<div id='eeprom_status' class='status-box'>Pronto per test EEPROM</div></div>",
 *         "\nReady for EEPROM test\n",
 *         "\nReady for EEPROM test\n",
 *         "\nReady for EEPROM test\n",
 *         "\nReady for EEPROM test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 352,
 *       "key_text": "div_id_gpios_status_class_status_box_stato_gpio_in_lettura_div_d",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_GPIOS_STATUS_CLASS_STATUS_BOX_STATO_GPIO_IN_LETTURA_DIV_D",
 *       "values": [
 *         "<div id='gpios_status' class='status-box'>Stato GPIO in lettura...</div></div>",
 *         "\nGPIO Status reading...\n",
 *         "\nGPIO Status reading...\n",
 *         "\nGPIO Status reading...\n",
 *         "\nGPIO Status reading...\n"
 *       ]
 *     },
 *     {
 *       "key_id": 353,
 *       "key_text": "div_id_gpios_test_grid_caricamento_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_GPIOS_TEST_GRID_CARICAMENTO_DIV",
 *       "values": [
 *         "<div id='gpios_test_grid'>Caricamento...</div>",
 *         "\nLoading...\n",
 *         "\nLoading...\n",
 *         "\nLoading...\n",
 *         "\nLoading...\n"
 *       ]
 *     },
 *     {
 *       "key_id": 354,
 *       "key_text": "div_id_ioexp_status_class_status_box_pronto_per_test_i_o_expande",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_IOEXP_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_I_O_EXPANDE",
 *       "values": [
 *         "<div id='ioexp_status' class='status-box'>Pronto per test I/O Expander</div></div>",
 *         "\nReady for I/O Expander test\n",
 *         "\nReady for I/O Expander test\n",
 *         "\nReady for I/O Expander test\n",
 *         "\nReady for I/O Expander test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 355,
 *       "key_text": "div_id_led_status_class_status_box_pronto_per_test_led_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_LED_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_LED_DIV_DIV",
 *       "values": [
 *         "<div id='led_status' class='status-box'>Pronto per test LED</div></div>",
 *         "\nReady for LED test\n",
 *         "\nReady for LED test\n",
 *         "\nReady for LED test\n",
 *         "\nReady for LED test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 356,
 *       "key_text": "div_id_mdb_status_class_status_box_pronto_per_test_mdb_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_MDB_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_MDB_DIV_DIV",
 *       "values": [
 *         "<div id='mdb_status' class='status-box'>Pronto per test MDB</div></div>",
 *         "\nReady for MDB test\n",
 *         "\nReady for MDB test\n",
 *         "\nReady for MDB test\n",
 *         "\nReady for MDB test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 357,
 *       "key_text": "div_id_pwm_status_class_status_box_pronto_per_test_pwm_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_PWM_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_PWM_DIV_DIV",
 *       "values": [
 *         "<div id='pwm_status' class='status-box'>Pronto per test PWM</div></div>",
 *         "\nReady for PWM test\n",
 *         "\nReady for PWM test\n",
 *         "\nReady for PWM test\n",
 *         "\nReady for PWM test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 358,
 *       "key_text": "div_id_rs232_status_class_status_box_monitor_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_RS232_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV",
 *       "values": [
 *         "<div id='rs232_status' class='status-box'>Monitor:</div></div>",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 359,
 *       "key_text": "div_id_rs485_status_class_status_box_monitor_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_RS485_STATUS_CLASS_STATUS_BOX_MONITOR_DIV_DIV",
 *       "values": [
 *         "<div id='rs485_status' class='status-box'>Monitor:</div></div>",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n",
 *         "\nMonitor:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 360,
 *       "key_text": "div_id_scanner_status_class_status_box_nessuna_lettura_ancora_di",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_SCANNER_STATUS_CLASS_STATUS_BOX_NESSUNA_LETTURA_ANCORA_DI",
 *       "values": [
 *         "<div id='scanner_status' class='status-box'>Nessuna lettura ancora.</div></div>",
 *         "\nNo readings yet.\n",
 *         "\nNo readings yet.\n",
 *         "\nNo readings yet.\n",
 *         "\nNo readings yet.\n"
 *       ]
 *     },
 *     {
 *       "key_id": 361,
 *       "key_text": "div_id_sd_status_class_status_box_style_display_none_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_SD_STATUS_CLASS_STATUS_BOX_STYLE_DISPLAY_NONE_DIV_DIV",
 *       "values": [
 *         "<div id='sd_status' class='status-box' style='display:none'></div></div>",
 *         "\n",
 *         "\n",
 *         "\n",
 *         "\n"
 *       ]
 *     },
 *     {
 *       "key_id": 362,
 *       "key_text": "div_id_section_gpio33_class_section_collapsed_h2_gpio_ausiliari_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_SECTION_GPIO33_CLASS_SECTION_COLLAPSED_H2_GPIO_AUSILIARI",
 *       "values": [
 *         "<div id='section_gpio33' class='section collapsed'><h2>🔘 GPIO Ausiliari (GPIO33)<span class='section-toggle-icon'>▸</span></h2>",
 *         "\n🔘 Auxiliary GPIOs (GPIO33)▸\n",
 *         "\n🔘 Auxiliary GPIOs (GPIO33)▸\n",
 *         "\n🔘 Auxiliary GPIOs (GPIO33)▸\n",
 *         "\n🔘 Auxiliary GPIOs (GPIO33)▸\n"
 *       ]
 *     },
 *     {
 *       "key_id": 363,
 *       "key_text": "div_id_section_scanner_class_section_collapsed_h2_scanner_usb_ba",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_SECTION_SCANNER_CLASS_SECTION_COLLAPSED_H2_SCANNER_USB_BA",
 *       "values": [
 *         "<div id='section_scanner' class='section collapsed'><h2>🔍 Scanner USB (Barcode/QR)<span class='section-toggle-icon'>▸</span></h2>",
 *         "\n🔍 USB Scanner (Barcode/QR)▸\n",
 *         "\n🔍 USB Scanner (Barcode/QR)▸\n",
 *         "\n🔍 USB Scanner (Barcode/QR)▸\n",
 *         "\n🔍 USB Scanner (Barcode/QR)▸\n"
 *       ]
 *     },
 *     {
 *       "key_id": 364,
 *       "key_text": "div_id_sht_status_class_status_box_pronto_per_test_sht40_div_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DIV_ID_SHT_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_SHT40_DIV_DIV",
 *       "values": [
 *         "<div id='sht_status' class='status-box'>Pronto per test SHT40</div></div>",
 *         "\nReady for SHT40 test\n",
 *         "\nReady for SHT40 test\n",
 *         "\nReady for SHT40 test\n",
 *         "\nReady for SHT40 test\n"
 *       ]
 *     },
 *     {
 *       "key_id": 365,
 *       "key_text": "duty",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_DUTY",
 *       "values": [
 *         "Duty (%):",
 *         "Duty (%):",
 *         "Duty (%):",
 *         "Duty (%):",
 *         "Duty (%):"
 *       ]
 *     },
 *     {
 *       "key_id": 366,
 *       "key_text": "eeprom_24lc16",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_EEPROM_24LC16",
 *       "values": [
 *         "💾 EEPROM 24LC16",
 *         "💾 EEPROM 24LC16",
 *         "💾 EEPROM 24LC16",
 *         "💾 EEPROM 24LC16",
 *         "💾 EEPROM 24LC16"
 *       ]
 *     },
 *     {
 *       "key_id": 367,
 *       "key_text": "elenca",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ELENCA",
 *       "values": [
 *         "📂 Elenca",
 *         "📂 List",
 *         "📂 List",
 *         "📂 List",
 *         "📂 List"
 *       ]
 *     },
 *     {
 *       "key_id": 368,
 *       "key_text": "elenco_file_root",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ELENCO_FILE_ROOT",
 *       "values": [
 *         "Elenco File (Root)",
 *         "File List (Root)",
 *         "File List (Root)",
 *         "File List (Root)",
 *         "File List (Root)"
 *       ]
 *     },
 *     {
 *       "key_id": 369,
 *       "key_text": "else_out",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ELSE_OUT",
 *       "values": [
 *         "else out += '",
 *         "else out += '",
 *         "else out += '",
 *         "else out += '",
 *         "else out += '"
 *       ]
 *     },
 *     {
 *       "key_id": 370,
 *       "key_text": "errore",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ERRORE",
 *       "values": [
 *         "❌ Errore",
 *         "❌ Error",
 *         "❌ Error",
 *         "❌ Error",
 *         "❌ Error"
 *       ]
 *     },
 *     {
 *       "key_id": 371,
 *       "key_text": "errore_comando_scanner",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ERRORE_COMANDO_SCANNER",
 *       "values": [
 *         "❌ Errore comando scanner",
 *         "❌ Scanner command error",
 *         "❌ Scanner command error",
 *         "❌ Scanner command error",
 *         "❌ Scanner command error"
 *       ]
 *     },
 *     {
 *       "key_id": 372,
 *       "key_text": "formattazione_fat32",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_FORMATTAZIONE_FAT32",
 *       "values": [
 *         "⚠️ Formattazione FAT32",
 *         "⚠️ FAT32 Formatting",
 *         "⚠️ FAT32 Formatting",
 *         "⚠️ FAT32 Formatting",
 *         "⚠️ FAT32 Formatting"
 *       ]
 *     },
 *     {
 *       "key_id": 373,
 *       "key_text": "forza_test_lettura",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_FORZA_TEST_LETTURA",
 *       "values": [
 *         "🔄 FORZA TEST LETTURA",
 *         "🔄 FORCE READ TEST",
 *         "🔄 FORCE READ TEST",
 *         "🔄 FORCE READ TEST",
 *         "🔄 FORCE READ TEST"
 *       ]
 *     },
 *     {
 *       "key_id": 374,
 *       "key_text": "freq_hz",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_FREQ_HZ",
 *       "values": [
 *         "Freq (Hz):",
 *         "Freq (Hz):",
 *         "Freq (Hz):",
 *         "Freq (Hz):",
 *         "Freq (Hz):"
 *       ]
 *     },
 *     {
 *       "key_id": 375,
 *       "key_text": "gpio_ausiliari_gpio33",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_GPIO_AUSILIARI_GPIO33",
 *       "values": [
 *         "🔘 GPIO Ausiliari (GPIO33)",
 *         "🔘 Auxiliary GPIOs (GPIO33)",
 *         "🔘 Auxiliary GPIOs (GPIO33)",
 *         "🔘 Auxiliary GPIOs (GPIO33)",
 *         "🔘 Auxiliary GPIOs (GPIO33)"
 *       ]
 *     },
 *     {
 *       "key_id": 376,
 *       "key_text": "h",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_H",
 *       "values": [
 *         "h+='",
 *         "h+='",
 *         "h+='",
 *         "h+='",
 *         "h+='"
 *       ]
 *     },
 *     {
 *       "key_id": 377,
 *       "key_text": "h3_controllo_manuale_h3",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_H3_CONTROLLO_MANUALE_H3",
 *       "values": [
 *         "<h3>Controllo Manuale</h3>",
 *         "\nManual Control\n",
 *         "\nManual Control\n",
 *         "\nManual Control\n",
 *         "\nManual Control\n"
 *       ]
 *     },
 *     {
 *       "key_id": 378,
 *       "key_text": "h3_controllo_manuale_pwm_h3",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_H3_CONTROLLO_MANUALE_PWM_H3",
 *       "values": [
 *         "<h3>Controllo Manuale PWM</h3>",
 *         "\nManual PWM Control\n",
 *         "\nManual PWM Control\n",
 *         "\nManual PWM Control\n",
 *         "\nManual PWM Control\n"
 *       ]
 *     },
 *     {
 *       "key_id": 379,
 *       "key_text": "hex",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_HEX",
 *       "values": [
 *         "HEX",
 *         "HEX",
 *         "HEX",
 *         "HEX",
 *         "HEX"
 *       ]
 *     },
 *     {
 *       "key_id": 380,
 *       "key_text": "hex_2",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_HEX_2",
 *       "values": [
 *         ".' + hex + '",
 *         ".' + hex + '",
 *         ".' + hex + '",
 *         ".' + hex + '",
 *         ".' + hex + '"
 *       ]
 *     },
 *     {
 *       "key_id": 381,
 *       "key_text": "hide_test_sections_when_corresponding_peripheral_is_disabled_in_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_HIDE_TEST_SECTIONS_WHEN_CORRESPONDING_PERIPHERAL_IS_DISABLED_IN",
 *       "values": [
 *         "// Hide /test sections when corresponding peripheral is disabled in /config",
 *         "// Hide /test sections when corresponding peripheral is disabled in /config",
 *         "// Hide /test sections when corresponding peripheral is disabled in /config",
 *         "// Hide /test sections when corresponding peripheral is disabled in /config",
 *         "// Hide /test sections when corresponding peripheral is disabled in /config"
 *       ]
 *     },
 *     {
 *       "key_id": 382,
 *       "key_text": "i2c_0x45",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_I2C_0X45",
 *       "values": [
 *         "(I2C 0x45)",
 *         "(I2C 0x45)",
 *         "(I2C 0x45)",
 *         "(I2C 0x45)",
 *         "(I2C 0x45)"
 *       ]
 *     },
 *     {
 *       "key_id": 383,
 *       "key_text": "if_isout_h",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_IF_ISOUT_H",
 *       "values": [
 *         "if(isOut) h+='",
 *         "if(isOut) h+='",
 *         "if(isOut) h+='",
 *         "if(isOut) h+='",
 *         "if(isOut) h+='"
 *       ]
 *     },
 *     {
 *       "key_id": 384,
 *       "key_text": "indirizzo_0_2047",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INDIRIZZO_0_2047",
 *       "values": [
 *         "Indirizzo (0-2047)",
 *         "Address (0-2047)",
 *         "Address (0-2047)",
 *         "Address (0-2047)",
 *         "Address (0-2047)"
 *       ]
 *     },
 *     {
 *       "key_id": 385,
 *       "key_text": "ingressi_chip_0x44",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INGRESSI_CHIP_0X44",
 *       "values": [
 *         "Ingressi (Chip 0x44)",
 *         "Inputs (Chip 0x44)",
 *         "Inputs (Chip 0x44)",
 *         "Inputs (Chip 0x44)",
 *         "Inputs (Chip 0x44)"
 *       ]
 *     },
 *     {
 *       "key_id": 386,
 *       "key_text": "init",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIT",
 *       "values": [
 *         "⚡ Init",
 *         "⚡ Init",
 *         "⚡ Init",
 *         "⚡ Init",
 *         "⚡ Init"
 *       ]
 *     },
 *     {
 *       "key_id": 387,
 *       "key_text": "inizia_blink",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIZIA_BLINK",
 *       "values": [
 *         "▶️ Inizia Blink",
 *         "▶️ Start Blink",
 *         "▶️ Start Blink",
 *         "▶️ Start Blink",
 *         "▶️ Start Blink"
 *       ]
 *     },
 *     {
 *       "key_id": 388,
 *       "key_text": "inizia_manuale",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIZIA_MANUALE",
 *       "values": [
 *         "🚀 Inizia Manuale",
 *         "🚀 Start Manual",
 *         "🚀 Start Manual",
 *         "🚀 Start Manual",
 *         "🚀 Start Manual"
 *       ]
 *     },
 *     {
 *       "key_id": 389,
 *       "key_text": "inizia_rainbow",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIZIA_RAINBOW",
 *       "values": [
 *         "▶️ Inizia Rainbow",
 *         "▶️ Start Rainbow",
 *         "▶️ Start Rainbow",
 *         "▶️ Start Rainbow",
 *         "▶️ Start Rainbow"
 *       ]
 *     },
 *     {
 *       "key_id": 390,
 *       "key_text": "inizia_sweep",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIZIA_SWEEP",
 *       "values": [
 *         "▶️ Inizia Sweep",
 *         "▶️ Start Sweep",
 *         "▶️ Start Sweep",
 *         "▶️ Start Sweep",
 *         "▶️ Start Sweep"
 *       ]
 *     },
 *     {
 *       "key_id": 391,
 *       "key_text": "inizia_test",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INIZIA_TEST",
 *       "values": [
 *         "▶️ Inizia Test",
 *         "▶️ Start Test",
 *         "▶️ Start Test",
 *         "▶️ Start Test",
 *         "▶️ Start Test"
 *       ]
 *     },
 *     {
 *       "key_id": 392,
 *       "key_text": "invia_pacchetto_es_0x01_0x02",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIA_PACCHETTO_ES_0X01_0X02",
 *       "values": [
 *         "Invia Pacchetto (es: 0x01 0x02)",
 *         "Send Packet (e.g: 0x01 0x02)",
 *         "Send Packet (e.g: 0x01 0x02)",
 *         "Send Packet (e.g: 0x01 0x02)",
 *         "Send Packet (e.g: 0x01 0x02)"
 *       ]
 *     },
 *     {
 *       "key_id": 393,
 *       "key_text": "invia_stringa",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIA_STRINGA",
 *       "values": [
 *         "Invia Stringa",
 *         "Send String",
 *         "Send String",
 *         "Send String",
 *         "Send String"
 *       ]
 *     },
 *     {
 *       "key_id": 394,
 *       "key_text": "invia_stringa_es_0x55_0xaa",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIA_STRINGA_ES_0X55_0XAA",
 *       "values": [
 *         "Invia Stringa (es: \\0x55\\0xAA\\",
 *         "Send String (e.g: \\0x55\\0xAA\\",
 *         "Send String (e.g: \\0x55\\0xAA\\",
 *         "Send String (e.g: \\0x55\\0xAA\\",
 *         "Send String (e.g: \\0x55\\0xAA\\"
 *       ]
 *     },
 *     {
 *       "key_id": 395,
 *       "key_text": "invia_stringa_hex_es_08_00",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIA_STRINGA_HEX_ES_08_00",
 *       "values": [
 *         "Invia Stringa (Hex, es: 08 00)",
 *         "Send String (Hex, e.g: 08 00)",
 *         "Send String (Hex, e.g: 08 00)",
 *         "Send String (Hex, e.g: 08 00)",
 *         "Send String (Hex, e.g: 08 00)"
 *       ]
 *     },
 *     {
 *       "key_id": 396,
 *       "key_text": "invio_backup",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_INVIO_BACKUP",
 *       "values": [
 *         "➡️ Invio Backup...",
 *         "➡️ Sending Backup...",
 *         "➡️ Sending Backup...",
 *         "➡️ Sending Backup...",
 *         "➡️ Sending Backup..."
 *       ]
 *     },
 *     {
 *       "key_id": 397,
 *       "key_text": "leggi",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LEGGI",
 *       "values": [
 *         "📖 Leggi",
 *         "📖 Read",
 *         "📖 Read",
 *         "📖 Read",
 *         "📖 Read"
 *       ]
 *     },
 *     {
 *       "key_id": 398,
 *       "key_text": "leggi_json",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LEGGI_JSON",
 *       "values": [
 *         "📄 Leggi JSON",
 *         "📄 Read JSON",
 *         "📄 Read JSON",
 *         "📄 Read JSON",
 *         "📄 Read JSON"
 *       ]
 *     },
 *     {
 *       "key_id": 399,
 *       "key_text": "lettura_ok",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LETTURA_OK",
 *       "values": [
 *         "✅ Lettura OK",
 *         "✅ Reading OK",
 *         "✅ Reading OK",
 *         "✅ Reading OK",
 *         "✅ Reading OK"
 *       ]
 *     },
 *     {
 *       "key_id": 400,
 *       "key_text": "live_update_to_hardware_short_debounce",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LIVE_UPDATE_TO_HARDWARE_SHORT_DEBOUNCE",
 *       "values": [
 *         "// live update to hardware (short debounce)",
 *         "// live update to hardware (short debounce)",
 *         "// live update to hardware (short debounce)",
 *         "// live update to hardware (short debounce)",
 *         "// live update to hardware (short debounce)"
 *       ]
 *     },
 *     {
 *       "key_id": 401,
 *       "key_text": "log_operazioni_sd",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LOG_OPERAZIONI_SD",
 *       "values": [
 *         "Log operazioni SD...",
 *         "Log SD operations...",
 *         "Log SD operations...",
 *         "Log SD operations...",
 *         "Log SD operations..."
 *       ]
 *     },
 *     {
 *       "key_id": 402,
 *       "key_text": "luminosita",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_LUMINOSITA",
 *       "values": [
 *         "Luminosità:",
 *         "Brightness:",
 *         "Brightness:",
 *         "Brightness:",
 *         "Brightness:"
 *       ]
 *     },
 *     {
 *       "key_id": 403,
 *       "key_text": "mdb_multi_drop_bus",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_MDB_MULTI_DROP_BUS",
 *       "values": [
 *         "🎰 MDB (Multi-Drop Bus)",
 *         "🎰 MDB (Multi-Drop Bus)",
 *         "🎰 MDB (Multi-Drop Bus)",
 *         "🎰 MDB (Multi-Drop Bus)",
 *         "🎰 MDB (Multi-Drop Bus)"
 *       ]
 *     },
 *     {
 *       "key_id": 404,
 *       "key_text": "monitor",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_MONITOR",
 *       "values": [
 *         "Monitor:",
 *         "Monitor:",
 *         "Monitor:",
 *         "Monitor:",
 *         "Monitor:"
 *       ]
 *     },
 *     {
 *       "key_id": 405,
 *       "key_text": "nessuna_lettura_ancora",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_NESSUNA_LETTURA_ANCORA",
 *       "values": [
 *         "Nessuna lettura ancora.",
 *         "No readings yet.",
 *         "No readings yet.",
 *         "No readings yet.",
 *         "No readings yet."
 *       ]
 *     },
 *     {
 *       "key_id": 406,
 *       "key_text": "off",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_OFF",
 *       "values": [
 *         "OFF",
 *         "OFF",
 *         "OFF",
 *         "OFF",
 *         "OFF"
 *       ]
 *     },
 *     {
 *       "key_id": 407,
 *       "key_text": "ok",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_OK",
 *       "values": [
 *         "✅ OK",
 *         "✅ OK",
 *         "✅ OK",
 *         "✅ OK",
 *         "✅ OK"
 *       ]
 *     },
 *     {
 *       "key_id": 408,
 *       "key_text": "on",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ON",
 *       "values": [
 *         "ON",
 *         "ON",
 *         "ON",
 *         "ON",
 *         "ON"
 *       ]
 *     },
 *     {
 *       "key_id": 409,
 *       "key_text": "out1_gpio47",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_OUT1_GPIO47",
 *       "values": [
 *         "OUT1 (GPIO47)",
 *         "OUT1 (GPIO47)",
 *         "OUT1 (GPIO47)",
 *         "OUT1 (GPIO47)",
 *         "OUT1 (GPIO47)"
 *       ]
 *     },
 *     {
 *       "key_id": 410,
 *       "key_text": "out2_gpio48",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_OUT2_GPIO48",
 *       "values": [
 *         "OUT2 (GPIO48)",
 *         "OUT2 (GPIO48)",
 *         "OUT2 (GPIO48)",
 *         "OUT2 (GPIO48)",
 *         "OUT2 (GPIO48)"
 *       ]
 *     },
 *     {
 *       "key_id": 411,
 *       "key_text": "pattern_rainbow",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PATTERN_RAINBOW",
 *       "values": [
 *         "Pattern Rainbow",
 *         "Rainbow Pattern",
 *         "Rainbow Pattern",
 *         "Rainbow Pattern",
 *         "Rainbow Pattern"
 *       ]
 *     },
 *     {
 *       "key_id": 412,
 *       "key_text": "pin_12_a_17_b",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_12_A_17_B",
 *       "values": [
 *         "(PIN 12 A, 17 B)",
 *         "(PIN 12 A, 17 B)",
 *         "(PIN 12 A, 17 B)",
 *         "(PIN 12 A, 17 B)",
 *         "(PIN 12 A, 17 B)"
 *       ]
 *     },
 *     {
 *       "key_id": 413,
 *       "key_text": "pin_15_gpio_5",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_15_GPIO_5",
 *       "values": [
 *         "(PIN 15 / GPIO 5)",
 *         "(PIN 15 / GPIO 5)",
 *         "(PIN 15 / GPIO 5)",
 *         "(PIN 15 / GPIO 5)",
 *         "(PIN 15 / GPIO 5)"
 *       ]
 *     },
 *     {
 *       "key_id": 414,
 *       "key_text": "pin_23_tx_35_rx",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_23_TX_35_RX",
 *       "values": [
 *         "(PIN 23 TX, 35 RX)",
 *         "(PIN 23 TX, 35 RX)",
 *         "(PIN 23 TX, 35 RX)",
 *         "(PIN 23 TX, 35 RX)",
 *         "(PIN 23 TX, 35 RX)"
 *       ]
 *     },
 *     {
 *       "key_id": 415,
 *       "key_text": "pin_32_37_i2c",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_32_37_I2C",
 *       "values": [
 *         "(PIN 32, 37 / I2C)",
 *         "(PIN 32, 37 / I2C)",
 *         "(PIN 32, 37 / I2C)",
 *         "(PIN 32, 37 / I2C)",
 *         "(PIN 32, 37 / I2C)"
 *       ]
 *     },
 *     {
 *       "key_id": 416,
 *       "key_text": "pin_34_38",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_34_38",
 *       "values": [
 *         "(PIN 34, 38)",
 *         "(PIN 34, 38)",
 *         "(PIN 34, 38)",
 *         "(PIN 34, 38)",
 *         "(PIN 34, 38)"
 *       ]
 *     },
 *     {
 *       "key_id": 417,
 *       "key_text": "pin_39",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_39",
 *       "values": [
 *         "(PIN 39)",
 *         "(PIN 39)",
 *         "(PIN 39)",
 *         "(PIN 39)",
 *         "(PIN 39)"
 *       ]
 *     },
 *     {
 *       "key_id": 418,
 *       "key_text": "pin_8_11",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PIN_8_11",
 *       "values": [
 *         "(PIN 8, 11)",
 *         "(PIN 8, 11)",
 *         "(PIN 8, 11)",
 *         "(PIN 8, 11)",
 *         "(PIN 8, 11)"
 *       ]
 *     },
 *     {
 *       "key_id": 419,
 *       "key_text": "poi_pulisce_lato_server",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_POI_PULISCE_LATO_SERVER",
 *       "values": [
 *         "// Poi pulisce lato server",
 *         "// Then clears server side",
 *         "// Then clears server side",
 *         "// Then clears server side",
 *         "// Then clears server side"
 *       ]
 *     },
 *     {
 *       "key_id": 420,
 *       "key_text": "pronto_per_test_eeprom",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_EEPROM",
 *       "values": [
 *         "Pronto per test EEPROM",
 *         "Ready for EEPROM test",
 *         "Ready for EEPROM test",
 *         "Ready for EEPROM test",
 *         "Ready for EEPROM test"
 *       ]
 *     },
 *     {
 *       "key_id": 421,
 *       "key_text": "pronto_per_test_i_o_expander",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_I_O_EXPANDER",
 *       "values": [
 *         "Pronto per test I/O Expander",
 *         "Ready for I/O Expander test",
 *         "Ready for I/O Expander test",
 *         "Ready for I/O Expander test",
 *         "Ready for I/O Expander test"
 *       ]
 *     },
 *     {
 *       "key_id": 422,
 *       "key_text": "pronto_per_test_led",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_LED",
 *       "values": [
 *         "Pronto per test LED",
 *         "Ready for LED test",
 *         "Ready for LED test",
 *         "Ready for LED test",
 *         "Ready for LED test"
 *       ]
 *     },
 *     {
 *       "key_id": 423,
 *       "key_text": "pronto_per_test_mdb",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_MDB",
 *       "values": [
 *         "Pronto per test MDB",
 *         "Ready for MDB test",
 *         "Ready for MDB test",
 *         "Ready for MDB test",
 *         "Ready for MDB test"
 *       ]
 *     },
 *     {
 *       "key_id": 424,
 *       "key_text": "pronto_per_test_pwm",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_PWM",
 *       "values": [
 *         "Pronto per test PWM",
 *         "Ready for PWM test",
 *         "Ready for PWM test",
 *         "Ready for PWM test",
 *         "Ready for PWM test"
 *       ]
 *     },
 *     {
 *       "key_id": 425,
 *       "key_text": "pronto_per_test_sht40",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PRONTO_PER_TEST_SHT40",
 *       "values": [
 *         "Pronto per test SHT40",
 *         "Ready for SHT40 test",
 *         "Ready for SHT40 test",
 *         "Ready for SHT40 test",
 *         "Ready for SHT40 test"
 *       ]
 *     },
 *     {
 *       "key_id": 426,
 *       "key_text": "pulisce_immediatamente_l_area_di_testo_lato_client",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PULISCE_IMMEDIATAMENTE_L_AREA_DI_TESTO_LATO_CLIENT",
 *       "values": [
 *         "// Pulisce immediatamente l'area di testo lato client",
 *         "// Immediately clears text area on client side",
 *         "// Immediately clears text area on client side",
 *         "// Immediately clears text area on client side",
 *         "// Immediately clears text area on client side"
 *       ]
 *     },
 *     {
 *       "key_id": 427,
 *       "key_text": "pulisci_formatta",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PULISCI_FORMATTA",
 *       "values": [
 *         "🧨 Pulisci/Formatta",
 *         "🧨 Clear/Format",
 *         "🧨 Clear/Format",
 *         "🧨 Clear/Format",
 *         "🧨 Clear/Format"
 *       ]
 *     },
 *     {
 *       "key_id": 428,
 *       "key_text": "pwm",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PWM",
 *       "values": [
 *         "⚡ PWM",
 *         "⚡ PWM",
 *         "⚡ PWM",
 *         "⚡ PWM",
 *         "⚡ PWM"
 *       ]
 *     },
 *     {
 *       "key_id": 429,
 *       "key_text": "pwm1_duty_cycle_sweep",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PWM1_DUTY_CYCLE_SWEEP",
 *       "values": [
 *         "PWM1 Duty Cycle Sweep",
 *         "PWM1 Duty Cycle Sweep",
 *         "PWM1 Duty Cycle Sweep",
 *         "PWM1 Duty Cycle Sweep",
 *         "PWM1 Duty Cycle Sweep"
 *       ]
 *     },
 *     {
 *       "key_id": 430,
 *       "key_text": "pwm2_duty_cycle_sweep",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_PWM2_DUTY_CYCLE_SWEEP",
 *       "values": [
 *         "PWM2 Duty Cycle Sweep",
 *         "PWM2 Duty Cycle Sweep",
 *         "PWM2 Duty Cycle Sweep",
 *         "PWM2 Duty Cycle Sweep",
 *         "PWM2 Duty Cycle Sweep"
 *       ]
 *     },
 *     {
 *       "key_id": 431,
 *       "key_text": "scanner_off",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_OFF",
 *       "values": [
 *         "Scanner OFF",
 *         "Scanner OFF",
 *         "Scanner OFF",
 *         "Scanner OFF",
 *         "Scanner OFF"
 *       ]
 *     },
 *     {
 *       "key_id": 432,
 *       "key_text": "scanner_on",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_ON",
 *       "values": [
 *         "Scanner ON",
 *         "Scanner ON",
 *         "Scanner ON",
 *         "Scanner ON",
 *         "Scanner ON"
 *       ]
 *     },
 *     {
 *       "key_id": 433,
 *       "key_text": "scanner_setup",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_SETUP",
 *       "values": [
 *         "Scanner SETUP",
 *         "Scanner SETUP",
 *         "Scanner SETUP",
 *         "Scanner SETUP",
 *         "Scanner SETUP"
 *       ]
 *     },
 *     {
 *       "key_id": 434,
 *       "key_text": "scanner_state",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_STATE",
 *       "values": [
 *         "Scanner STATE",
 *         "Scanner STATE",
 *         "Scanner STATE",
 *         "Scanner STATE",
 *         "Scanner STATE"
 *       ]
 *     },
 *     {
 *       "key_id": 435,
 *       "key_text": "scanner_ui_mostra_le_ultime_letture_tag_scanner_e_comandi_on_off",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_UI_MOSTRA_LE_ULTIME_LETTURE_TAG_SCANNER_E_COMANDI_ON_OFF",
 *       "values": [
 *         "// Scanner UI: mostra le ultime letture (tag 'SCANNER') e comandi ON/OFF",
 *         "// Scanner UI: shows last readings (tag 'SCANNER') and ON/OFF commands",
 *         "// Scanner UI: shows last readings (tag 'SCANNER') and ON/OFF commands",
 *         "// Scanner UI: shows last readings (tag 'SCANNER') and ON/OFF commands",
 *         "// Scanner UI: shows last readings (tag 'SCANNER') and ON/OFF commands"
 *       ]
 *     },
 *     {
 *       "key_id": 436,
 *       "key_text": "scanner_usb_barcode_qr",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCANNER_USB_BARCODE_QR",
 *       "values": [
 *         "🔍 Scanner USB (Barcode/QR)",
 *         "🔍 USB Scanner (Barcode/QR)",
 *         "🔍 USB Scanner (Barcode/QR)",
 *         "🔍 USB Scanner (Barcode/QR)",
 *         "🔍 USB Scanner (Barcode/QR)"
 *       ]
 *     },
 *     {
 *       "key_id": 437,
 *       "key_text": "scheda_microsd",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCHEDA_MICROSD",
 *       "values": [
 *         "💾 Scheda MicroSD",
 *         "💾 MicroSD Card",
 *         "💾 MicroSD Card",
 *         "💾 MicroSD Card",
 *         "💾 MicroSD Card"
 *       ]
 *     },
 *     {
 *       "key_id": 438,
 *       "key_text": "schedule_persistence_after_5s_of_inactivity",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCHEDULE_PERSISTENCE_AFTER_5S_OF_INACTIVITY",
 *       "values": [
 *         "// schedule persistence after 5s of inactivity",
 *         "// schedule persistence after 5s of inactivity",
 *         "// schedule persistence after 5s of inactivity",
 *         "// schedule persistence after 5s of inactivity",
 *         "// schedule persistence after 5s of inactivity"
 *       ]
 *     },
 *     {
 *       "key_id": 439,
 *       "key_text": "scrivi",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SCRIVI",
 *       "values": [
 *         "✍️ Scrivi",
 *         "✍️ Write",
 *         "✍️ Write",
 *         "✍️ Write",
 *         "✍️ Write"
 *       ]
 *     },
 *     {
 *       "key_id": 440,
 *       "key_text": "select_id_cctalk_mode_onchange_clearserial_cctalk_option_value_h",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SELECT_ID_CCTALK_MODE_ONCHANGE_CLEARSERIAL_CCTALK_OPTION_VALUE_H",
 *       "values": [
 *         "<select id='cctalk_mode' onchange=\"clearSerial('cctalk')\"><option value='HEX'>HEX</option><option value='TEXT'>TEXT</option></select>",
 *         " <select id='cctalk_mode' onchange=\"clearSerial('cctalk')\"> HEX TEXT",
 *         " <select id='cctalk_mode' onchange=\"clearSerial('cctalk')\"> HEX TEXT",
 *         " <select id='cctalk_mode' onchange=\"clearSerial('cctalk')\"> HEX TEXT",
 *         " <select id='cctalk_mode' onchange=\"clearSerial('cctalk')\"> HEX TEXT"
 *       ]
 *     },
 *     {
 *       "key_id": 441,
 *       "key_text": "select_id_mdb_mode_onchange_clearserial_mdb_option_value_hex_hex",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SELECT_ID_MDB_MODE_ONCHANGE_CLEARSERIAL_MDB_OPTION_VALUE_HEX_HEX",
 *       "values": [
 *         "<select id='mdb_mode' onchange=\"clearSerial('mdb')\"><option value='HEX'>HEX</option><option value='TEXT'>TEXT</option></select>",
 *         " <select id='mdb_mode' onchange=\"clearSerial('mdb')\"> HEX TEXT",
 *         " <select id='mdb_mode' onchange=\"clearSerial('mdb')\"> HEX TEXT",
 *         " <select id='mdb_mode' onchange=\"clearSerial('mdb')\"> HEX TEXT",
 *         " <select id='mdb_mode' onchange=\"clearSerial('mdb')\"> HEX TEXT"
 *       ]
 *     },
 *     {
 *       "key_id": 442,
 *       "key_text": "select_id_rs232_mode_onchange_clearserial_rs232_option_value_hex",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SELECT_ID_RS232_MODE_ONCHANGE_CLEARSERIAL_RS232_OPTION_VALUE_HEX",
 *       "values": [
 *         "<select id='rs232_mode' onchange=\"clearSerial('rs232')\"><option value='HEX'>HEX</option><option value='TEXT'>TEXT</option></select>",
 *         " <select id='rs232_mode' onchange=\"clearSerial('rs232')\"> HEX TEXT",
 *         " <select id='rs232_mode' onchange=\"clearSerial('rs232')\"> HEX TEXT",
 *         " <select id='rs232_mode' onchange=\"clearSerial('rs232')\"> HEX TEXT",
 *         " <select id='rs232_mode' onchange=\"clearSerial('rs232')\"> HEX TEXT"
 *       ]
 *     },
 *     {
 *       "key_id": 443,
 *       "key_text": "select_id_rs485_mode_onchange_clearserial_rs485_option_value_hex",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SELECT_ID_RS485_MODE_ONCHANGE_CLEARSERIAL_RS485_OPTION_VALUE_HEX",
 *       "values": [
 *         "<select id='rs485_mode' onchange=\"clearSerial('rs485')\"><option value='HEX'>HEX</option><option value='TEXT'>TEXT</option></select>",
 *         " <select id='rs485_mode' onchange=\"clearSerial('rs485')\"> HEX TEXT",
 *         " <select id='rs485_mode' onchange=\"clearSerial('rs485')\"> HEX TEXT",
 *         " <select id='rs485_mode' onchange=\"clearSerial('rs485')\"> HEX TEXT",
 *         " <select id='rs485_mode' onchange=\"clearSerial('rs485')\"> HEX TEXT"
 *       ]
 *     },
 *     {
 *       "key_id": 444,
 *       "key_text": "seleziona_colore",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SELEZIONA_COLORE",
 *       "values": [
 *         "Seleziona Colore:",
 *         "Select Color:",
 *         "Select Color:",
 *         "Select Color:",
 *         "Select Color:"
 *       ]
 *     },
 *     {
 *       "key_id": 445,
 *       "key_text": "sensore_sht40",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SENSORE_SHT40",
 *       "values": [
 *         "🌡️ Sensore SHT40",
 *         "🌡️ SHT40 Sensor",
 *         "🌡️ SHT40 Sensor",
 *         "🌡️ SHT40 Sensor",
 *         "🌡️ SHT40 Sensor"
 *       ]
 *     },
 *     {
 *       "key_id": 446,
 *       "key_text": "sensors",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SENSORS",
 *       "values": [
 *         "/* sensors */",
 *         "/\n sensors */",
 *         "/\n sensors */",
 *         "/\n sensors */",
 *         "/\n sensors */"
 *       ]
 *     },
 *     {
 *       "key_id": 447,
 *       "key_text": "seriale_rs232",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SERIALE_RS232",
 *       "values": [
 *         "📡 Seriale RS232",
 *         "📡 RS232 Serial",
 *         "📡 RS232 Serial",
 *         "📡 RS232 Serial",
 *         "📡 RS232 Serial"
 *       ]
 *     },
 *     {
 *       "key_id": 448,
 *       "key_text": "seriale_rs485",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SERIALE_RS485",
 *       "values": [
 *         "📡 Seriale RS485",
 *         "📡 RS485 Serial",
 *         "📡 RS485 Serial",
 *         "📡 RS485 Serial",
 *         "📡 RS485 Serial"
 *       ]
 *     },
 *     {
 *       "key_id": 449,
 *       "key_text": "span_canale_span",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SPAN_CANALE_SPAN",
 *       "values": [
 *         "<span>Canale:</span>",
 *         "\nChannel:\n",
 *         "\nChannel:\n",
 *         "\nChannel:\n",
 *         "\nChannel:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 450,
 *       "key_text": "span_duty_span_input_type_number_id_pwm_duty_value_50_min_0_max_",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SPAN_DUTY_SPAN_INPUT_TYPE_NUMBER_ID_PWM_DUTY_VALUE_50_MIN_0_MAX",
 *       "values": [
 *         "<span>Duty (%):</span><input type='number' id='pwm_duty' value='50' min='0' max='100' style='width:60px'>",
 *         "\nDuty (%):\n",
 *         "\nDuty (%):\n",
 *         "\nDuty (%):\n",
 *         "\nDuty (%):\n"
 *       ]
 *     },
 *     {
 *       "key_id": 451,
 *       "key_text": "span_freq_hz_span_input_type_number_id_pwm_freq_value_1000_min_1",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SPAN_FREQ_HZ_SPAN_INPUT_TYPE_NUMBER_ID_PWM_FREQ_VALUE_1000_MIN_1",
 *       "values": [
 *         "<span>Freq (Hz):</span><input type='number' id='pwm_freq' value='1000' min='100' max='20000' style='width:80px'>",
 *         "\nFreq (Hz):\n",
 *         "\nFreq (Hz):\n",
 *         "\nFreq (Hz):\n",
 *         "\nFreq (Hz):\n"
 *       ]
 *     },
 *     {
 *       "key_id": 452,
 *       "key_text": "span_id_sd_mounted_status_style_font_weight_bold_span_div",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SPAN_ID_SD_MOUNTED_STATUS_STYLE_FONT_WEIGHT_BOLD_SPAN_DIV",
 *       "values": [
 *         "<span id='sd_mounted_status' style='font-weight:bold'>--</span></div>",
 *         "\n--\n",
 *         "\n--\n",
 *         "\n--\n",
 *         "\n--\n"
 *       ]
 *     },
 *     {
 *       "key_id": 453,
 *       "key_text": "span_luminosita_span_input_type_range_id_led_bright_min_0_max_10",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_SPAN_LUMINOSITA_SPAN_INPUT_TYPE_RANGE_ID_LED_BRIGHT_MIN_0_MAX_10",
 *       "values": [
 *         "<span>Luminosità:</span><input type='range' id='led_bright' min='0' max='100' value='50' oninput='updateLedRealtime()' style='width:100px'>",
 *         "\nBrightness:\n",
 *         "\nBrightness:\n",
 *         "\nBrightness:\n",
 *         "\nBrightness:\n"
 *       ]
 *     },
 *     {
 *       "key_id": 454,
 *       "key_text": "stato_gpio_in_lettura",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_STATO_GPIO_IN_LETTURA",
 *       "values": [
 *         "Stato GPIO in lettura...",
 *         "GPIO Status reading...",
 *         "GPIO Status reading...",
 *         "GPIO Status reading...",
 *         "GPIO Status reading..."
 *       ]
 *     },
 *     {
 *       "key_id": 455,
 *       "key_text": "stato_montaggio",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_STATO_MONTAGGIO",
 *       "values": [
 *         "Stato Montaggio:",
 *         "Mounting Status:",
 *         "Mounting Status:",
 *         "Mounting Status:",
 *         "Mounting Status:"
 *       ]
 *     },
 *     {
 *       "key_id": 456,
 *       "key_text": "striscia_led_ws2812",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_STRISCIA_LED_WS2812",
 *       "values": [
 *         "💡 Striscia LED (WS2812)",
 *         "💡 LED Strip (WS2812)",
 *         "💡 LED Strip (WS2812)",
 *         "💡 LED Strip (WS2812)",
 *         "💡 LED Strip (WS2812)"
 *       ]
 *     },
 *     {
 *       "key_id": 457,
 *       "key_text": "test_loopback_0x55_0xaa_0x01_0x07",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEST_LOOPBACK_0X55_0XAA_0X01_0X07",
 *       "values": [
 *         "Test Loopback: 0x55, 0xAA, 0x01, 0x07",
 *         "Test Loopback: 0x55, 0xAA, 0x01, 0x07",
 *         "Test Loopback: 0x55, 0xAA, 0x01, 0x07",
 *         "Test Loopback: 0x55, 0xAA, 0x01, 0x07",
 *         "Test Loopback: 0x55, 0xAA, 0x01, 0x07"
 *       ]
 *     },
 *     {
 *       "key_id": 458,
 *       "key_text": "test_loopback_echo",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEST_LOOPBACK_ECHO",
 *       "values": [
 *         "Test Loopback/Echo",
 *         "Test Loopback/Echo",
 *         "Test Loopback/Echo",
 *         "Test Loopback/Echo",
 *         "Test Loopback/Echo"
 *       ]
 *     },
 *     {
 *       "key_id": 459,
 *       "key_text": "test_loopback_invia_pacchetto_di_prova",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEST_LOOPBACK_INVIA_PACCHETTO_DI_PROVA",
 *       "values": [
 *         "Test Loopback: invia pacchetto di prova",
 *         "Test Loopback: send test packet",
 *         "Test Loopback: send test packet",
 *         "Test Loopback: send test packet",
 *         "Test Loopback: send test packet"
 *       ]
 *     },
 *     {
 *       "key_id": 460,
 *       "key_text": "text",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEXT",
 *       "values": [
 *         "TEXT",
 *         "TEXT",
 *         "TEXT",
 *         "TEXT",
 *         "TEXT"
 *       ]
 *     },
 *     {
 *       "key_id": 461,
 *       "key_text": "text_2",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_TEXT_2",
 *       "values": [
 *         "' + text + '",
 *         "' + text + '",
 *         "' + text + '",
 *         "' + text + '",
 *         "' + text + '"
 *       ]
 *     },
 *     {
 *       "key_id": 462,
 *       "key_text": "ultime_letture",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ULTIME_LETTURE",
 *       "values": [
 *         "Ultime Letture",
 *         "Last Readings",
 *         "Last Readings",
 *         "Last Readings",
 *         "Last Readings"
 *       ]
 *     },
 *     {
 *       "key_id": 463,
 *       "key_text": "ultimo_errore",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_ULTIMO_ERRORE",
 *       "values": [
 *         "Ultimo Errore:",
 *         "Last Error:",
 *         "Last Error:",
 *         "Last Error:",
 *         "Last Error:"
 *       ]
 *     },
 *     {
 *       "key_id": 464,
 *       "key_text": "uscite_chip_0x43",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_USCITE_CHIP_0X43",
 *       "values": [
 *         "Uscite (Chip 0x43)",
 *         "Outputs (Chip 0x43)",
 *         "Outputs (Chip 0x43)",
 *         "Outputs (Chip 0x43)",
 *         "Outputs (Chip 0x43)"
 *       ]
 *     },
 *     {
 *       "key_id": 465,
 *       "key_text": "valori_correnti",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_VALORI_CORRENTI",
 *       "values": [
 *         "Valori Correnti:",
 *         "Current Values:",
 *         "Current Values:",
 *         "Current Values:",
 *         "Current Values:"
 *       ]
 *     },
 *     {
 *       "key_id": 466,
 *       "key_text": "web_ui_test_page",
 *       "scope_id": 9,
 *       "scope_text": "p_test",
 *       "symbol": "P_TEST_WEB_UI_TEST_PAGE",
 *       "values": [
 *         "WEB_UI_TEST_PAGE",
 *         "WEB_UI_TEST_PAGE",
 *         "WEB_UI_TEST_PAGE",
 *         "WEB_UI_TEST_PAGE",
 *         "WEB_UI_TEST_PAGE"
 *       ]
 *     }
 *   ],
 *   "languages": [
 *     "it",
 *     "en",
 *     "de",
 *     "fr",
 *     "es"
 *   ]
 * }
 * I18N_LANGUAGE_MODELS_DATA_END */

