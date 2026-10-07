# Prosit 4 – Avancement : algorithme de mesure des capteurs

## 1. Contexte

L'équipe MMH a écrit un algorithme de gestion des mesures (fichier `Algo_Mesure.docx`) :

- chaque capteur a une constante (`#define`) qui sert d'indice dans les tableaux ;
- `Tab_Mesures[]` contient la dernière mesure de chaque capteur ;
- `Tab_Erreur[]` signale un échec de mesure ;
- chaque capteur a son propre tableau d'historique pour calculer une moyenne glissante ;
- `Tab_Moy_Instant[]` regroupe les moyennes ;
- les fonctions `Decalage()` et `Calcul_Moy_Instant()` sont communes à tous les tableaux.

La supervision a refusé cette version :

1. la complexité doit être réduite, car le temps d'exécution doit être minimal ;
2. les tableaux doivent être remplacés par des structures ;
3. la mémoire est limitée ;
4. les mesures n'ont pas besoin de nombres réels (`float`, qui montent jusqu'à environ ±3,4·10³⁸, soit environ 2¹²⁸), d'autant plus qu'elles sont tronquées pendant le traitement.

La remarque finale (« il leur reste toujours la solution de tester le programme, à moins que… ») indique qu'on n'a pas besoin de tester pour savoir si le programme est efficace : on peut le **démontrer par le calcul de complexité**.

---

## 2. Étapes réalisées

| Étape | Contenu |
|---|---|
| 1 | Traduction du pseudocode en C++ Arduino (**version 1**) |
| 2 | Calcul de la complexité de la version 1 (section 3) |
| 3 | Analyse du retour de la supervision (section 1) |
| 4 | Étude d'une proposition de code d'un autre groupe : rejetée (section 5) |
| 5 | Prototype complet (structures, entiers 16 bits, champs de bits) : mis de côté pour l'instant (section 6) |
| 6 | **Version 2** : optimisation de la complexité et suppression des données redondantes, sur la base de la version 1 (section 4) |

### Corrections du pseudocode faites lors de la traduction

- `Tab_Mesures[Nb_Capteurs - 1]` est trop petit d'une case : il faut `NB_CAPTEURS` cases (indices 0 à N−1). C'est pareil pour `Tab_Erreur` et `Tab_Moy_Instant`.
- `Decalage` écrit dans `Tableau[indice_max_Tableau]` et la moyenne divise par `indice_max_Tableau + 1`. Le tableau d'historique doit donc avoir `INDICE_MAX_TABLEAU + 1` cases.
- Les tableaux `Tab_Capteur_x` ont été regroupés dans un tableau à deux dimensions `Tab_Capteurs[N][T]`, pour pouvoir les parcourir avec une boucle.

---

## 3. Complexité de la version 1

### 3.1 Paramètres

On exprime le coût en fonction de deux paramètres :

- **N** = nombre de capteurs (`NB_CAPTEURS`), ici **4** ;
- **T** = taille de l'historique (`TAILLE_TABLEAU = INDICE_MAX_TABLEAU + 1`), ici **10**.

### 3.2 Méthode de calcul

1. **Choisir les opérations élémentaires.** On compte les affectations, les opérations arithmétiques, les comparaisons sur les données et les appels de lecture capteur. Chacune coûte 1. La gestion des compteurs de boucle (`i++`, `i < T`) n'est pas comptée, et l'affichage série non plus.
2. **Compter chaque fonction, de l'intérieur vers l'extérieur :**
   - une **séquence** d'instructions : on **additionne** leurs coûts ;
   - une **boucle** de k tours : on **multiplie** le coût du corps par k ;
   - un **appel de fonction** : on remplace l'appel par le coût de la fonction.
3. **Additionner** le coût des fonctions appelées dans un tour de `loop()`.
4. **Passer à la notation O** : on garde le terme qui grandit le plus vite et on enlève les constantes.

### 3.3 Calcul fonction par fonction

**`Decalage(tableau, valeur)`**

```cpp
for (int i = 0; i < INDICE_MAX_TABLEAU; i++)   // T − 1 tours
    tableau[i] = tableau[i + 1];                // 1 affectation
tableau[INDICE_MAX_TABLEAU] = valeur;           // 1 affectation
```

Coût : (T − 1) × 1 + 1 = **T**

**`Calcul_Moy_Instant(tableau)`**

```cpp
float moyenne = 0;                              // 1
for (int i = 0; i < TAILLE_TABLEAU; i++)        // T tours
    moyenne += tableau[i];                      // 1 addition
return moyenne / TAILLE_TABLEAU;                // 1 division
```

Coût : 1 + T + 1 = **T + 2**

