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
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include "stm32l4xx_hal.h"
#include "grove_mode_configuration.h"

#define CONFIG_MAGIQUE          0x57575731UL
#define CONFIG_FLASH_ADRESSE    0x080FF800UL
#define CONFIG_FLASH_BANQUE     FLASH_BANK_2
#define CONFIG_FLASH_PAGE       255U
#define CONFIG_INACTIVITE_MS    (30UL * 60UL * 1000UL)
#define CONFIG_LIGNE_TAILLE     64U

_Static_assert((sizeof(Config_Parametres) % 8U) == 0U, "Config_Parametres doit faire un multiple de 8 octets");

extern UART_HandleTypeDef huart2;

typedef struct
{
    const char *nom;
    size_t decalage;
    int32_t min;
    int32_t max;
} Config_Champ;

static const Config_Champ champs[] = {
    {"LOG_INTERVAL",  offsetof(Config_Parametres, log_interval),  1,    1440},
    {"LOG_INTERVALL", offsetof(Config_Parametres, log_interval),  1,    1440},
    {"FILE_MAX_SIZE", offsetof(Config_Parametres, file_max_size), 512,  1048576},
    {"TIMEOUT",       offsetof(Config_Parametres, timeout),       1,    600},
    {"LUMIN",         offsetof(Config_Parametres, lumin),         0,    1},
    {"LUMIN_LOW",     offsetof(Config_Parametres, lumin_low),     0,    1023},
    {"LUMIN_HIGH",    offsetof(Config_Parametres, lumin_high),    0,    1023},
    {"TEMP_AIR",      offsetof(Config_Parametres, temp_air),      0,    1},
    {"MIN_TEMP_AIR",  offsetof(Config_Parametres, min_temp_air),  -40,  85},
    {"MAX_TEMP_AIR",  offsetof(Config_Parametres, max_temp_air),  -40,  85},
    {"HYGR",          offsetof(Config_Parametres, hygr),          0,    1},
    {"HYGR_MINT",     offsetof(Config_Parametres, hygr_mint),     -40,  85},
    {"HYGR_MAXT",     offsetof(Config_Parametres, hygr_maxt),     -40,  85},
    {"PRESSURE",      offsetof(Config_Parametres, pressure),      0,    1},
    {"PRESSURE_MIN",  offsetof(Config_Parametres, pressure_min),  300,  1100},
    {"PRESSURE_MAX",  offsetof(Config_Parametres, pressure_max),  300,  1100}
};

#define NB_CHAMPS (sizeof(champs) / sizeof(champs[0]))

static const Config_Parametres parametres_defaut = {
    CONFIG_MAGIQUE,
    10, 2048, 30,
    1, 255, 768,
    1, -10, 60,
    1, 0, 50,
    1, 850, 1080
};

static const char *const jours_semaine[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

static Config_Parametres parametres;
static char ligne_commande[CONFIG_LIGNE_TAILLE];
static uint32_t longueur_commande = 0;
static uint32_t derniere_activite = 0;

static int32_t *config_champ(Config_Parametres *p, const Config_Champ *champ)
{
    return (int32_t *)((uint8_t *)p + champ->decalage);
}

static bool config_valide(Config_Parametres *p)
{
    uint32_t i;

    if (p->magique != CONFIG_MAGIQUE)
    {
        return false;
    }

    for (i = 0; i < NB_CHAMPS; ++i)
    {
        int32_t valeur = *config_champ(p, &champs[i]);
        if ((valeur < champs[i].min) || (valeur > champs[i].max))
        {
            return false;
        }
    }

    return true;
}

static bool config_sauvegarder(void)
{
    FLASH_EraseInitTypeDef effacement = {0};
    uint32_t page_erreur = 0;
    uint64_t mot;
    uint32_t i;
    bool ok;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    effacement.TypeErase = FLASH_TYPEERASE_PAGES;
    effacement.Banks = CONFIG_FLASH_BANQUE;
    effacement.Page = CONFIG_FLASH_PAGE;
    effacement.NbPages = 1;
    ok = (HAL_FLASHEx_Erase(&effacement, &page_erreur) == HAL_OK);

    for (i = 0; ok && (i < sizeof(parametres)); i += 8U)
    {
        memcpy(&mot, (const uint8_t *)&parametres + i, sizeof(mot));
        ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, CONFIG_FLASH_ADRESSE + i, mot) == HAL_OK);
    }

    HAL_FLASH_Lock();
    return ok;
}

static void config_repondre_sauvegarde(void)
{
    if (config_sauvegarder())
    {
        printf("OK\r\n");
    }
    else
    {
        printf("ERREUR sauvegarde\r\n");
    }
}

static void config_afficher_aide(void)
{
    printf("Mode configuration\r\n");
    printf("Commandes : PARAMETRE=valeur, PARAMETRE, RESET, VERSION\r\n");
    printf("            CLOCK=HH:MM:SS, DATE=MM,JJ,AAAA, DAY=MON..SUN\r\n");
    printf("Retour automatique en mode standard apres 30 minutes sans activite\r\n");
}

