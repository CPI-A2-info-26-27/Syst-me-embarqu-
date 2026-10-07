#include <Arduino.h>

#define Nb_Capteurs 4
#define Indice_max_Tableau 9 

#define capteur_luminosite  0
#define capteur_temperature 1
#define capteur_humidite    2
#define capteur_pression    3

float   Tab_Mesures[Nb_Capteurs];
uint8_t Tab_Erreur[Nb_Capteurs];
float   Tab_Moy_Instant[Nb_Capteurs];

float Tab_Capteur_luminosite[Indice_max_Tableau + 1]  = {0};
float Tab_Capteur_temperature[Indice_max_Tableau + 1] = {0};
float Tab_Capteur_humidite[Indice_max_Tableau + 1]    = {0};
float Tab_Capteur_pression[Indice_max_Tableau + 1]    = {0};

uint8_t Lecture_capteur(float *Mesure, uint8_t num_capteur);
void Lecture_Capteurs(void);
void Maj_Mesures(float *Tableau, uint8_t *Erreur);
void Decalage(float *Tableau, float Valeur);
float Calcul_Moy_Instant(float *Tableau);
void Afficher(void);

void setup()
{
  Serial.begin(9600);
  randomSeed(analogRead(A0));
}

void loop()
{
  Lecture_Capteurs();
  Maj_Mesures(Tab_Mesures, Tab_Erreur);

  Tab_Moy_Instant[capteur_luminosite]  = Calcul_Moy_Instant(Tab_Capteur_luminosite);
  Tab_Moy_Instant[capteur_temperature] = Calcul_Moy_Instant(Tab_Capteur_temperature);
  Tab_Moy_Instant[capteur_humidite]    = Calcul_Moy_Instant(Tab_Capteur_humidite);
  Tab_Moy_Instant[capteur_pression]    = Calcul_Moy_Instant(Tab_Capteur_pression);

  Afficher();
  delay(1000);
}

void Lecture_Capteurs(void)
{
    float Mesure_capteur;
    uint8_t num_capteur;
    for (num_capteur = 0; num_capteur < Nb_Capteurs; num_capteur++)
    {
        Tab_Erreur[num_capteur] = 0;
        Mesure_capteur = 0;
        if (!Lecture_capteur(&Mesure_capteur, num_capteur))
        {
            Tab_Mesures[num_capteur] = Mesure_capteur;
        }
        else {
            Tab_Erreur[num_capteur] = 1;
        }
    }
}

void Maj_Mesures(float *Tableau, uint8_t *Erreur)
{
    if (Erreur[capteur_luminosite] != 1)  Decalage(Tab_Capteur_luminosite,  Tableau[capteur_luminosite]);
    if (Erreur[capteur_temperature] != 1) Decalage(Tab_Capteur_temperature, Tableau[capteur_temperature]);
    if (Erreur[capteur_humidite] != 1)    Decalage(Tab_Capteur_humidite,    Tableau[capteur_humidite]);
    if (Erreur[capteur_pression] != 1)    Decalage(Tab_Capteur_pression,    Tableau[capteur_pression]);
}

void Decalage(float *Tableau, float Valeur)
{
    uint8_t i;
    for (i = 0; i < Indice_max_Tableau; i++)
    {
        Tableau[i] = Tableau[i + 1];
    }
    Tableau[Indice_max_Tableau] = Valeur;
}

float Calcul_Moy_Instant(float *Tableau)
{
    float Moyenne = 0;
    uint8_t i;
    for (i = 0; i <= Indice_max_Tableau; i++)
    {
        Moyenne = Moyenne + Tableau[i];
    }
    Moyenne = Moyenne / (Indice_max_Tableau + 1);
    return Moyenne;
}

uint8_t Lecture_capteur(float *Mesure, uint8_t num_capteur)
{
    *Mesure = random(1000)/10.0 + num_capteur;
    return random(10) == 0;
}

void Afficher(void)
{
    uint8_t i;
    Serial.print("Moy : ");
    for (i = 0; i < Nb_Capteurs; i++)
    {
        Serial.print(Tab_Moy_Instant[i], 1);
        Serial.print(" ");
    }
    Serial.print("| err : ");
    for (i = 0; i < Nb_Capteurs; i++)
    {
        Serial.print(Tab_Erreur[i]);
        Serial.print(" ");
    }
    Serial.println();
}