**`Lecture_Capteurs()`** : une boucle de N tours, avec dans chaque tour :
`Tab_Erreur = false` (1), lecture du capteur (1), test (1), affectation de `Tab_Mesures` ou de `Tab_Erreur` (1), soit 4.

Coût : **4N**

**`Maj_Mesures()`** : une boucle de N tours, avec dans chaque tour un test (1) et un appel à `Decalage` (T).

Coût : **N(T + 1)**

**Calcul des moyennes dans `loop()`** : une boucle de N tours, avec dans chaque tour un appel à `Calcul_Moy_Instant` (T + 2) et l'affectation dans `Tab_Moy_Instant` (1).

Coût : **N(T + 3)**

### 3.4 Total pour un tour de `loop()`

```
C₁(N, T) = 4N + N(T + 1) + N(T + 3)
         = N(2T + 8)
```

**Valeur avec N = 4 et T = 10 :**

```
C₁ = 4 × (2 × 10 + 8) = 4 × 28 = 112 opérations par tour de loop()
```

**Complexité : O(N·T).** Le coût est proportionnel au nombre de capteurs multiplié par la taille de l'historique. Si on double T (moyenne sur 20 mesures), on double presque le temps de calcul.

Remarques :
- **Pire cas** : tous les capteurs répondent, donc tous les décalages sont faits. C'est le cas calculé ci-dessus. Si un capteur est en erreur, son `Decalage` est sauté, mais l'ordre de grandeur reste le même.
- `setup()` coûte aussi O(N·T) (mise à zéro des tableaux), mais une seule fois au démarrage, donc on ne le compte pas dans le coût de fonctionnement.
- N et T sont des constantes fixées à la compilation. Sur la carte, le coût d'un tour est donc fixe. La notation O(N·T) indique comment il évolue si on modifie ces paramètres, ce qui compte parce que le système est censé être évolutif.

### 3.5 Complexité en mémoire

Sur Arduino (AVR) : `float` = 4 octets, `bool` = 1 octet, `int` = 2 octets.

| Donnée | Taille | Octets (N = 4, T = 10) |
|---|---|---|
| `Tab_Mesures[N]` | 4N | 16 |
| `Tab_Erreur[N]` | N | 4 |
| `Tab_Moy_Instant[N]` | 4N | 16 |
| `Tab_Capteurs[N][T]` | 4NT | 160 |
| **Total** | **N(4T + 9)** | **196 octets** |

**Complexité en mémoire : O(N·T)**

### 3.6 Problèmes identifiés

1. **`Decalage` recopie tout l'historique** à chaque nouvelle mesure (T copies), alors qu'une seule valeur change.
2. **`Calcul_Moy_Instant` refait toute la somme** à chaque tour (T additions), alors que la somme ne change que d'une valeur entrante et d'une valeur sortante.
3. **Des données sont stockées en double :**
   - `Tab_Mesures` sert seulement d'intermédiaire entre la lecture et `Maj_Mesures`. La valeur est ensuite recopiée dans l'historique, donc elle existe deux fois ;
   - `Tab_Moy_Instant` contient une valeur qu'on peut recalculer directement à partir de la somme de l'historique.

---

## 4. Version 2 (fichier `pseudocode.cpp`)

On part de la version 1 et on change **seulement** la complexité et la redondance des données. Les types (`float`) et l'organisation en tableaux restent les mêmes.

### 4.1 Changements

**a) Buffer circulaire au lieu du décalage**

Au lieu de décaler tout le tableau, on garde pour chaque capteur l'indice de la case la plus ancienne (`Tab_Index`). La nouvelle mesure écrase cette case, puis l'indice avance d'une case et revient à 0 après la dernière.
L'ordre des valeurs dans le tableau n'a pas d'importance, puisqu'on ne s'en sert que pour la moyenne.

**b) Somme glissante**

On garde la somme de l'historique (`Tab_Somme`). À chaque nouvelle mesure :

```
somme = somme + nouvelle_valeur − valeur_écrasée
```

La moyenne devient alors une simple division : `Tab_Somme[c] / TAILLE_TABLEAU`.

**c) Suppression des redondances**

| Donnée supprimée | Pourquoi |
|---|---|
| `Tab_Mesures` | La mesure est écrite directement dans l'historique. La lecture et la mise à jour se font dans une seule boucle (`Lecture_Capteurs`), donc `Maj_Mesures` disparaît aussi. |
| `Tab_Moy_Instant` | La moyenne se calcule en O(1) depuis `Tab_Somme`, il n'y a plus besoin de la stocker. |

`Tab_Erreur` est conservé, car c'est une information qui n'existe nulle part ailleurs.

### 4.2 Complexité de la version 2

On compte de la même façon qu'à la section 3.

**`Decalage(num_capteur, valeur)`** : lire l'indice (1), mettre à jour la somme (2 : une soustraction et une addition), écrire la valeur (1), incrémenter l'indice (1), le tester (1), l'enregistrer (1).

