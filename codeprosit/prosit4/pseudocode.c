
void setup():

#define capteur_fonction = num_capteur (allant de 0 à Nb_Capteurs-1)

Tab_Mesures[Nb_Capteurs - 1]

Tab_Erreur[Nb_Capteurs – 1]

Tab_Moy_Instant[NbCapteurs – 1]


Tab_Capteur_num_capteur[indice_max_Tableau]


void loop() {

Lecture_Capteurs()

Maj_Mesures(…,…)

Calcul_Moy_Instant() pour chaque tableau de capteur

Les fonctions/procédures principales :
PROCEDURE LECTURE_CAPTEURS

Lecture_Capteurs()

Tab_Erreur = 0

Pour chaque capteur (num_capteur) :

               Si Mesure_capteur <> « erreur » alors Tab_Mesures[num_capteur] = Mesure_capteur Sinon Tab_erreur[num_capteur] = 1


 

PROCEDURE MAJ_MESURES

int[] Maj_Mesures(Tableau[],Erreur[]): {
    

Une instruction par tableau de capteur :

Si Erreur[num_capteur] <> 1 alors

Tableau_Capteur_num_capteur[] = Decalage(Tableau_Capteur_num_capteur[], Tableau[num_capteur])


int[] Decalage(Tableau[], Valeur): {

Pour i allant de 0 à indice_max_Tableau - 1

Tableau[i] = Tableau [i+1]

Tableau[indice_max_Tableau] = Valeur

Return Tableau[]



Moy_Tableau Calcul_Moy_Instant(Tableau[])

Moyenne = 0

Pour chaque i de Tableau

Moyenne = Moyenne + Tableau[i]

Moyenne = Moyenne / (indice_max_Tableau + 1)

Return Moyenne