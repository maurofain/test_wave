// Header per accesso a icone embed nel firmware
#ifndef EMBEDDED_ICONS_H
#define EMBEDDED_ICONS_H

#include <stdbool.h>

// Restituisce un puntatore alla risorsa embedded per indice (0..3) e stato.
// 0=cloud, 1=credit card, 2=coin, 3=qr.
const void *get_embedded_icon_src(int index, bool ok);

#endif // EMBEDDED_ICONS_H
