#include <Arduino.h>
#define LED_PIN LED_BUILTIN
#define BUTTON_PIN 2
#define BUTTON_PRESSED LOW

// AJOUT 1 : variables partagées entre l'interruption et le programme principal.
// Elles sont déclarées "volatile" car l'interruption les modifie à tout moment,
// sans que loop() le sache : le programme doit donc relire leur valeur à chaque accès.
volatile unsigned long debutAppui = 0;   // instant (en ms) où le bouton a été appuyé
volatile unsigned long dureeAppui = 0;   // durée du dernier appui (en ms)
volatile bool appuiTermine = false;      // vrai quand un appui est fini et que sa durée est prête à être lue

void acquisition();

// AJOUT 2 : routine d'interruption (ISR).
// Elle est appelée automatiquement à chaque changement d'état du bouton (appui ou relâchement),
// même si le programme est en train d'attendre dans acquisition().
// Elle doit rester très courte pour ne pas retarder le programme principal.
void isrBouton() {
  unsigned long maintenant = millis();               // instant actuel, en ms depuis le démarrage de la carte

  if (digitalRead(BUTTON_PIN) == BUTTON_PRESSED) {   // le bouton vient d'être appuyé
    debutAppui = maintenant;                          // on mémorise l'instant de début de l'appui
    digitalWrite(LED_PIN, HIGH);                      // la LED s'allume immédiatement
  } else {                                            // le bouton vient d'être relâché
    dureeAppui = maintenant - debutAppui;             // durée de l'appui = instant de fin - instant de début
    appuiTermine = true;                              // on signale à loop() qu'une durée est disponible
    digitalWrite(LED_PIN, LOW);                       // la LED s'éteint immédiatement
  }
}

void setup() {
  Serial.begin(9600);                                 // AJOUT 3 : active la liaison série pour afficher la durée sur l'ordinateur
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  // AJOUT 4 : associe l'interruption au bouton.
  // Dès que l'état de la broche change (CHANGE = appui ou relâchement), isrBouton() est exécutée.
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isrBouton, CHANGE);
}

void loop() {
  acquisition();

  // AJOUT 5 : à la fin de l'acquisition, on vérifie si un appui s'est terminé.
  if (appuiTermine) {
    // On copie la durée en désactivant brièvement les interruptions : la variable fait 4 octets,
    // et si l'ISR la modifiait pendant la lecture, on pourrait obtenir une valeur incohérente.
    noInterrupts();
    unsigned long duree = dureeAppui;                 // copie de la durée mesurée par l'ISR
    appuiTermine = false;                             // on remet l'indicateur à zéro
    interrupts();                                     // on réactive les interruptions

    Serial.print("Appui de ");
    Serial.print(duree);
    Serial.println(" ms");
  }
}

void acquisition() {
  unsigned long acquisition_duration = 3000UL + (millis() % 4000UL);
  delay(acquisition_duration);
}