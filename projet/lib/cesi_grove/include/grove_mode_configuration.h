#ifndef GROVE_MODE_CONFIGURATION_H
#define GROVE_MODE_CONFIGURATION_H

#include <stdint.h>
#include <stdbool.h>

#define VERSION_PROGRAMME "1.0.0"
#define NUMERO_LOT        "WWW-2026-001"

typedef struct
{
    uint32_t magique;
    int32_t log_interval;
    int32_t file_max_size;
    int32_t timeout;
    int32_t lumin;
    int32_t lumin_low;
    int32_t lumin_high;
    int32_t temp_air;
    int32_t min_temp_air;
    int32_t max_temp_air;
    int32_t hygr;
    int32_t hygr_mint;
    int32_t hygr_maxt;
    int32_t pressure;
    int32_t pressure_min;
    int32_t pressure_max;
} Config_Parametres;

void mode_configuration_charger(void);
const Config_Parametres *mode_configuration_parametres(void);
void mode_configuration_init(void);
bool mode_configuration_update(void);

#endif
