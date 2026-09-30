#ifndef GROVE_CHOIX_MODE_H
#define GROVE_CHOIX_MODE_H

typedef enum
{
    MODE_STANDARD = 0,
    MODE_CONFIGURATION,
    MODE_MAINTENANCE,
    MODE_ECONOMIQUE
} Mode_Systeme;

typedef enum
{
    ERREUR_AUCUNE = 0,
    ERREUR_RTC,
    ERREUR_GPS,
    ERREUR_CAPTEUR,
    ERREUR_DONNEES_INCOHERENTES,
    ERREUR_SD_PLEINE,
    ERREUR_SD_ECRITURE
} Erreur_Systeme;

void choix_mode_init(void);
void choix_mode_update(void);
Mode_Systeme choix_mode_actuel(void);
void choix_mode_signaler_erreur(Erreur_Systeme erreur);

#endif
