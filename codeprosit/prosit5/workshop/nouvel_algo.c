#include <arduino.h>

// Constantes

#define NB_CAPTEURS 5
#define NB_VAL 10


// Prototypes des fonctions
uint8_t Lecture_capteur(float *Mesure, uint8_t i);
void lecture(float *Tab_Val, uint16_t *Erreurs);
void Add_Val(float *Tab_Moy, float Val);
void Afficher(float *Tab_Moy, uint16_t *Erreurs);

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A0));
}

void loop() {

    uint16_t Nb_erreur[NB_CAPTEURS] = {0};
    float Moy_gliss[NB_VAL] = {0};

    Lecture(Moy_gliss, Nb_erreur);
    Afficher(Moy_gliss, Nb_erreur);
    delay(1000);
}


void Lecture(float *Tab_Val, uint16_t *Erreurs) {
    float Mesure;
    uit_8_t Erreur, i;
    for (i = 0; i < NB_CAPTEURS; i++) {
        Mesure = 0;
        Erreur = Lecture_capteur(&Mesure, i);
        if (Erreur)
            Erreurs[i]++;
        else
            Add_Val(Tab_Val, Mesure);
    }
}


void Add_Val(float *Tab_Moy, float Val) {
    static uint_8t ind_moy = 0;
    Tab_Moy[ind_moy] = Val;
    if (ind_moy >= NB_VAL - 1) 
        ind_moy = 0;
    else
        ind_moy++;
}


uint8_t lecture_capteur(float *Mesure, uint8_t i) {
    *Mesure = random(1000) / 10.0 + i;
    return random(10) == 0;
}

void Afficher(float *Tab_Moy, uint16_t *Erreurs) {
    uint8_t i;
    for (i = 0; i < NB_VAL; i++)
        {
        Serial.print(Tab_Moy[i], 1);
        Serial.print(" ");
        }
    Serial.print("| err; ");
    for (i = 0; i < NB_CAPTEURS; i++)
        {
        Serial.print(Erreurs[i]);
        Serial.print(" ");
        }
Serial.println();
}