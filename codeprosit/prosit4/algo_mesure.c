#include <Arduino.h>

// Constantes

#define CAPTEUR_TEMPERATURE 0
#define CAPTEUR_HUMIDITE    1
#define CAPTEUR_PRESSION    2
#define CAPTEUR_LUMIERE     3
#define CAPTEUR_SON         4

#define NB_CAPTEURS 5
#define NB_VAL 10


// Tableaux
float   Tab_Mesures[NB_CAPTEURS];
uint8_t Tab_Erreur[NB_CAPTEURS];
float   Tab_Moy_Instant[NB_CAPTEURS];
float   Tab_Capteurs[NB_CAPTEURS][NB_VAL];


// Prototypes des fonctions
uint8_t Lecture_capteur(float *Mesure, uint8_t i);
void Lecture_Capteurs(float *Mesures, uint8_t *Erreurs);
void Maj_Mesures(float *Mesures, uint8_t *Erreurs);
void Decalage(float *Tableau, float Valeur);
float Calcul_Moy_Instant(float *Tableau);
void Afficher(float *Tab_Moy, uint8_t *Erreurs);

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A0));
}

void loop() {
    uint8_t i;

    Lecture_Capteurs(Tab_Mesures, Tab_Erreur);
    Maj_Mesures(Tab_Mesures, Tab_Erreur);
    for (i = 0; i < NB_CAPTEURS; i++)
        Tab_Moy_Instant[i] = Calcul_Moy_Instant(Tab_Capteurs[i]);

    Afficher(Tab_Moy_Instant, Tab_Erreur);
    delay(1000);
}


uint8_t Lecture_capteur(float *Mesure, uint8_t i) {
    *Mesure = random(1000) / 10.0 + i;
    return random(10) == 0;
}


void Lecture_Capteurs(float *Mesures, uint8_t *Erreurs) {
    float Mesure;
    uint8_t i;
    for (i = 0; i < NB_CAPTEURS; i++) {
        Erreurs[i] = 0;
        Mesure = 0;
        if (Lecture_capteur(&Mesure, i))
            Erreurs[i] = 1;
        else
            Mesures[i] = Mesure;
    }
}


void Maj_Mesures(float *Mesures, uint8_t *Erreurs) {
    uint8_t i;
    for (i = 0; i < NB_CAPTEURS; i++) {
        if (Erreurs[i] != 1)
            Decalage(Tab_Capteurs[i], Mesures[i]);
    }
}


void Decalage(float *Tableau, float Valeur) {
    uint8_t i;
    for (i = 0; i < NB_VAL - 1; i++)
        Tableau[i] = Tableau[i + 1];
    Tableau[NB_VAL - 1] = Valeur;
}


float Calcul_Moy_Instant(float *Tableau) {
    float Moyenne = 0;
    uint8_t i;
    for (i = 0; i < NB_VAL; i++)
        Moyenne += Tableau[i];
    return Moyenne / NB_VAL;
}


void Afficher(float *Tab_Moy, uint8_t *Erreurs) {
    uint8_t i;
    for (i = 0; i < NB_CAPTEURS; i++)
        {
        Serial.print(Tab_Moy[i], 1);
        Serial.print(" ");
        }
    Serial.print("| err: ");
    for (i = 0; i < NB_CAPTEURS; i++)
        {
        Serial.print(Erreurs[i]);
        Serial.print(" ");
        }
    Serial.println();
}
