#include <Arduino.h>


// Configuration

// Un #define par capteur : num_capteur allant de 0 a NB_CAPTEURS - 1
#define CAPTEUR_TEMPERATURE 0
#define CAPTEUR_HUMIDITE    1
#define CAPTEUR_PRESSION    2
#define CAPTEUR_LUMIERE     3

#define NB_CAPTEURS 4

// Nombre de mesures utilisees pour la moyenne instantanee = INDICE_MAX_TABLEAU + 1
#define INDICE_MAX_TABLEAU 9
#define TAILLE_TABLEAU (INDICE_MAX_TABLEAU + 1)

bool  Tab_Erreur[NB_CAPTEURS];

// Un tableau d'historique par capteur (Tab_Capteur_num_capteur), utilise en buffer circulaire
float Tab_Capteurs[NB_CAPTEURS][TAILLE_TABLEAU];
int   Tab_Index[NB_CAPTEURS];   // case qui contient la plus ancienne mesure
float Tab_Somme[NB_CAPTEURS];   // somme des mesures de l'historique


// Lecture materielle d'un capteur (a remplacer par les vrais drivers)
// Retourne false si le capteur est en erreur.
bool Lire_Capteur(int num_capteur, float *mesure)
{
    switch (num_capteur) {
        case CAPTEUR_TEMPERATURE: *mesure = 20.0f; return true;
        case CAPTEUR_HUMIDITE:    *mesure = 50.0f; return true;
        case CAPTEUR_PRESSION:    *mesure = 1013.0f; return true;
        case CAPTEUR_LUMIERE:     *mesure = analogRead(A0); return true;
        default:                  return false;
    }
}


// Fonctions factorisees


// Remplace la plus ancienne mesure par la nouvelle et met la somme a jour
void Decalage(int num_capteur, float valeur)
{
    int i = Tab_Index[num_capteur];

    Tab_Somme[num_capteur] += valeur - Tab_Capteurs[num_capteur][i];
    Tab_Capteurs[num_capteur][i] = valeur;

    i++;
    if (i == TAILLE_TABLEAU) {
        i = 0;
    }
    Tab_Index[num_capteur] = i;
}

float Calcul_Moy_Instant(int num_capteur)
{
    return Tab_Somme[num_capteur] / TAILLE_TABLEAU;
}


// Procedures principales


// Lit chaque capteur et met directement son historique a jour
void Lecture_Capteurs()
{
    for (int num_capteur = 0; num_capteur < NB_CAPTEURS; num_capteur++) {
        float mesure;
        Tab_Erreur[num_capteur] = false;

        if (Lire_Capteur(num_capteur, &mesure)) {
            Decalage(num_capteur, mesure);
        } else {
            Tab_Erreur[num_capteur] = true;
        }
    }
}


// Arduino


void setup()
{
    Serial.begin(9600);

    for (int c = 0; c < NB_CAPTEURS; c++) {
        Tab_Erreur[c] = false;
        Tab_Index[c] = 0;
        Tab_Somme[c] = 0;
        for (int i = 0; i < TAILLE_TABLEAU; i++) {
            Tab_Capteurs[c][i] = 0;
        }
    }
}

void loop()
{
    Lecture_Capteurs();

    for (int num_capteur = 0; num_capteur < NB_CAPTEURS; num_capteur++) {
        Serial.print("Capteur ");
        Serial.print(num_capteur);
        Serial.print(" : moyenne = ");
        Serial.println(Calcul_Moy_Instant(num_capteur));
    }

    delay(1000);
}
