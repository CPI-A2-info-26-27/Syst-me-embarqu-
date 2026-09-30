#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "grove_mode_economique.h"
#include "grove_mode_standard.h"

static uint32_t intervalle_ms = 0;
static uint32_t derniere_mesure = 0;
static int timeout_s = 0;
static bool gps_cette_mesure = true;

void mode_economique_init(int LOG_INTERVAL, int TIMEOUT)
{
    intervalle_ms = (uint32_t)LOG_INTERVAL * 2U * 60000U;
    timeout_s = TIMEOUT;
    gps_cette_mesure = true;
    derniere_mesure = HAL_GetTick();

    printf("Mode economique\r\n");
}

void mode_economique_update(void)
{
    if ((HAL_GetTick() - derniere_mesure) >= intervalle_ms)
    {
        derniere_mesure = HAL_GetTick();
        mode_standard_acquisition(timeout_s, gps_cette_mesure, true);
        gps_cette_mesure = !gps_cette_mesure;
    }
}
