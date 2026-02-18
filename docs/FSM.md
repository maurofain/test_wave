1. Stato 0: idle
   creiamo una pagina web /emulator , con un button perr entrare in home page prima di API, per emulare l'operatività del pannello utente. 
la pagina deve avere : a sx una colonna con 8 tasti - In attesa di eventi esterni, la macchina mantiene operative le connessioni di rete e monitora tutti i device
   - Il display mostra uno slideshow con immagini memorizzate in locale
   - possibili eventi che portano la macchina allo stato 1:
     - tocco del touchscreen
     - pressione tasti
     - inserimento monete/gettoni/tessera
     - il sistema controlla lo stato dei sistemi di pagamento (monete, gettoni, tessera, qrcode) ed attiva solo quelli operativi
2. Stato 1: credit. Visualizzazione dell'interfaccia di gestione credito
   - ogni sessione gestisce un credito per i servizi, inizialmente 0
   - dopo un tempo di inattività pari a un parametro SPLASHSCREENTIME la macchina passa al stato 0
   - se sono state inserite monete/gettoni vanno ad aumentare il credito
   - se è stata presentata una tessera si acquisisce il credito 
   - se è stato letto un qrcode, si interroga il server e si ricava il credito disponibile
   - quando il credito è superiore alla soglia minima si attiva la visualizzazione dei programmi disponibili
   - la transizione da Stato 1 a Stato 2 avviene con la scelta del programma, che si avvia immediatamente
   - l'utente può annullare l'operazione: se sono state utilizzate monete/gettoni le restituisce, se è credito elettronico non viene addebitato
3. Stato 2: running. Alla selezione del programma parte il countdown
   - viene visualizzato sullo schermo il credito residuo e rappresentato visivamente tramite la stripe LED. 
   - durante il running l'utente può inserire ulteriori monete o gettoni
   - timeout e programma sono allineati durante il running
   - L'esecuzione del programma può essere sospesa dal touchscreen o dalla tastiera (con lo stesso tasto usato per selezionare il programma) per un tempo massimo definito nei parametri
   - in sospensione timeout e programma restano allineati per tutto il tempo massimo di sospensione; scaduto tale tempo il countdown riprende ma la macchina resta in attesa della pressione del tasto programma per far ripartire il programma
   - Il programma termina su azione sul tasto o touchscreen o alla fine del credito e ritorna in stato 1
   - al termine la macchina addebita l'importo del programma trattenendo le monete o scalando l'importo tramite comunicazione con il server
   - tutte le operazioni vanno loggate
   