Coût : **7**, quel que soit T

**`Calcul_Moy_Instant(num_capteur)`** : une division.

Coût : **1**

**`Lecture_Capteurs()`** : une boucle de N tours, avec dans chaque tour `Tab_Erreur = false` (1), la lecture (1), le test (1) et l'appel à `Decalage` (7), soit 10.

Coût : **10N**

**Calcul des moyennes dans `loop()`** : N × 1 = **N**

```
C₂(N) = 10N + N = 11N
```

Avec N = 4 : **C₂ = 44 opérations par tour**, contre 112 pour la version 1.

**Complexité : O(N)**. Le coût ne dépend plus de T.

| T (taille de l'historique) | Version 1 : N(2T + 8) | Version 2 : 11N | Gain |
|---|---|---|---|
| 10 | 112 | 44 | ≈ 2,5× |
| 50 | 432 | 44 | ≈ 10× |
| 100 | 832 | 44 | ≈ 19× |

### 4.3 Mémoire de la version 2

| Donnée | Taille | Octets (N = 4, T = 10) |
|---|---|---|
| `Tab_Erreur[N]` | N | 4 |
| `Tab_Capteurs[N][T]` | 4NT | 160 |
| `Tab_Index[N]` | 2N | 8 |
| `Tab_Somme[N]` | 4N | 16 |
| **Total** | **N(4T + 7)** | **188 octets** |

La mémoire baisse un peu (196 → 188 octets) : on a supprimé deux tableaux redondants, mais ajouté l'indice et la somme. Ça reste en **O(N·T)**, parce que l'historique est indispensable pour connaître la valeur qui sort de la moyenne.
Le gain principal de la version 2 est donc sur le **temps**. Pour gagner vraiment de la mémoire, il faut changer les types (section 6).

### 4.4 Vérification

Le programme a été compilé et exécuté sur PC, avec une fausse bibliothèque Arduino qui renvoie des valeurs fixes. Les moyennes affichées sont celles qu'on attend de la version 1 : elles montent progressivement pendant les 10 premiers tours (l'historique part de 0), puis se stabilisent à la valeur mesurée.

Point d'attention : avec des `float`, la somme glissante peut accumuler de petites erreurs d'arrondi sur un très grand nombre de mesures à virgule. Ce problème disparaît avec des entiers (section 6).

---

## 5. Proposition de code étudiée (rejetée)

Une proposition de code (5 capteurs, tableaux `tab_C1` à `tab_C5`, `tab_er_C1` à `tab_er_C5`) a été analysée. Elle contient deux bonnes idées :

- revenir à l'indice 0 en fin de tableau : c'est le principe du buffer circulaire ;
- un tableau de pointeurs vers les historiques, pour éviter les copies.

Mais elle ne répond à aucune critique de la supervision et ne compile pas :

- elle ajoute des tableaux au lieu de les remplacer par des structures ;
- les erreurs sont stockées sur 50 `int` au lieu d'1 bit par capteur ;
- elle utilise encore `float`, avec une division entière qui tronque ;
- elle contient des erreurs de C : tableaux `[4]` initialisés avec 5 éléments, type `str` inexistant, opérateur virgule dans `(a, b, c…)/5`, `return` en dehors de la fonction, indices `i+6` hors du tableau, `si i=9` (affectation au lieu de comparaison) ;
- la boucle `i` est remise à 0 à l'intérieur d'elle-même, ce qui crée une boucle infinie ;
- `j` va de 1 à 4, donc seulement 4 capteurs sur 5 sont traités.

---

## 6. Pistes pour la version finale (pas encore appliquées)

Un prototype a été écrit pour répondre aux autres remarques de la supervision. Il est mis de côté pour l'instant :

| Remarque de la supervision | Piste |
|---|---|
| Remplacer les tableaux par des structures | Une `struct Capteur` qui regroupe l'historique, la somme, l'indice et l'erreur d'un capteur. On passe un **pointeur** sur la structure, donc il n'y a aucune copie. |
| Mémoire limitée | Des **champs de bits** : `erreur : 1` occupe vraiment 1 bit, `index : 3` occupe 3 bits. |
| Pas besoin de réels | Des entiers `int16_t`, éventuellement en virgule fixe (23,5 °C stocké comme 235). Ça divise par deux la mémoire de l'historique, et les calculs sont beaucoup plus rapides sur un AVR qui n'a pas d'unité de calcul flottant. |
| Temps minimal | Une taille d'historique en **puissance de 2** (8 ou 16) : la division devient un décalage de bits et le retour à 0 de l'indice se fait tout seul. |

Estimation pour N = 4 et T = 8 : environ **80 octets**, contre 196 pour la version 1, avec un coût en **O(N)**.
