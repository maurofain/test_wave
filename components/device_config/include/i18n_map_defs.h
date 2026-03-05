#pragma once
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Auto-generated from i18n_it.map.json — do not edit manually. */
/* All 5 language map.json files were byte-for-byte identical.    */

#define I18N_SCOPE_COUNT 9
#define I18N_KEY_COUNT   476

#define I18N_SCOPE_HEADER              1
#define I18N_SCOPE_LVGL                2
#define I18N_SCOPE_NAV                 3
#define I18N_SCOPE_P_CONFIG            4
#define I18N_SCOPE_P_EMULATOR          5
#define I18N_SCOPE_P_LOGS              6
#define I18N_SCOPE_P_PROGRAMS          7
#define I18N_SCOPE_P_RUNTIME           8
#define I18N_SCOPE_P_TEST              9

#define I18N_KEY_TIME_NOT_SET                    1
#define I18N_KEY_TIME_NOT_AVAILABLE              2
#define I18N_KEY_CONFIG                          3
#define I18N_KEY_EMULATOR                        4
#define I18N_KEY_HOME                            5
#define I18N_KEY_LOGS                            6
#define I18N_KEY_OTA                             7
#define I18N_KEY_STATS                           8
#define I18N_KEY_TASKS                           9
#define I18N_KEY_TEST                            10
#define I18N_KEY_ABILITATO                       11
#define I18N_KEY_AGGIORNA                        12
#define I18N_KEY_AGGIORNA_DATI                   13
#define I18N_KEY_APRI_EDITOR_PROGRAMMI           14
#define I18N_KEY_ATTENDERE_IL_RIAVVIO            15
#define I18N_KEY_BACKUP_CONFIG                   16
#define I18N_KEY_BASE_SERVER_URL                 17
#define I18N_KEY_BAUDRATE                        18
#define I18N_KEY_BUFFER_RX                       19
#define I18N_KEY_BUFFER_TX                       20
#define I18N_KEY_CARICA_FIRMWARE                 21
#define I18N_KEY_CONFERMA_NUOVA_PASSWORD         22
#define I18N_KEY_DATA_BITS                       23
#define I18N_KEY_DHCP                            24
#define I18N_KEY_DISPLAY                         25
#define I18N_KEY_DISPLAY_ABILITATO               26
#define I18N_KEY_DUAL_PID_OPZIONALE              27
#define I18N_KEY_ETHERNET                        28
#define I18N_KEY_GATEWAY                         29
#define I18N_KEY_GESTIONE_PREZZI_DURATA_E_RELAY_PER_PROGRAMMA 30
#define I18N_KEY_GPIO_33                         31
#define I18N_KEY_GPIO_AUSILIARIO_GPIO33          32
#define I18N_KEY_I_O_EXPANDER                    33
#define I18N_KEY_IDENTITA_DISPOSITIVO            34
#define I18N_KEY_INDIRIZZO_IP                    35
#define I18N_KEY_INPUT_FLOAT                     36
#define I18N_KEY_INPUT_PULL_DOWN                 37
#define I18N_KEY_INPUT_PULL_UP                   38
#define I18N_KEY_ITALIANO_IT                     39
#define I18N_KEY_LED_STRIP_WS2812                40
#define I18N_KEY_LINGUA_UI                       41
#define I18N_KEY_LOGGING_REMOTO                  42
#define I18N_KEY_LUMINOSITA_LCD                  43
#define I18N_KEY_MDB_CONFIGURATION               44
#define I18N_KEY_MDB_ENGINE                      45
#define I18N_KEY_MODALITA                        46
#define I18N_KEY_MODIFICA_PASSWORD_RICHIESTA_PER_EMULATOR_E_REBOOT_FACTORY 47
#define I18N_KEY_NOME_DISPOSITIVO                48
#define I18N_KEY_NTP                             49
#define I18N_KEY_NTP_ABILITATO                   50
#define I18N_KEY_NUMERO_DI_LED                   51
#define I18N_KEY_NUOVA_PASSWORD                  52
#define I18N_KEY_OFFSET_FUSO_ORARIO_ORE          53
#define I18N_KEY_OUTPUT                          54
#define I18N_KEY_PARITA_0_NONE_1_ODD_2_EVEN      55
#define I18N_KEY_PASSWORD                        56
#define I18N_KEY_PASSWORD_ATTUALE                57
#define I18N_KEY_PASSWORD_BOOT_EMULATORE         58
#define I18N_KEY_PERIFERICHE_HARDWARE            59
#define I18N_KEY_PID                             60
#define I18N_KEY_PORTA_UDP                       61
#define I18N_KEY_PWM_CANALE_1                    62
#define I18N_KEY_PWM_CANALE_2                    63
#define I18N_KEY_REBOOT_IN_APP_LAST              64
#define I18N_KEY_REBOOT_IN_FACTORY_MODE          65
#define I18N_KEY_REBOOT_IN_OTA0                  66
#define I18N_KEY_REBOOT_IN_OTA1                  67
#define I18N_KEY_REBOOT_IN_PRODUCTION_MODE       68
#define I18N_KEY_RESET_FABBRICA                  69
#define I18N_KEY_RS232_CONFIGURATION             70
#define I18N_KEY_RS485_CONFIGURATION             71
#define I18N_KEY_SALVA_CONFIGURAZIONE            72
#define I18N_KEY_SALVA_PASSWORD_BOOT             73
#define I18N_KEY_SCANNER_USB_CDC                 74
#define I18N_KEY_SCHEDA_SD                       75
#define I18N_KEY_SENSORE_TEMPERATURA             76
#define I18N_KEY_SERVER_ABILITATO                77
#define I18N_KEY_SERVER_NTP_1                    78
#define I18N_KEY_SERVER_NTP_2                    79
#define I18N_KEY_SERVER_PASSWORD_MD5             80
#define I18N_KEY_SERVER_REMOTO                   81
#define I18N_KEY_SERVER_SERIAL                   82
#define I18N_KEY_SSID                            83
#define I18N_KEY_STATO_INIZIALE_SOLO_OUT         84
#define I18N_KEY_STOP_BITS_1_2                   85
#define I18N_KEY_SUBNET_MASK                     86
#define I18N_KEY_TABELLA_PROGRAMMI               87
#define I18N_KEY_TAG_PORTE_SERIALI               88
#define I18N_KEY_UART_RS232                      89
#define I18N_KEY_UART_RS485                      90
#define I18N_KEY_USA_BROADCAST_UDP               91
#define I18N_KEY_VID                             92
#define I18N_KEY_WIFI_ABILITATO                  93
#define I18N_KEY_WIFI_STA                        94
#define I18N_KEY_ANNULLA                         95
#define I18N_KEY_CONTINUA                        96
#define I18N_KEY_CREDITO                         97
#define I18N_KEY_IN_PAUSA_00_00                  98
#define I18N_KEY_INSERIRE_LA_PASSWORD_PER_CONTINUARE 99
#define I18N_KEY_LAYOUT_OPERATIVO_PANNELLO_UTENTE_800X1280_A_SINISTRA_QUADRO_ELET 100
#define I18N_KEY_NESSUN_EVENTO_IN_CODA           101
#define I18N_KEY_PASSWORD_RICHIESTA              102
#define I18N_KEY_PROGRAMMA_1                     103
#define I18N_KEY_PROGRAMMA_2                     104
#define I18N_KEY_PROGRAMMA_3                     105
#define I18N_KEY_PROGRAMMA_4                     106
#define I18N_KEY_PROGRAMMA_5                     107
#define I18N_KEY_PROGRAMMA_6                     108
#define I18N_KEY_PROGRAMMA_7                     109
#define I18N_KEY_PROGRAMMA_8                     110
#define I18N_KEY_QUADRO_ELETTRICO                111
#define I18N_KEY_R1                              112
#define I18N_KEY_R10                             113
#define I18N_KEY_R2                              114
#define I18N_KEY_R3                              115
#define I18N_KEY_R4                              116
#define I18N_KEY_R5                              117
#define I18N_KEY_R6                              118
#define I18N_KEY_R7                              119
#define I18N_KEY_R8                              120
#define I18N_KEY_R9                              121
#define I18N_KEY_RELAY                           122
#define I18N_KEY_RICARICA_COIN                   123
#define I18N_KEY_STATO_CREDITO                   124
#define I18N_KEY_TEMPO_00_00                     125
#define I18N_KEY_APPLICA_FILTRO                  126
#define I18N_KEY_CONFIGURA_IL_LOGGING_REMOTO_NELLA_PAGINA 127
#define I18N_KEY_CONFIGURAZIONE                  128
#define I18N_KEY_DERE_I_NUOVI_LOG_APPLICA_LIVELLI 129
#define I18N_KEY_HTML                            130
#define I18N_KEY_I_LOG_VENGONO_RICEVUTI_VIA_UDP_DAL_SERVER_CONFIGURATO_AGGIORNA_L 131
#define I18N_KEY_IN_ATTESA_DI_LOG                132
#define I18N_KEY_ITEM_TAG                        133
#define I18N_KEY_L_T                             134
#define I18N_KEY_LOG_REMOTO_RICEVUTI             135
#define I18N_KEY_NESSUN_TAG_DISPONIBILE          136
#define I18N_KEY_PER_INIZIARE_A_RICEVERE_LOG     137
#define I18N_KEY_PULISCI_FILTRO                  138
#define I18N_KEY_400_BAD_REQUEST                 139
#define I18N_KEY_403_FORBIDDEN                   140
#define I18N_KEY_404_NON_TROVATO                 141
#define I18N_KEY_405_METHOD_NOT_ALLOWED          142
#define I18N_KEY_BUTTON_CLASS_BTN_SECONDARY_ONCLICK_LOADPROGRAMS_RICARICA_BUTTON 143
#define I18N_KEY_BUTTON_ONCLICK_SAVEPROGRAMS_SALVA_TABELLA_BUTTON 144
#define I18N_KEY_C_GET_CONFIG_PROGRAMS           145
#define I18N_KEY_CAMPI_CURRENT_PASSWORD_NEW_PASSWORD_OBBLIGATORI 146
#define I18N_KEY_CAMPI_RELAY_NUMBER_STATUS_DURATION_OBBLIGATORI 147
#define I18N_KEY_DIV                             148
#define I18N_KEY_DIV_CLASS_CONTAINER_DIV_CLASS_SECTION 149
#define I18N_KEY_DIV_DIV                         150
#define I18N_KEY_DIV_ID_STATUS_CLASS_STATUS_DIV  151
#define I18N_KEY_DURATA_S                        152
#define I18N_KEY_EDITOR_TABELLA_PROGRAMMI_FACTORY 153
#define I18N_KEY_ERRORE_LETTURA_PAYLOAD          154
#define I18N_KEY_ERRORE_VALIDAZIONE              155
#define I18N_KEY_H2_EDITOR_TABELLA_PROGRAMMI_FACTORY_H2 156
#define I18N_KEY_ID                              157
#define I18N_KEY_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI_PR 158
#define I18N_KEY_JSON_NON_VALIDO                 159
#define I18N_KEY_METHOD_NOT_ALLOWED              160
#define I18N_KEY_NOME                            161
#define I18N_KEY_NUOVA_PASSWORD_NON_VALIDA_O_ERRORE_SALVATAGGIO 162
#define I18N_KEY_P_IMPOSTA_NOME_ABILITAZIONE_PREZZO_DURATA_E_RELAY_MASK_PER_OGNI_ 163
#define I18N_KEY_PASSWORD_ATTUALE_NON_VALIDA     164
#define I18N_KEY_PAUSA_MAX_S                     165
#define I18N_KEY_PAUSE_ALL_SECONDS               166
#define I18N_KEY_SET_PAUSE_ALL                   167
#define I18N_KEY_PAYLOAD_NON_VALIDO              168
#define I18N_KEY_PREZZO                          169
#define I18N_KEY_RELAY_MASK                      170
#define I18N_KEY_RELAY_NUMBER_FUORI_RANGE        171
#define I18N_KEY_RETURN                          172
#define I18N_KEY_RETURN_TR                       173
#define I18N_KEY_RICARICA                        174
#define I18N_KEY_SALVA_TABELLA                   175
#define I18N_KEY_SCRIPT_BODY_HTML                176
#define I18N_KEY_TABLE_THEAD_TR_TH_ID_TH_TH_NOME_TH_TH_ABILITATO_TH_TH_PREZZO_TH_ 177
#define I18N_KEY_WEB_UI_PROGRAMS_PAGE            178
#define I18N_KEY_ACTIVITY                        179
#define I18N_KEY_AGGIORNA_PAGINA                 180
#define I18N_KEY_AGGIUNGI_TASK                   181
#define I18N_KEY_AMBIENTE                        182
#define I18N_KEY_API                             183
#define I18N_KEY_API_ENDPOINTS                   184
#define I18N_KEY_APP_LAST                        185
#define I18N_KEY_APPLICA                         186
#define I18N_KEY_APPLY_TASKS                     187
#define I18N_KEY_AUTHENTICATE_REMOTE             188
#define I18N_KEY_BACKUP_CONFIGURATION            189
#define I18N_KEY_BENVENUTI_NELL_INTERFACCIA_DI_CONFIGURAZIONE_E_TEST 190
#define I18N_KEY_CARICAMENTO                     191
#define I18N_KEY_CONFIGURAZIONE_TASK             192
#define I18N_KEY_COPIA                           193
#define I18N_KEY_CORE                            194
#define I18N_KEY_CREDITO_ACCUMULATO              195
#define I18N_KEY_CURRENT_CONFIGURATION           196
#define I18N_KEY_DESCRIPTION                     197
#define I18N_KEY_DEVICE_STATUS_JSON              198
#define I18N_KEY_EDITOR_CSV                      199
#define I18N_KEY_EMULATORE                       200
#define I18N_KEY_FACTORY                         201
#define I18N_KEY_FACTORY_RESET                   202
#define I18N_KEY_FETCH_FIRMWARE                  203
#define I18N_KEY_FETCH_IMAGES                    204
#define I18N_KEY_FETCH_TRANSLATIONS              205
#define I18N_KEY_FIRMWARE                        206
#define I18N_KEY_FORCE_NTP_SYNC                  207
#define I18N_KEY_GET                             208
#define I18N_KEY_GET_CUSTOMERS                   209
#define I18N_KEY_GET_OPERATORS                   210
#define I18N_KEY_GET_REMOTE_CONFIG               211
#define I18N_KEY_GETTONIERA                      212
#define I18N_KEY_HEADER                          213
#define I18N_KEY_HTTP_SERVICES                   214
#define I18N_KEY_INDIRIZZO_IP_ETHERNET           215
#define I18N_KEY_INDIRIZZO_IP_WIFI_AP            216
#define I18N_KEY_INDIRIZZO_IP_WIFI_STA           217
#define I18N_KEY_INFORMAZIONI                    218
#define I18N_KEY_INVIA                           219
#define I18N_KEY_IS_JWT                          220
#define I18N_KEY_KEEPALIVE                       221
#define I18N_KEY_LED_WS2812                      222
#define I18N_KEY_LOG_LEVELS                      223
#define I18N_KEY_LOGIN_CHIAMATE_HTTP             224
#define I18N_KEY_MDB_STATUS                      225
#define I18N_KEY_METHOD                          226
#define I18N_KEY_OFFLINE_PAYMENT                 227
#define I18N_KEY_OTA0                            228
#define I18N_KEY_OTA1                            229
#define I18N_KEY_PARTIZIONE_AL_BOOT              230
#define I18N_KEY_PARTIZIONE_CORRENTE             231
#define I18N_KEY_PASSWORD_MD5                    232
#define I18N_KEY_PAYLOAD                         233
#define I18N_KEY_PAYMENT_REQUEST                 234
#define I18N_KEY_PERIODO_MS                      235
#define I18N_KEY_POST                            236
#define I18N_KEY_POST_API_ACTIVITY               237
#define I18N_KEY_POST_API_GETCONFIG              238
#define I18N_KEY_POST_API_GETCUSTOMERS           239
#define I18N_KEY_POST_API_GETFIRMWARE            240
#define I18N_KEY_POST_API_GETIMAGES              241
#define I18N_KEY_POST_API_GETOPERATORS           242
#define I18N_KEY_POST_API_GETTRANSLATIONS        243
#define I18N_KEY_POST_API_KEEPALIVE              244
#define I18N_KEY_POST_API_PAYMENT                245
#define I18N_KEY_POST_API_PAYMENTOFFLINE         246
#define I18N_KEY_POST_API_SERVICEUSED            247
#define I18N_KEY_PRIORITA                        248
#define I18N_KEY_PROFILO                         249
#define I18N_KEY_PWM_CHANNEL_1_2                 250
#define I18N_KEY_REBOOT                          251
#define I18N_KEY_RESTART_USB_HOST                252
#define I18N_KEY_RETE                            253
#define I18N_KEY_RICHIESTA                       254
#define I18N_KEY_RIMUOVI                         255
#define I18N_KEY_RISPOSTA                        256
#define I18N_KEY_RUN_INTERNAL_TESTS              257
#define I18N_KEY_SALVA                           258
#define I18N_KEY_SAVE_CONFIGURATION              259
#define I18N_KEY_SAVE_TASKS                      260
#define I18N_KEY_SD_CARD                         261
#define I18N_KEY_SERIAL                          262
#define I18N_KEY_SERVICE_USED                    263
#define I18N_KEY_SET_LOG_LEVEL                   264
#define I18N_KEY_SPAZIO_TOTALE                   265
#define I18N_KEY_SPAZIO_USATO                    266
#define I18N_KEY_STACK_WORDS                     267
#define I18N_KEY_STATISTICHE                     268
#define I18N_KEY_STATO                           269
#define I18N_KEY_STATO_DRIVER                    270
#define I18N_KEY_STATO_LOGICO_SM                 271
#define I18N_KEY_STORED_LOGS                     272
#define I18N_KEY_TASKS_CSV                       273
#define I18N_KEY_TEMPERATURA                     274
#define I18N_KEY_TEST_HARDWARE                   275
#define I18N_KEY_TOKEN                           276
#define I18N_KEY_UMIDITA                         277
#define I18N_KEY_UPDATE_OTA                      278
#define I18N_KEY_URI                             279
#define I18N_KEY_USB_ENUMERATE                   280
#define I18N_KEY_4800_BAUD_PIN_21_RX_20_TX       281
#define I18N_KEY_AVVIA_AGGIORNAMENTI_PERIODICI_PER_LA_SEZIONE_SCANNER 282
#define I18N_KEY_BACKUP                          283
#define I18N_KEY_BACKUP_JSON_CONFIG              284
#define I18N_KEY_BLINK_TUTTE_LE_USCITE_1HZ       285
#define I18N_KEY_BUTTON_ID_BTN_LED_MANUAL_ONCLICK_TOGGLELEDMANUAL_STYLE_BACKGROUN 286
#define I18N_KEY_BUTTON_ONCLICK_REFRESHSDSTATUS_STYLE_BACKGROUND_3498DB_AGGIORNA_ 287
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
#define I18N_KEY_C                               298
#define I18N_KEY_C_GET_TEST                      299
#define I18N_KEY_CANALE                          300
#define I18N_KEY_CCTALK                          301
#define I18N_KEY_CLEAR                           302
#define I18N_KEY_COLLAPSIBLE_SECTIONS_SUPPORT    303
#define I18N_KEY_CONFIG_JSON                     304
#define I18N_KEY_CONTROLLO_MANUALE               305
#define I18N_KEY_CONTROLLO_MANUALE_PWM           306
#define I18N_KEY_COPIATO                         307
#define I18N_KEY_DATO_BYTE_0_255                 308
#define I18N_KEY_DISPLAY_BRIGHTNESS_SLIDER_HANDLER_DEBOUNCED_PERSIST_AFTER_IDLE 309
#define I18N_KEY_DIV_CLASS_CONTAINER             310
#define I18N_KEY_DIV_CLASS_TEST_CONTROLS         311
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
#define I18N_KEY_DIV_CLASS_TEST_ITEM             329
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_BACKUP_JSON_CONFIG_SPAN 330
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_BLINK_TUTTE_LE_USCITE_ 331
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PATTERN_RAINBOW_SPAN 332
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM1_DUTY_CYCLE_SWEEP_ 333
#define I18N_KEY_DIV_CLASS_TEST_ITEM_SPAN_CLASS_TEST_LABEL_PWM2_DUTY_CYCLE_SWEEP_ 334
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
#define I18N_KEY_DIV_ID_SECTION_GPIO33_CLASS_SECTION_COLLAPSED_H2_GPIO_AUSILIARI_ 362
#define I18N_KEY_DIV_ID_SECTION_SCANNER_CLASS_SECTION_COLLAPSED_H2_SCANNER_USB_BA 363
#define I18N_KEY_DIV_ID_SHT_STATUS_CLASS_STATUS_BOX_PRONTO_PER_TEST_SHT40_DIV_DIV 364
#define I18N_KEY_DUTY                            365
#define I18N_KEY_EEPROM_24LC16                   366
#define I18N_KEY_ELENCA                          367
#define I18N_KEY_ELENCO_FILE_ROOT                368
#define I18N_KEY_ELSE_OUT                        369
#define I18N_KEY_ERRORE                          370
#define I18N_KEY_ERRORE_COMANDO_SCANNER          371
#define I18N_KEY_FORMATTAZIONE_FAT32             372
#define I18N_KEY_FORZA_TEST_LETTURA              373
#define I18N_KEY_FREQ_HZ                         374
#define I18N_KEY_GPIO_AUSILIARI_GPIO33           375
#define I18N_KEY_H                               376
#define I18N_KEY_H3_CONTROLLO_MANUALE_H3         377
#define I18N_KEY_H3_CONTROLLO_MANUALE_PWM_H3     378
#define I18N_KEY_HEX                             379
#define I18N_KEY_HEX_2                           380
#define I18N_KEY_HIDE_TEST_SECTIONS_WHEN_CORRESPONDING_PERIPHERAL_IS_DISABLED_IN_ 381
#define I18N_KEY_I2C_0X45                        382
#define I18N_KEY_IF_ISOUT_H                      383
#define I18N_KEY_INDIRIZZO_0_2047                384
#define I18N_KEY_INGRESSI_CHIP_0X44              385
#define I18N_KEY_INIT                            386
#define I18N_KEY_INIZIA_BLINK                    387
#define I18N_KEY_INIZIA_MANUALE                  388
#define I18N_KEY_INIZIA_RAINBOW                  389
#define I18N_KEY_INIZIA_SWEEP                    390
#define I18N_KEY_INIZIA_TEST                     391
#define I18N_KEY_INVIA_PACCHETTO_ES_0X01_0X02    392
#define I18N_KEY_INVIA_STRINGA                   393
#define I18N_KEY_INVIA_STRINGA_ES_0X55_0XAA      394
#define I18N_KEY_INVIA_STRINGA_HEX_ES_08_00      395
#define I18N_KEY_INVIO_BACKUP                    396
#define I18N_KEY_LEGGI                           397
#define I18N_KEY_LEGGI_JSON                      398
#define I18N_KEY_LETTURA_OK                      399
#define I18N_KEY_LIVE_UPDATE_TO_HARDWARE_SHORT_DEBOUNCE 400
#define I18N_KEY_LOG_OPERAZIONI_SD               401
#define I18N_KEY_LUMINOSITA                      402
#define I18N_KEY_MDB_MULTI_DROP_BUS              403
#define I18N_KEY_MONITOR                         404
#define I18N_KEY_NESSUNA_LETTURA_ANCORA          405
#define I18N_KEY_OFF                             406
#define I18N_KEY_OK                              407
#define I18N_KEY_ON                              408
#define I18N_KEY_OUT1_GPIO47                     409
#define I18N_KEY_OUT2_GPIO48                     410
#define I18N_KEY_PATTERN_RAINBOW                 411
#define I18N_KEY_PIN_12_A_17_B                   412
#define I18N_KEY_PIN_15_GPIO_5                   413
#define I18N_KEY_PIN_23_TX_35_RX                 414
#define I18N_KEY_PIN_32_37_I2C                   415
#define I18N_KEY_PIN_34_38                       416
#define I18N_KEY_PIN_39                          417
#define I18N_KEY_PIN_8_11                        418
#define I18N_KEY_POI_PULISCE_LATO_SERVER         419
#define I18N_KEY_PRONTO_PER_TEST_EEPROM          420
#define I18N_KEY_PRONTO_PER_TEST_I_O_EXPANDER    421
#define I18N_KEY_PRONTO_PER_TEST_LED             422
#define I18N_KEY_PRONTO_PER_TEST_MDB             423
#define I18N_KEY_PRONTO_PER_TEST_PWM             424
#define I18N_KEY_PRONTO_PER_TEST_SHT40           425
#define I18N_KEY_PULISCE_IMMEDIATAMENTE_L_AREA_DI_TESTO_LATO_CLIENT 426
#define I18N_KEY_PULISCI_FORMATTA                427
#define I18N_KEY_PWM                             428
#define I18N_KEY_PWM1_DUTY_CYCLE_SWEEP           429
#define I18N_KEY_PWM2_DUTY_CYCLE_SWEEP           430
#define I18N_KEY_SCANNER_OFF                     431
#define I18N_KEY_SCANNER_ON                      432
#define I18N_KEY_SCANNER_SETUP                   433
#define I18N_KEY_SCANNER_STATE                   434
#define I18N_KEY_SCANNER_UI_MOSTRA_LE_ULTIME_LETTURE_TAG_SCANNER_E_COMANDI_ON_OFF 435
#define I18N_KEY_SCANNER_USB_BARCODE_QR          436
#define I18N_KEY_SCHEDA_MICROSD                  437
#define I18N_KEY_SCHEDULE_PERSISTENCE_AFTER_5S_OF_INACTIVITY 438
#define I18N_KEY_SCRIVI                          439
#define I18N_KEY_SELECT_ID_CCTALK_MODE_ONCHANGE_CLEARSERIAL_CCTALK_OPTION_VALUE_H 440
#define I18N_KEY_SELECT_ID_MDB_MODE_ONCHANGE_CLEARSERIAL_MDB_OPTION_VALUE_HEX_HEX 441
#define I18N_KEY_SELECT_ID_RS232_MODE_ONCHANGE_CLEARSERIAL_RS232_OPTION_VALUE_HEX 442
#define I18N_KEY_SELECT_ID_RS485_MODE_ONCHANGE_CLEARSERIAL_RS485_OPTION_VALUE_HEX 443
#define I18N_KEY_SELEZIONA_COLORE                444
#define I18N_KEY_SENSORE_SHT40                   445
#define I18N_KEY_SENSORS                         446
#define I18N_KEY_SERIALE_RS232                   447
#define I18N_KEY_SERIALE_RS485                   448
#define I18N_KEY_SPAN_CANALE_SPAN                449
#define I18N_KEY_SPAN_DUTY_SPAN_INPUT_TYPE_NUMBER_ID_PWM_DUTY_VALUE_50_MIN_0_MAX_ 450
#define I18N_KEY_SPAN_FREQ_HZ_SPAN_INPUT_TYPE_NUMBER_ID_PWM_FREQ_VALUE_1000_MIN_1 451
#define I18N_KEY_SPAN_ID_SD_MOUNTED_STATUS_STYLE_FONT_WEIGHT_BOLD_SPAN_DIV 452
#define I18N_KEY_SPAN_LUMINOSITA_SPAN_INPUT_TYPE_RANGE_ID_LED_BRIGHT_MIN_0_MAX_10 453
#define I18N_KEY_STATO_GPIO_IN_LETTURA           454
#define I18N_KEY_STATO_MONTAGGIO                 455
#define I18N_KEY_STRISCIA_LED_WS2812             456
#define I18N_KEY_TEST_LOOPBACK_0X55_0XAA_0X01_0X07 457
#define I18N_KEY_TEST_LOOPBACK_ECHO              458
#define I18N_KEY_TEST_LOOPBACK_INVIA_PACCHETTO_DI_PROVA 459
#define I18N_KEY_TEXT                            460
#define I18N_KEY_TEXT_2                          461
#define I18N_KEY_ULTIME_LETTURE                  462
#define I18N_KEY_ULTIMO_ERRORE                   463
#define I18N_KEY_USCITE_CHIP_0X43                464
#define I18N_KEY_VALORI_CORRENTI                 465
#define I18N_KEY_WEB_UI_TEST_PAGE                466
#define I18N_KEY_OUT_OF_SERVICE_TITLE            467
#define I18N_KEY_OUT_OF_SERVICE_SUB              468
#define I18N_KEY_CREDIT_LABEL                    469
#define I18N_KEY_ELAPSED_FMT                     470
#define I18N_KEY_PAUSE_FMT                       471
#define I18N_KEY_PROGRAM_BTN_FMT                 472
#define I18N_KEY_PROGRAMMA_9                     473
#define I18N_KEY_PROGRAMMA_10                    474
#define I18N_KEY_LINGUA_PANNELLO_UTENTE_LCD      475
#define I18N_KEY_LINGUA_BACKEND_WEB_UI_SERVER    476

