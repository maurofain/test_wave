// Header per accesso a icone embed nel firmware
#ifndef EMBEDDED_ICONS_H
#define EMBEDDED_ICONS_H

#include <stdbool.h>

// Restituisce un puntatore alla risorsa embedded per indice (0..3), oppure NULL
// L'oggetto restituito è un buffer contenente l'immagine in formato supportato da LVGL
// (es. PNG binario convertito con LVGL o rle/raw). Se NULL usare il fallback su SPIFFS.
const void *get_embedded_icon_src(int index);

#endif // EMBEDDED_ICONS_H
