#include <stdio.h>
#include "setup.h"
#include "cesi_types.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "grove_bme680.h"
#include "sd_logger.h"
#include "grove_choix_mode.h"

int main(void)
{
    Global_Init();
    choix_mode_init();

    while (1)
    {
        choix_mode_update();
    }
}