static const char * const s_i18n_scope_names[] = {
    NULL, /* [0] unused */
    "header", /* [1] */
    "lvgl", /* [2] */
    "nav", /* [3] */
    "p_config", /* [4] */
    "p_emulator", /* [5] */
    "p_logs", /* [6] */
    "p_programs", /* [7] */
    "p_runtime", /* [8] */
    "p_test" /* [9] */
};

static const char * const s_i18n_key_names[] = {
    NULL, /* [0] unused */
    "time_not_set", /* [1] */
    "time_not_available", /* [2] */
    "config", /* [3] */
    "emulator", /* [4] */
    "home", /* [5] */
    "logs", /* [6] */
    "ota", /* [7] */
    "stats", /* [8] */
    "tasks", /* [9] */
    "test", /* [10] */
    "abilitato", /* [11] */
    "aggiorna", /* [12] */
    "aggiorna_dati", /* [13] */
    "apri_editor_programmi", /* [14] */
    "attendere_il_riavvio", /* [15] */
    "backup_config", /* [16] */
    "base_server_url", /* [17] */
    "baudrate", /* [18] */
    "buffer_rx", /* [19] */
    "buffer_tx", /* [20] */
    "carica_firmware", /* [21] */
    "conferma_nuova_password", /* [22] */
    "data_bits", /* [23] */
    "dhcp", /* [24] */
    "display", /* [25] */
    "display_abilitato", /* [26] */
    "dual_pid_opzionale", /* [27] */
    "ethernet", /* [28] */
    "gateway", /* [29] */
    "gestione_prezzi_durata_e_relay_per_programma", /* [30] */
    "gpio_33", /* [31] */
    "gpio_ausiliario_gpio33", /* [32] */
    "i_o_expander", /* [33] */
    "identita_dispositivo", /* [34] */
    "indirizzo_ip", /* [35] */
    "input_float", /* [36] */
    "input_pull_down", /* [37] */
    "input_pull_up", /* [38] */
    "italiano_it", /* [39] */
    "led_strip_ws2812", /* [40] */
    "lingua_ui", /* [41] */
    "logging_remoto", /* [42] */
    "luminosita_lcd", /* [43] */
    "mdb_configuration", /* [44] */
    "mdb_engine", /* [45] */
    "modalita", /* [46] */
    "modifica_password_richiesta_per_emulator_e_reboot_factory", /* [47] */
    "nome_dispositivo", /* [48] */
    "ntp", /* [49] */
    "ntp_abilitato", /* [50] */
    "numero_di_led", /* [51] */
    "nuova_password", /* [52] */
    "offset_fuso_orario_ore", /* [53] */
    "output", /* [54] */
    "parita_0_none_1_odd_2_even", /* [55] */
    "password", /* [56] */
    "password_attuale", /* [57] */
    "password_boot_emulatore", /* [58] */
    "periferiche_hardware", /* [59] */
    "pid", /* [60] */
    "porta_udp", /* [61] */
    "pwm_canale_1", /* [62] */
    "pwm_canale_2", /* [63] */
    "reboot_in_app_last", /* [64] */
    "reboot_in_factory_mode", /* [65] */
    "reboot_in_ota0", /* [66] */
    "reboot_in_ota1", /* [67] */
    "reboot_in_production_mode", /* [68] */
    "reset_fabbrica", /* [69] */
    "rs232_configuration", /* [70] */
    "rs485_configuration", /* [71] */
    "salva_configurazione", /* [72] */
    "salva_password_boot", /* [73] */
    "scanner_usb_cdc", /* [74] */
    "scheda_sd", /* [75] */
    "sensore_temperatura", /* [76] */
    "server_abilitato", /* [77] */
    "server_ntp_1", /* [78] */
    "server_ntp_2", /* [79] */
    "server_password_md5", /* [80] */
    "server_remoto", /* [81] */
    "server_serial", /* [82] */
    "ssid", /* [83] */
    "stato_iniziale_solo_out", /* [84] */
    "stop_bits_1_2", /* [85] */
    "subnet_mask", /* [86] */
    "tabella_programmi", /* [87] */
    "tag_porte_seriali", /* [88] */
    "uart_rs232", /* [89] */
    "uart_rs485", /* [90] */
    "usa_broadcast_udp", /* [91] */
    "vid", /* [92] */
    "wifi_abilitato", /* [93] */
    "wifi_sta", /* [94] */
    "annulla", /* [95] */
    "continua", /* [96] */
    "credito", /* [97] */
    "in_pausa_00_00", /* [98] */
    "inserire_la_password_per_continuare", /* [99] */
    "layout_operativo_pannello_utente_800x1280_a_sinistra_quadro_elet", /* [100] */
    "nessun_evento_in_coda", /* [101] */
    "password_richiesta", /* [102] */
    "programma_1", /* [103] */
    "programma_2", /* [104] */
    "programma_3", /* [105] */
    "programma_4", /* [106] */
    "programma_5", /* [107] */
    "programma_6", /* [108] */
    "programma_7", /* [109] */
    "programma_8", /* [110] */
    "quadro_elettrico", /* [111] */
    "r1", /* [112] */
    "r10", /* [113] */
    "r2", /* [114] */
    "r3", /* [115] */
    "r4", /* [116] */
    "r5", /* [117] */
    "r6", /* [118] */
    "r7", /* [119] */
    "r8", /* [120] */
    "r9", /* [121] */
    "relay", /* [122] */
    "ricarica_coin", /* [123] */
    "stato_credito", /* [124] */
    "tempo_00_00", /* [125] */
    "applica_filtro", /* [126] */
    "configura_il_logging_remoto_nella_pagina", /* [127] */
    "configurazione", /* [128] */
    "dere_i_nuovi_log_applica_livelli", /* [129] */
    "html", /* [130] */
    "i_log_vengono_ricevuti_via_udp_dal_server_configurato_aggiorna_l", /* [131] */
    "in_attesa_di_log", /* [132] */
    "item_tag", /* [133] */
    "l_t", /* [134] */
    "log_remoto_ricevuti", /* [135] */
    "nessun_tag_disponibile", /* [136] */
    "per_iniziare_a_ricevere_log", /* [137] */
    "pulisci_filtro", /* [138] */
    "400_bad_request", /* [139] */
    "403_forbidden", /* [140] */
    "404_non_trovato", /* [141] */
    "405_method_not_allowed", /* [142] */
    "button_class_btn_secondary_onclick_loadprograms_ricarica_button", /* [143] */
    "button_onclick_saveprograms_salva_tabella_button", /* [144] */
    "c_get_config_programs", /* [145] */
    "campi_current_password_new_password_obbligatori", /* [146] */
    "campi_relay_number_status_duration_obbligatori", /* [147] */
    "div", /* [148] */
    "div_class_container_div_class_section", /* [149] */
    "div_div", /* [150] */
    "div_id_status_class_status_div", /* [151] */
    "durata_s", /* [152] */
    "editor_tabella_programmi_factory", /* [153] */
    "errore_lettura_payload", /* [154] */
    "errore_validazione", /* [155] */
    "h2_editor_tabella_programmi_factory_h2", /* [156] */
    "id", /* [157] */
    "imposta_nome_abilitazione_prezzo_durata_e_relay_mask_per_ogni_pr", /* [158] */
    "json_non_valido", /* [159] */
    "method_not_allowed", /* [160] */
    "nome", /* [161] */
    "nuova_password_non_valida_o_errore_salvataggio", /* [162] */
    "p_imposta_nome_abilitazione_prezzo_durata_e_relay_mask_per_ogni_", /* [163] */
    "password_attuale_non_valida", /* [164] */
    "pausa_max_s", /* [165] */
    "pause_all_seconds", /* [166] */
    "set_pause_all", /* [167] */
    "payload_non_valido", /* [168] */
    "prezzo", /* [169] */
    "relay_mask", /* [170] */
    "relay_number_fuori_range", /* [171] */
    "return", /* [172] */
    "return_tr", /* [173] */
    "ricarica", /* [174] */
    "salva_tabella", /* [175] */
    "script_body_html", /* [176] */
    "table_thead_tr_th_id_th_th_nome_th_th_abilitato_th_th_prezzo_th_", /* [177] */
    "web_ui_programs_page", /* [178] */
    "activity", /* [179] */
    "aggiorna_pagina", /* [180] */
    "aggiungi_task", /* [181] */
    "ambiente", /* [182] */
    "api", /* [183] */
    "api_endpoints", /* [184] */
    "app_last", /* [185] */
    "applica", /* [186] */
    "apply_tasks", /* [187] */
    "authenticate_remote", /* [188] */
    "backup_configuration", /* [189] */
    "benvenuti_nell_interfaccia_di_configurazione_e_test", /* [190] */
    "caricamento", /* [191] */
    "configurazione_task", /* [192] */
    "copia", /* [193] */
    "core", /* [194] */
    "credito_accumulato", /* [195] */
    "current_configuration", /* [196] */
    "description", /* [197] */
    "device_status_json", /* [198] */
    "editor_csv", /* [199] */
    "emulatore", /* [200] */
    "factory", /* [201] */
    "factory_reset", /* [202] */
    "fetch_firmware", /* [203] */
    "fetch_images", /* [204] */
    "fetch_translations", /* [205] */
    "firmware", /* [206] */
    "force_ntp_sync", /* [207] */
    "get", /* [208] */
    "get_customers", /* [209] */
    "get_operators", /* [210] */
    "get_remote_config", /* [211] */
    "gettoniera", /* [212] */
    "header", /* [213] */
    "http_services", /* [214] */
    "indirizzo_ip_ethernet", /* [215] */
    "indirizzo_ip_wifi_ap", /* [216] */
    "indirizzo_ip_wifi_sta", /* [217] */
    "informazioni", /* [218] */
    "invia", /* [219] */
    "is_jwt", /* [220] */
    "keepalive", /* [221] */
    "led_ws2812", /* [222] */
    "log_levels", /* [223] */
    "login_chiamate_http", /* [224] */
    "mdb_status", /* [225] */
    "method", /* [226] */
    "offline_payment", /* [227] */
    "ota0", /* [228] */
    "ota1", /* [229] */
    "partizione_al_boot", /* [230] */
    "partizione_corrente", /* [231] */
    "password_md5", /* [232] */
    "payload", /* [233] */
    "payment_request", /* [234] */
    "periodo_ms", /* [235] */
    "post", /* [236] */
    "post_api_activity", /* [237] */
    "post_api_getconfig", /* [238] */
    "post_api_getcustomers", /* [239] */
    "post_api_getfirmware", /* [240] */
    "post_api_getimages", /* [241] */
    "post_api_getoperators", /* [242] */
    "post_api_gettranslations", /* [243] */
    "post_api_keepalive", /* [244] */
    "post_api_payment", /* [245] */
    "post_api_paymentoffline", /* [246] */
    "post_api_serviceused", /* [247] */
    "priorita", /* [248] */
    "profilo", /* [249] */
    "pwm_channel_1_2", /* [250] */
    "reboot", /* [251] */
    "restart_usb_host", /* [252] */
    "rete", /* [253] */
    "richiesta", /* [254] */
    "rimuovi", /* [255] */
    "risposta", /* [256] */
    "run_internal_tests", /* [257] */
    "salva", /* [258] */
    "save_configuration", /* [259] */
    "save_tasks", /* [260] */
    "sd_card", /* [261] */
    "serial", /* [262] */
    "service_used", /* [263] */
    "set_log_level", /* [264] */
    "spazio_totale", /* [265] */
    "spazio_usato", /* [266] */
    "stack_words", /* [267] */
    "statistiche", /* [268] */
    "stato", /* [269] */
    "stato_driver", /* [270] */
    "stato_logico_sm", /* [271] */
    "stored_logs", /* [272] */
    "tasks_csv", /* [273] */
    "temperatura", /* [274] */
    "test_hardware", /* [275] */
    "token", /* [276] */
    "umidita", /* [277] */
    "update_ota", /* [278] */
    "uri", /* [279] */
    "usb_enumerate", /* [280] */
    "4800_baud_pin_21_rx_20_tx", /* [281] */
    "avvia_aggiornamenti_periodici_per_la_sezione_scanner", /* [282] */
    "backup", /* [283] */
    "backup_json_config", /* [284] */
    "blink_tutte_le_uscite_1hz", /* [285] */
    "button_id_btn_led_manual_onclick_toggleledmanual_style_backgroun", /* [286] */
    "button_onclick_refreshsdstatus_style_background_3498db_aggiorna_", /* [287] */
    "button_onclick_runtest_sd_init_style_background_27ae60_init_butt", /* [288] */
    "button_onclick_runtest_sht_read_style_background_f39c12_forza_te", /* [289] */
    "button_onclick_setpwm_style_background_2980b9_applica_button", /* [290] */
    "button_onclick_testeeprom_read_json_style_background_9b59b6_legg", /* [291] */
    "button_onclick_testeeprom_read_style_background_3498db_leggi_but", /* [292] */
    "button_onclick_testeeprom_write_scrivi_button", /* [293] */
    "button_type_button_onclick_runscannercommand_scanner_off_class_b", /* [294] */
    "button_type_button_onclick_runscannercommand_scanner_on_style_ba", /* [295] */
    "button_type_button_onclick_runscannercommand_scanner_setup_style", /* [296] */
    "button_type_button_onclick_runscannercommand_scanner_state_style", /* [297] */
    "c", /* [298] */
    "c_get_test", /* [299] */
    "canale", /* [300] */
    "cctalk", /* [301] */
    "clear", /* [302] */
    "collapsible_sections_support", /* [303] */
    "config_json", /* [304] */
    "controllo_manuale", /* [305] */
    "controllo_manuale_pwm", /* [306] */
    "copiato", /* [307] */
    "dato_byte_0_255", /* [308] */
    "display_brightness_slider_handler_debounced_persist_after_idle", /* [309] */
    "div_class_container", /* [310] */
    "div_class_test_controls", /* [311] */
    "div_class_test_controls_button_id_btn_cctalk_onclick_togglesimpl", /* [312] */
    "div_class_test_controls_button_id_btn_ioexp_onclick_togglesimple", /* [313] */
    "div_class_test_controls_button_id_btn_led_rainbow_onclick_toggle", /* [314] */
    "div_class_test_controls_button_id_btn_mdb_onclick_togglesimplete", /* [315] */
    "div_class_test_controls_button_id_btn_pwm1_onclick_togglesimplet", /* [316] */
    "div_class_test_controls_button_id_btn_pwm2_onclick_togglesimplet", /* [317] */
    "div_class_test_controls_button_id_btn_rs232_onclick_togglesimple", /* [318] */
    "div_class_test_controls_button_id_btn_rs485_onclick_togglesimple", /* [319] */
    "div_class_test_controls_button_onclick_if_confirm_cancellare_tut", /* [320] */
    "div_class_test_controls_button_onclick_runconfigbackup_style_bac", /* [321] */
    "div_class_test_controls_button_onclick_runtest_sd_list_elenca_bu", /* [322] */
    "div_class_test_controls_input_type_number_id_eeprom_addr_value_0", /* [323] */
    "div_class_test_controls_input_type_number_id_eeprom_val_value_12", /* [324] */
    "div_class_test_controls_input_type_text_id_cctalk_input_placehol", /* [325] */
    "div_class_test_controls_input_type_text_id_mdb_input_placeholder", /* [326] */
    "div_class_test_controls_input_type_text_id_rs232_input_placehold", /* [327] */
    "div_class_test_controls_input_type_text_id_rs485_input_placehold", /* [328] */
    "div_class_test_item", /* [329] */
    "div_class_test_item_span_backup_json_config_span", /* [330] */
    "div_class_test_item_span_class_test_label_blink_tutte_le_uscite_", /* [331] */
    "div_class_test_item_span_class_test_label_pattern_rainbow_span", /* [332] */
    "div_class_test_item_span_class_test_label_pwm1_duty_cycle_sweep_", /* [333] */
    "div_class_test_item_span_class_test_label_pwm2_duty_cycle_sweep_", /* [334] */
    "div_class_test_item_span_class_test_label_stato_montaggio_span", /* [335] */
    "div_class_test_item_span_class_test_label_test_loopback_0x55_0xa", /* [336] */
    "div_class_test_item_span_class_test_label_test_loopback_echo_spa", /* [337] */
    "div_class_test_item_span_class_test_label_test_loopback_invia_pa", /* [338] */
    "div_class_test_item_span_class_test_label_ultime_letture_span", /* [339] */
    "div_class_test_item_span_class_test_label_ultimo_errore_span", /* [340] */
    "div_class_test_item_span_dato_byte_0_255_span", /* [341] */
    "div_class_test_item_span_elenco_file_root_span", /* [342] */
    "div_class_test_item_span_indirizzo_0_2047_span", /* [343] */
    "div_class_test_item_span_invia_pacchetto_es_0x01_0x02_span", /* [344] */
    "div_class_test_item_span_invia_stringa_es_0x55_0xaa_r_span", /* [345] */
    "div_class_test_item_span_invia_stringa_hex_es_08_00_span", /* [346] */
    "div_class_test_item_span_invia_stringa_span", /* [347] */
    "div_class_test_item_span_seleziona_colore_span", /* [348] */
    "div_class_test_item_span_valori_correnti_span", /* [349] */
    "div_id_cctalk_status_class_status_box_monitor_div_div", /* [350] */
    "div_id_eeprom_status_class_status_box_pronto_per_test_eeprom_div", /* [351] */
    "div_id_gpios_status_class_status_box_stato_gpio_in_lettura_div_d", /* [352] */
    "div_id_gpios_test_grid_caricamento_div", /* [353] */
    "div_id_ioexp_status_class_status_box_pronto_per_test_i_o_expande", /* [354] */
    "div_id_led_status_class_status_box_pronto_per_test_led_div_div", /* [355] */
    "div_id_mdb_status_class_status_box_pronto_per_test_mdb_div_div", /* [356] */
    "div_id_pwm_status_class_status_box_pronto_per_test_pwm_div_div", /* [357] */
    "div_id_rs232_status_class_status_box_monitor_div_div", /* [358] */
    "div_id_rs485_status_class_status_box_monitor_div_div", /* [359] */
    "div_id_scanner_status_class_status_box_nessuna_lettura_ancora_di", /* [360] */
    "div_id_sd_status_class_status_box_style_display_none_div_div", /* [361] */
    "div_id_section_gpio33_class_section_collapsed_h2_gpio_ausiliari_", /* [362] */
    "div_id_section_scanner_class_section_collapsed_h2_scanner_usb_ba", /* [363] */
    "div_id_sht_status_class_status_box_pronto_per_test_sht40_div_div", /* [364] */
    "duty", /* [365] */
    "eeprom_24lc16", /* [366] */
    "elenca", /* [367] */
    "elenco_file_root", /* [368] */
    "else_out", /* [369] */
    "errore", /* [370] */
    "errore_comando_scanner", /* [371] */
    "formattazione_fat32", /* [372] */
    "forza_test_lettura", /* [373] */
    "freq_hz", /* [374] */
    "gpio_ausiliari_gpio33", /* [375] */
    "h", /* [376] */
    "h3_controllo_manuale_h3", /* [377] */
    "h3_controllo_manuale_pwm_h3", /* [378] */
    "hex", /* [379] */
    "hex_2", /* [380] */
    "hide_test_sections_when_corresponding_peripheral_is_disabled_in_", /* [381] */
    "i2c_0x45", /* [382] */
    "if_isout_h", /* [383] */
    "indirizzo_0_2047", /* [384] */
    "ingressi_chip_0x44", /* [385] */
    "init", /* [386] */
    "inizia_blink", /* [387] */
    "inizia_manuale", /* [388] */
    "inizia_rainbow", /* [389] */
    "inizia_sweep", /* [390] */
    "inizia_test", /* [391] */
    "invia_pacchetto_es_0x01_0x02", /* [392] */
    "invia_stringa", /* [393] */
    "invia_stringa_es_0x55_0xaa", /* [394] */
    "invia_stringa_hex_es_08_00", /* [395] */
    "invio_backup", /* [396] */
    "leggi", /* [397] */
    "leggi_json", /* [398] */
    "lettura_ok", /* [399] */
    "live_update_to_hardware_short_debounce", /* [400] */
    "log_operazioni_sd", /* [401] */
    "luminosita", /* [402] */
    "mdb_multi_drop_bus", /* [403] */
    "monitor", /* [404] */
    "nessuna_lettura_ancora", /* [405] */
    "off", /* [406] */
    "ok", /* [407] */
    "on", /* [408] */
    "out1_gpio47", /* [409] */
    "out2_gpio48", /* [410] */
    "pattern_rainbow", /* [411] */
    "pin_12_a_17_b", /* [412] */
    "pin_15_gpio_5", /* [413] */
    "pin_23_tx_35_rx", /* [414] */
    "pin_32_37_i2c", /* [415] */
    "pin_34_38", /* [416] */
    "pin_39", /* [417] */
    "pin_8_11", /* [418] */
    "poi_pulisce_lato_server", /* [419] */
    "pronto_per_test_eeprom", /* [420] */
    "pronto_per_test_i_o_expander", /* [421] */
    "pronto_per_test_led", /* [422] */
    "pronto_per_test_mdb", /* [423] */
    "pronto_per_test_pwm", /* [424] */
    "pronto_per_test_sht40", /* [425] */
    "pulisce_immediatamente_l_area_di_testo_lato_client", /* [426] */
    "pulisci_formatta", /* [427] */
    "pwm", /* [428] */
    "pwm1_duty_cycle_sweep", /* [429] */
    "pwm2_duty_cycle_sweep", /* [430] */
    "scanner_off", /* [431] */
    "scanner_on", /* [432] */
    "scanner_setup", /* [433] */
    "scanner_state", /* [434] */
    "scanner_ui_mostra_le_ultime_letture_tag_scanner_e_comandi_on_off", /* [435] */
    "scanner_usb_barcode_qr", /* [436] */
    "scheda_microsd", /* [437] */
    "schedule_persistence_after_5s_of_inactivity", /* [438] */
    "scrivi", /* [439] */
    "select_id_cctalk_mode_onchange_clearserial_cctalk_option_value_h", /* [440] */
    "select_id_mdb_mode_onchange_clearserial_mdb_option_value_hex_hex", /* [441] */
    "select_id_rs232_mode_onchange_clearserial_rs232_option_value_hex", /* [442] */
    "select_id_rs485_mode_onchange_clearserial_rs485_option_value_hex", /* [443] */
    "seleziona_colore", /* [444] */
    "sensore_sht40", /* [445] */
    "sensors", /* [446] */
    "seriale_rs232", /* [447] */
    "seriale_rs485", /* [448] */
    "span_canale_span", /* [449] */
    "span_duty_span_input_type_number_id_pwm_duty_value_50_min_0_max_", /* [450] */
    "span_freq_hz_span_input_type_number_id_pwm_freq_value_1000_min_1", /* [451] */
    "span_id_sd_mounted_status_style_font_weight_bold_span_div", /* [452] */
    "span_luminosita_span_input_type_range_id_led_bright_min_0_max_10", /* [453] */
    "stato_gpio_in_lettura", /* [454] */
    "stato_montaggio", /* [455] */
    "striscia_led_ws2812", /* [456] */
    "test_loopback_0x55_0xaa_0x01_0x07", /* [457] */
    "test_loopback_echo", /* [458] */
    "test_loopback_invia_pacchetto_di_prova", /* [459] */
    "text", /* [460] */
    "text_2", /* [461] */
    "ultime_letture", /* [462] */
    "ultimo_errore", /* [463] */
    "uscite_chip_0x43", /* [464] */
    "valori_correnti", /* [465] */
    "web_ui_test_page", /* [466] */
    "out_of_service_title", /* [467] */
    "out_of_service_sub", /* [468] */
    "credit_label", /* [469] */
    "elapsed_fmt", /* [470] */
    "pause_fmt", /* [471] */
    "program_btn_fmt", /* [472] */
    "programma_9", /* [473] */
    "programma_10", /* [474] */
    "lingua_pannello_utente_lcd", /* [475] */
    "lingua_backend_web_ui_server" /* [476] */
};

static inline const char *i18n_scope_name(int id)
{
    return (id >= 1 && id <= I18N_SCOPE_COUNT) ? s_i18n_scope_names[id] : NULL;
}

static inline const char *i18n_key_name(int id)
{
    return (id >= 1 && id <= I18N_KEY_COUNT) ? s_i18n_key_names[id] : NULL;
}

/* Reverse lookups (O(n) linear scan on rodata arrays) */
static inline int i18n_scope_id(const char *name)
{
    if (!name) return 0;
    for (int i = 1; i <= I18N_SCOPE_COUNT; i++) {
        if (s_i18n_scope_names[i] && strcmp(s_i18n_scope_names[i], name) == 0) return i;
    }
    return 0;
}

static inline int i18n_key_id(const char *name)
{
    if (!name) return 0;
    for (int i = 1; i <= I18N_KEY_COUNT; i++) {
        if (s_i18n_key_names[i] && strcmp(s_i18n_key_names[i], name) == 0) return i;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
