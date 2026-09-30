#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "sd_logger.h"
#include "grove_mode_maintenance.h"
#include "grove_mode_standard.h"

#define PERIODE_MAINTENANCE_MS 5000U

static uint32_t derniere_mesure = 0;
static int timeout_s = 0;

void mode_maintenance_init(int TIMEOUT)
{
    timeout_s = TIMEOUT;

    SDLogger_Unmount();
    printf("Mode maintenance : carte SD demontee, retrait possible\r\n");

    derniere_mesure = HAL_GetTick() - PERIODE_MAINTENANCE_MS;
}

void mode_maintenance_update(void)
{
    if ((HAL_GetTick() - derniere_mesure) >= PERIODE_MAINTENANCE_MS)
    {
        derniere_mesure = HAL_GetTick();
        mode_standard_acquisition(timeout_s, true, false);
    }
}

void mode_maintenance_quitter(void)
{
    printf("Fin du mode maintenance : carte SD reactivee\r\n");
}
