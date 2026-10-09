// Constantes
#define Nb_Capteurs 5
#define Nb_Val 10

uint8_t Lecture_capteur(float *Mesure, uint8_t i);
void Lecture(float *Tab_Val, uint16_t *Erreurs);
void Add_Val(float *Tab_Moy, float Val);
void Afficher(float *Tab_Moy, uint16_t *Erreurs);

void setup() 
{
  Serial.begin(9600);
  RandomSeed(analogRead(A0));
}

void loop() 
{
  // Variables locales (loop)
  uint16_t Nb_erreur[Nb_Capteurs] = {0};
  float Moy_gliss[Nb_Val] = {0};

  Lecture(Moy_gliss, Nb_erreur);
  Afficher(Moy_gliss, Nb_erreur);
  delay(1000);
}

// PROCEDURE LECTURE(POINTEUR_TABLEAU , POINTEUR_ERREURS)
void Lecture(float *Tab_Val, uint16_t *Erreurs)
{

    float Mesure;
    uint8_t Erreur, i;
    for (i = 0; i < Nb_Capteurs; i++)
    {
        Mesure = 0;
        Erreur = Lecture_capteur(&Mesure,i); 
        if (Erreur)
        {
            (Erreurs[i])++;
        }
        else {
            Add_Val(Tab_Val,Mesure);
        }
    }
}
// PROCEDURE ADD_VAL(POINTEUR_TABLEAU , VALEUR)
void Add_Val(float *Tab_Moy, float Val)
{
    static uint8_t ind_moy = 0;
    Tab_Moy[ind_moy] = Val;
    if (ind_moy >= Nb_Val - 1) ind_moy = 0;
    else ind_moy++;
}

// Simulation de la lecture d'un capteur par une valeur aléatoire
uint8_t Lecture_capteur(float *Mesure, uint8_t i)
{
    *Mesure = random(1000)/10.0 + i; 
    return random(10) == 0;
}

void Afficher(float *Tab_Moy, uint16_t *Erreurs)
{
    uint8_t i;
    for (i = 0; i < Nb_Val; i++)
    {
        Serial.print(Tab_Moy[i], 1);
        Serial.print(" ");
    }
    Serial.println("| err : ");
    for (i = 0; i < Nb_Capteurs; i++)
    {
        Serial.print(Erreurs[i]);
        Serial.print(" ");
    }
    Serial.println();
}

Pb : comment réduire la taille de la pile.
Contraintes : 
sram limitée a 2Ko partagée entre les variables globales, la pile et le tas 
fixer une limite de taille pour la pile.
temps d'acces aux variables (utilisation des pointeurs)
rester formel au comprtement de l'algorithme.

Livrables :
Programme.
schéma de la pile a en cours d'évolution
maquette physique du système embarqué