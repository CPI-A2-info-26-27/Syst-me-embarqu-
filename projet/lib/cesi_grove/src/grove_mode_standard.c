#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "setup.h"
#include "cesi_types.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "grove_bme680.h"
#include "sd_logger.h"
#include "grove_mode_standard.h"
#include "grove_mode_configuration.h"
#include "grove_choix_mode.h"

#define ECHECS_AVANT_ERREUR 2U

static uint32_t intervalle_ms = 0;
static uint32_t timeout_ms = 0;
static uint32_t derniere_mesure = 0;
static char ligne[160];

static bool mesure_gps = true;
static bool mesure_sd = true;
static RTC_DateTime derniere_date = {0};
static uint8_t echecs_rtc = 0;
static uint8_t echecs_luminosite = 0;
static uint8_t echecs_bme680 = 0;
static uint8_t echecs_gps = 0;

static void compter_echec(uint8_t *compteur, bool reussi, bool actif)
{
    if (!actif)
    {
        return;
    }

    if (reussi)
    {
        *compteur = 0;
    }
    else if (*compteur < 255U)
    {
        (*compteur)++;
    }
}

static const char *niveau_luminosite(uint16_t luminosite, const Config_Parametres *config)
{
    int32_t valeur = (int32_t)(luminosite >> 2);

    if (valeur < config->lumin_low)
    {
        return "faible";
    }
    if (valeur > config->lumin_high)
    {
        return "forte";
    }
    return "moyenne";
}

static void mode_standard_mesure(void)
{
    const Config_Parametres *config = mode_configuration_parametres();
    RTC_DateTime date_heure;
    uint16_t luminosite = 0;
    Env_Data env = {0};
    GPS_Data gps = {0};
    size_t pos = 0;
    char nom_fichier[16];
    Erreur_Systeme erreur = ERREUR_AUCUNE;
    bool bme680_actif = config->temp_air || config->hygr || config->pressure;

    bool rtc_ok = GroveRTC_ReadWithTimeout(&date_heure, timeout_ms);
    bool lum_ok = config->lumin && GroveLight_ReadWithTimeout(&luminosite, timeout_ms);
    bool env_ok = bme680_actif && GroveBME680_ReadWithTimeout(&env, timeout_ms);
    bool gps_ok = mesure_gps && GroveGPS_ReadWithTimeout(&gps, timeout_ms);

    bool temp_ok = env_ok && config->temp_air &&
                   (env.temperature_c >= config->min_temp_air) &&
                   (env.temperature_c <= config->max_temp_air);
    bool hygr_ok = env_ok && config->hygr &&
                   (env.temperature_c >= config->hygr_mint) &&
                   (env.temperature_c <= config->hygr_maxt);
    bool pres_ok = env_ok && config->pressure &&
                   (env.pressure_hpa >= config->pressure_min) &&
                   (env.pressure_hpa <= config->pressure_max);
    bool incoherent = env_ok && ((config->temp_air && !temp_ok) || (config->pressure && !pres_ok));

    compter_echec(&echecs_rtc, rtc_ok, true);
    compter_echec(&echecs_luminosite, lum_ok, config->lumin);
    compter_echec(&echecs_bme680, env_ok, bme680_actif);
    compter_echec(&echecs_gps, gps_ok, mesure_gps);

    if (rtc_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos,
                        "%04d-%02d-%02d %02d:%02d:%02d;",
                        (int)date_heure.year, (int)date_heure.month, (int)date_heure.day,
                        (int)date_heure.hours, (int)date_heure.minutes, (int)date_heure.seconds);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;");

    if (lum_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "%u;", luminosite);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;");

    if (temp_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "%.1f;", env.temperature_c);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;");

    if (hygr_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "%.1f;", env.humidity_percent);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;");

    if (pres_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "%.1f;", env.pressure_hpa);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;");

    if (gps_ok)
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "%.5f;%.5f",
                        gps.latitude, gps.longitude);
    else
        pos += snprintf(ligne + pos, sizeof(ligne) - pos, "NA;NA");

    printf("%s\r\n", ligne);

    if (!mesure_sd && lum_ok)
    {
        printf("Luminosite %s\r\n", niveau_luminosite(luminosite, config));
    }

    if (echecs_rtc >= ECHECS_AVANT_ERREUR)
        erreur = ERREUR_RTC;
    else if (echecs_gps >= ECHECS_AVANT_ERREUR)
        erreur = ERREUR_GPS;
    else if ((echecs_luminosite >= ECHECS_AVANT_ERREUR) || (echecs_bme680 >= ECHECS_AVANT_ERREUR))
        erreur = ERREUR_CAPTEUR;
    else if (incoherent)
        erreur = ERREUR_DONNEES_INCOHERENTES;

    if (mesure_sd)
    {
        if (rtc_ok)
        {
            derniere_date = date_heure;
        }

        snprintf(nom_fichier, sizeof(nom_fichier), "%02u%02u%02u_0.LOG",
                 (unsigned int)(derniere_date.year % 100U),
                 (unsigned int)derniere_date.month,
                 (unsigned int)derniere_date.day);

        if (!SDLogger_WriteLine(nom_fichier, ligne) && (erreur == ERREUR_AUCUNE))
        {
            erreur = SDLogger_IsCardFull() ? ERREUR_SD_PLEINE : ERREUR_SD_ECRITURE;
        }
    }

    choix_mode_signaler_erreur(erreur);
}

void mode_standard_init(int LOG_INTERVAL, int TIMEOUT)
{
    intervalle_ms = (uint32_t)LOG_INTERVAL * 60000U;
    timeout_ms    = (uint32_t)TIMEOUT * 1000U;

    GroveRGB_SetColor(0, 128, 0);

    derniere_mesure = HAL_GetTick();
    mode_standard_mesure();
}

void mode_standard_update(void)
{
    if ((HAL_GetTick() - derniere_mesure) >= intervalle_ms)
    {
        derniere_mesure = HAL_GetTick();
        mode_standard_mesure();
    }
}

void mode_standard_acquisition(int TIMEOUT, bool avec_gps, bool avec_sd)
{
    timeout_ms = (uint32_t)TIMEOUT * 1000U;
    mesure_gps = avec_gps;
    mesure_sd  = avec_sd;

    mode_standard_mesure();

    mesure_gps = true;
    mesure_sd  = true;
}
