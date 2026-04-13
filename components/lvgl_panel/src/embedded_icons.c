#include "embedded_icons.h"
#include "embedded_icons_images.h"

// Return embedded image pointer by index, or NULL
const void *get_embedded_icon_src(int index)
{
    switch(index) {
        case 0: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_CloudKo_png[];
        case 0: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_CloudOk_png[];
        case 1: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_CreditCardKo_png[];
        case 1: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_CreditCardOk_png[];
        case 2: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_MoneteKo_png[];
        case 2: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_MoneteOk3_png[];
        case 3: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_QrKo_png[];
        case 3: return (const void *)_home_mauro_1P_MicroHard_test_wave_data_icons_QrOk_png[];
        default: return NULL;
    }
}
