// Header per accesso a icone embed nel firmware
#ifndef EMBEDDED_ICONS_H
#define EMBEDDED_ICONS_H

#include <stdbool.h>

// Restituisce un puntatore alla risorsa embedded per indice (0..3) e stato.
// L'oggetto restituito e' un buffer contenente l'immagine in formato supportato da LVGL.
const void *get_embedded_icon_src(int index, bool ok);

#endif // EMBEDDED_ICONS_H
