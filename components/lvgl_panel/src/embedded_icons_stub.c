// Stub che fornisce get_embedded_icon_src quando non sono state generate icone embed
#include "embedded_icons.h"
#include <stddef.h>

const void *get_embedded_icon_src(int index, bool ok)
{
    (void)index;
    (void)ok;
    return NULL; // nessuna icona embedded di default
}
