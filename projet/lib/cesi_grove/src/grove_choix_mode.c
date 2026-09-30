#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_hal.h"
#include "grove_choix_mode.h"
#include "grove_mode_standard.h"
#include "grove_mode_configuration.h"
#include "grove_mode_economique.h"
#include "grove_mode_maintenance.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "grove_bme680.h"
#include "sd_logger.h"

#define APPUI_LONG_MS           5000U
#define PERIODE_CLIGNOTEMENT_MS 1000U

typedef struct
{
    bool appuye;
    bool traite;
    uint32_t debut;
} Appui_Bouton;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Couleur;

typedef struct
{
    Couleur autre;
    uint32_t duree_rouge_ms;
} Clignotement;

static const Couleur ROUGE = {128, 0, 0};

static const Couleur couleur_mode[] = {
    [MODE_STANDARD]      = {0, 128, 0},
    [MODE_CONFIGURATION] = {128, 128, 0},
    [MODE_MAINTENANCE]   = {128, 40, 0},
    [MODE_ECONOMIQUE]    = {0, 0, 128}
};

static const Clignotement clignotement[] = {
    [ERREUR_AUCUNE]               = {{0, 0, 0}, 0},
    [ERREUR_RTC]                  = {{0, 0, 128}, 500},
    [ERREUR_GPS]                  = {{128, 128, 0}, 500},
    [ERREUR_CAPTEUR]              = {{0, 128, 0}, 500},
    [ERREUR_DONNEES_INCOHERENTES] = {{0, 128, 0}, 333},
    [ERREUR_SD_PLEINE]            = {{128, 128, 128}, 500},
    [ERREUR_SD_ECRITURE]          = {{128, 128, 128}, 333}
};

static Mode_Systeme mode_actuel = MODE_STANDARD;
static Mode_Systeme mode_precedent = MODE_STANDARD;
static Erreur_Systeme erreur_actuelle = ERREUR_AUCUNE;
static Appui_Bouton bouton_rouge = {false, false, 0};
static Appui_Bouton bouton_vert = {false, false, 0};
static Couleur couleur_affichee = {0, 0, 0};
static bool couleur_connue = false;

static bool appui_long(Appui_Bouton *bouton, bool appuye)
{
    if (!appuye)
    {
        bouton->appuye = false;
        bouton->traite = false;
        return false;
    }

    if (!bouton->appuye)
    {
        bouton->appuye = true;
        bouton->debut = HAL_GetTick();
        return false;
    }

    if (!bouton->traite && ((HAL_GetTick() - bouton->debut) >= APPUI_LONG_MS))
    {
        bouton->traite = true;
        return true;
    }

    return false;
}

static void afficher_couleur(Couleur couleur)
{
    if (couleur_connue &&
        (couleur.r == couleur_affichee.r) &&
        (couleur.g == couleur_affichee.g) &&
        (couleur.b == couleur_affichee.b))
    {
        return;
    }

    GroveRGB_SetColor(couleur.r, couleur.g, couleur.b);
    couleur_affichee = couleur;
    couleur_connue = true;
}

static void choix_mode_led(void)
{
    bool acquisition = (mode_actuel == MODE_STANDARD) || (mode_actuel == MODE_ECONOMIQUE);

    if (acquisition && (erreur_actuelle != ERREUR_AUCUNE))
    {
        if ((HAL_GetTick() % PERIODE_CLIGNOTEMENT_MS) < clignotement[erreur_actuelle].duree_rouge_ms)
        {
            afficher_couleur(ROUGE);
        }
        else
        {
            afficher_couleur(clignotement[erreur_actuelle].autre);
        }
    }
    else
    {
        afficher_couleur(couleur_mode[mode_actuel]);
    }
}

static void choix_mode_changer(Mode_Systeme nouveau)
{
    const Config_Parametres *config = mode_configuration_parametres();
    Mode_Systeme ancien = mode_actuel;

    if (ancien == MODE_MAINTENANCE)
    {
        mode_maintenance_quitter();
    }

    mode_actuel = nouveau;
    erreur_actuelle = ERREUR_AUCUNE;
    SDLogger_SetMaxFileSize((uint32_t)config->file_max_size);
    choix_mode_led();

    switch (nouveau)
    {
    case MODE_STANDARD:
        printf("Mode standard\r\n");
        mode_standard_init((int)config->log_interval, (int)config->timeout);
        couleur_connue = false;
        break;

    case MODE_CONFIGURATION:
        mode_configuration_init();
        break;

    case MODE_MAINTENANCE:
        mode_precedent = ancien;
        mode_maintenance_init((int)config->timeout);
        break;

    case MODE_ECONOMIQUE:
        mode_economique_init((int)config->log_interval, (int)config->timeout);
        break;
    }
}

void choix_mode_init(void)
{
    GroveRGB_Init();
    GroveButton_Init();
    GroveLight_Init();
    GroveGPS_Init();
    (void)GroveRTC_Init();
    (void)GroveBME680_Init();

    mode_configuration_charger();

    HAL_Delay(50);

    if (GroveButton1_IsPressed())
    {
        bouton_rouge.appuye = true;
        bouton_rouge.traite = true;
        choix_mode_changer(MODE_CONFIGURATION);
    }
    else
    {
        choix_mode_changer(MODE_STANDARD);
    }
}

void choix_mode_update(void)
{
    bool rouge = appui_long(&bouton_rouge, GroveButton1_IsPressed());
    bool vert = appui_long(&bouton_vert, GroveButton2_IsPressed());

    switch (mode_actuel)
    {
    case MODE_STANDARD:
        if (rouge)
            choix_mode_changer(MODE_MAINTENANCE);
        else if (vert)
            choix_mode_changer(MODE_ECONOMIQUE);
        else
            mode_standard_update();
        break;

    case MODE_ECONOMIQUE:
        if (rouge)
            choix_mode_changer(MODE_STANDARD);
        else
            mode_economique_update();
        break;

    case MODE_MAINTENANCE:
        if (rouge)
            choix_mode_changer(mode_precedent);
        else
            mode_maintenance_update();
        break;

    case MODE_CONFIGURATION:
        if (mode_configuration_update())
        {
            printf("30 minutes sans activite\r\n");
            choix_mode_changer(MODE_STANDARD);
        }
        break;
    }

    choix_mode_led();
}

Mode_Systeme choix_mode_actuel(void)
{
    return mode_actuel;
}

void choix_mode_signaler_erreur(Erreur_Systeme erreur)
{
    erreur_actuelle = erreur;
    choix_mode_led();
}
