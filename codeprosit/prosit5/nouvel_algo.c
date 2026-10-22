#include <arduino.h>

// Constantes

#define NB_CAPTEURS 5
#define NB_VAL 10


// Prototypes des fonctions
uint8_t Lecture_capteur(uint16_t *Mesure, uint8_t i);
void lecture(uint16_t *Tab_Val, uint8_t *Erreurs);
void Add_Val(uint16_t *Tab_Moy, uint16_t Val);
void Afficher(uint16_t *Tab_Moy, uint8_t *Erreurs);

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(A0));
}

void loop() {
    static uint8_t Nb_erreur[NB_CAPTEURS] = {0};
    static uint16_t Moy_gliss[NB_VAL] = {0};

Lecture(Moy_gliss, Nb_erreur);  // PILE : paramètres de la fonction
    Afficher(Moy_gliss, Nb_erreur); // PILE : paramètres
    delay(1000);                    // PILE : paramètre
}


void Lecture(uint16_t *Tab_Val, uint8_t *Erreurs) {  // PILE : paramètres
    uint16_t Mesure;                // PILE : variable locale
    uit_8_t Erreur, i;              // PILE : variables locales
    for (i = 0; i < NB_CAPTEURS; i++) {
        Mesure = 0;
        Erreur = Lecture_capteur(&Mesure, i);  // PILE : paramètres
        if (Erreur)
            Erreurs[i]++;
        else
            Add_Val(Tab_Val, Mesure);   // PILE : paramètres
    }
}


void Add_Val(uint16_t *Tab_Moy, uint16_t Val) {   // PILE : paramètres
    static uint_8t ind_moy = 0;     // PAS dans la pile : .bss
    Tab_Moy[ind_moy] = Val;
    if (ind_moy >= NB_VAL - 1)
        ind_moy = 0;
    else
        ind_moy++;
}


uint8_t lecture_capteur(uint16_t *Mesure, uint8_t i) { // PILE : paramètres Mesure et i
    *Mesure = random(1000) + i * 10;
    return random(10) == 0;
}

void Afficher(uint16_t *Tab_Moy, uint8_t *Erreurs) {  // PILE : paramètres Tab_Moy et Erreurs
    uint8_t i;                      // PILE : variable locale
    for (i = 0; i < NB_VAL; i++)
        {
        Serial.print(Tab_Moy[i] / 10.0, 1);
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
