// Stub che fornisce get_embedded_icon_src quando non sono state generate icone embed
#include "embedded_icons.h"
#include <stddef.h>

const void *get_embedded_icon_src(int index)
{
    (void)index;
    return NULL; // nessuna icona embedded di default
}
