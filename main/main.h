#pragma once

/**
 * @brief Invia la sequenza di inizializzazione CCTalk al dispositivo gettoniera.
 * 
 * Verifica che l'interfaccia CCTalk sia abilitata nella configurazione e invia
 * una sequenza di 4 comandi di inizializzazione al dispositivo (indirizzo 0x02).
 * Questa funzione va richiamata dopo la connessione ethernet e prima di ogni
 * caricamento della pagina di selezione programmi.
 * 
 * @return void Non restituisce alcun valore.
 */
void main_cctalk_send_initialization_sequence(void);
