En configuration (setup) :

Nb_Capteurs : le nombre total de capteurs et outils de mesures

Pour chaque capteur : #define capteur_fonction = num_capteur (allant de 0 à Nb_Capteurs-1)

Indice_max_Tableau : représente le nombre de mesure pour la moyenne instantanée

Les tableaux seront dimensionnés par rapport à ces différentes valeurs :

Tab_Mesures[Nb_Capteurs - 1]

Tab_Erreur[Nb_Capteurs – 1]

Tab_Moy_Instant[NbCapteurs – 1]

Et pour chaque capteur :

Tab_Capteur_num_capteur[indice_max_Tableau]
En exécution (loop) :

Le programme appelera successivement les fonctions :

Lecture_Capteurs()

Maj_Mesures(…,…)

Calcul_Moy_Instant() pour chaque tableau de capteur
Les fonctions/procédures principales :

PROCEDURE LECTURE_CAPTEURS

Lecture_Capteurs()

Tab_Erreur = 0

Pour chaque capteur (num_capteur) :

               Si Mesure_capteur <> « erreur » alors Tab_Mesures[num_capteur] = Mesure_capteur Sinon Tab_erreur[num_capteur] = 1

--------------------------------------------------

 

PROCEDURE MAJ_MESURES

Maj_Mesures(Tableau[],Erreur[])

Une instruction par tableau de capteur :

Si Erreur[num_capteur] <> 1 alors

Tableau_Capteur_num_capteur[] = Decalage(Tableau_Capteur_num_capteur[], Tableau[num_capteur])
Les 2 fonctions factorisées :

FONCTION DECALAGE

NouveauTableau[] = Decalage(Tableau[], Valeur)

Pour i allant de 0 à indice_max_Tableau - 1

Tableau[i] = Tableau [i+1]

Tableau[indice_max_Tableau] = Valeur

Return Tableau[]

--------------------------------------------------

 

FONCTION CALCUL_MOY_INSTANT

Moy_Tableau Calcul_Moy_Instant(Tableau[])

Moyenne = 0