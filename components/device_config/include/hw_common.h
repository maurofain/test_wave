#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stato operativo generico di un componente hardware.
 *
 * Tutti i driver hardware devono esporre una funzione del tipo:
 *   hw_component_status_t <component>_get_status(void);
 *
 * Stati:
 *   HW_STATUS_DISABLED  — il componente è escluso dalla configurazione
 *   HW_STATUS_ENABLED   — abilitato ma non ancora operativo (init completato, nessun task attivo)
 *   HW_STATUS_OFFLINE   — task attivo ma la periferica non risponde
 *   HW_STATUS_ONLINE    — pienamente operativo
 */
typedef enum {
    HW_STATUS_DISABLED = 0,  /**< [C] Componente disabilitato in configurazione */
    HW_STATUS_ENABLED,       /**< [C] Abilitato, init completato, task non ancora attivo */
    HW_STATUS_OFFLINE,       /**< [C] Task attivo ma periferica non risponde */
    HW_STATUS_ONLINE,        /**< [C] Pienamente operativo */
} hw_component_status_t;

#ifdef __cplusplus
}
#endif