static void config_commande_horloge(const char *nom, const char *valeur)
{
    RTC_DateTime dt;
    int a = 0;
    int b = 0;
    int c = 0;
    char reste;
    bool ok = false;
    uint8_t i;

    if (!GroveRTC_GetDateTime(&dt))
    {
        dt.seconds = 0;
        dt.minutes = 0;
        dt.hours = 0;
        dt.dayOfWeek = 1;
        dt.day = 1;
        dt.month = 1;
        dt.year = 2000;
    }

    if (strcmp(nom, "CLOCK") == 0)
    {
        if ((sscanf(valeur, "%d:%d:%d%c", &a, &b, &c, &reste) == 3) &&
            (a >= 0) && (a <= 23) && (b >= 0) && (b <= 59) && (c >= 0) && (c <= 59))
        {
            dt.hours = (uint8_t)a;
            dt.minutes = (uint8_t)b;
            dt.seconds = (uint8_t)c;
            ok = true;
        }
    }
    else if (strcmp(nom, "DATE") == 0)
    {
        if ((sscanf(valeur, "%d,%d,%d%c", &a, &b, &c, &reste) == 3) &&
            (a >= 1) && (a <= 12) && (b >= 1) && (b <= 31) && (c >= 2000) && (c <= 2099))
        {
            dt.month = (uint8_t)a;
            dt.day = (uint8_t)b;
            dt.year = (uint16_t)c;
            ok = true;
        }
    }
    else
    {
        for (i = 0; i < 7U; ++i)
        {
            if (strcmp(valeur, jours_semaine[i]) == 0)
            {
                dt.dayOfWeek = (uint8_t)(i + 1U);
                ok = true;
            }
        }
    }

    if (ok && GroveRTC_SetDateTime(&dt))
    {
        printf("OK\r\n");
    }
    else
    {
        printf("ERREUR\r\n");
    }
}

static void config_executer(char *commande)
{
    char *egal = strchr(commande, '=');
    const char *valeur = 0;
    char *fin;
    long nombre;
    uint32_t i;

    if (egal != 0)
    {
        *egal = '\0';
        valeur = egal + 1;
    }

    if ((valeur == 0) && (strcmp(commande, "RESET") == 0))
    {
        parametres = parametres_defaut;
        SDLogger_SetMaxFileSize((uint32_t)parametres.file_max_size);
        config_repondre_sauvegarde();
        return;
    }

    if ((valeur == 0) && (strcmp(commande, "VERSION") == 0))
    {
        printf("Version %s - lot %s\r\n", VERSION_PROGRAMME, NUMERO_LOT);
        return;
    }

    if ((valeur != 0) &&
        ((strcmp(commande, "CLOCK") == 0) || (strcmp(commande, "DATE") == 0) || (strcmp(commande, "DAY") == 0)))
    {
        config_commande_horloge(commande, valeur);
        return;
    }

    for (i = 0; i < NB_CHAMPS; ++i)
    {
        if (strcmp(commande, champs[i].nom) != 0)
        {
            continue;
        }

        if (valeur == 0)
        {
            printf("%s=%ld\r\n", champs[i].nom, (long)*config_champ(&parametres, &champs[i]));
            return;
        }

        nombre = strtol(valeur, &fin, 10);
        if ((fin == valeur) || (*fin != '\0') || (nombre < champs[i].min) || (nombre > champs[i].max))
        {
            printf("ERREUR valeur hors domaine [%ld;%ld]\r\n", (long)champs[i].min, (long)champs[i].max);
            return;
        }

        *config_champ(&parametres, &champs[i]) = (int32_t)nombre;
        SDLogger_SetMaxFileSize((uint32_t)parametres.file_max_size);
        config_repondre_sauvegarde();
        return;
    }

    printf("ERREUR commande inconnue\r\n");
}

void mode_configuration_charger(void)
{
    Config_Parametres lu;

    memcpy(&lu, (const void *)CONFIG_FLASH_ADRESSE, sizeof(lu));
    parametres = config_valide(&lu) ? lu : parametres_defaut;
}

const Config_Parametres *mode_configuration_parametres(void)
{
    return &parametres;
}

void mode_configuration_init(void)
{
    longueur_commande = 0;
    derniere_activite = HAL_GetTick();
    config_afficher_aide();
}

bool mode_configuration_update(void)
{
    uint8_t c;

    __HAL_UART_CLEAR_OREFLAG(&huart2);

    while (HAL_UART_Receive(&huart2, &c, 1, 0) == HAL_OK)
    {
        derniere_activite = HAL_GetTick();

        if ((c == '\r') || (c == '\n'))
        {
            if (longueur_commande > 0U)
            {
                printf("\r\n");
                ligne_commande[longueur_commande] = '\0';
                config_executer(ligne_commande);
                longueur_commande = 0;
            }
        }
        else if ((c == 0x08U) || (c == 0x7FU))
        {
            if (longueur_commande > 0U)
            {
                longueur_commande--;
            }
        }
        else if (longueur_commande < (CONFIG_LIGNE_TAILLE - 1U))
        {
            ligne_commande[longueur_commande++] = (char)toupper(c);
            (void)HAL_UART_Transmit(&huart2, &c, 1, 10);
        }
    }

    return (HAL_GetTick() - derniere_activite) >= CONFIG_INACTIVITE_MS;
}
