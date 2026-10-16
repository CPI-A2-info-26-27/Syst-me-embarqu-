#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define PIN_BTN_VERT      5
#define PIN_BTN_ROUGE     6
#define PIN_LED_CLK       8
#define PIN_LED_DATA      9
#define PIN_GPS_RX        2
#define PIN_GPS_TX        3
#define PIN_SD_CS         4
#define ADR_RTC           0x68
#define ADR_BME280        0x76
#define PIN_LUMINOSITE    A0

#define BTN_N   0
#define BTN_RC  1
#define BTN_RL  2
#define BTN_VC  3
#define BTN_VL  4

#define NA_TEMP      0x01
#define NA_PRESSURE  0x02
#define NA_HYGR      0x04
#define NA_LUMIN     0x08
#define NA_GPS       0x10
#define NA_RTC       0x20

uint16_t LOG_INTERVAL  = 60;
uint16_t FILE_MAX_SIZE = 2048;
uint8_t  TIMEOUT       = 30;

const int16_t LUMIN_LOW    = 255;  const int16_t LUMIN_HIGH   = 768;
const int8_t  MIN_TEMP_AIR = -10;  const int8_t  MAX_TEMP_AIR = 60;
const uint8_t HYGR_MINT    = 0;    const uint8_t HYGR_MAXT    = 50;
const int16_t PRESSURE_MIN = 850;  const int16_t PRESSURE_MAX = 1080;

uint16_t LUMIN    = 0;
float    TEMP_AIR = 0.0f;
float    HYGR     = 0.0f;
float    PRESSURE = 0.0f;
float    GPS_LAT  = 0.0f;
float    GPS_LON  = 0.0f;
DateTime RTC_TIME;
uint8_t  NA_FLAGS = 0;

char FILE_PATH[13] = "";
uint8_t COMPTEUR_ECO = 0;
char MODE = 'S';
char MODE_MEMOIRE = 'S';
uint8_t num_revision = 0;        // revision du fichier du jour (AAMMJJ_N.LOG)
uint8_t jour_fichier = 0;        // jour du fichier en cours, pour remettre la revision a 0
bool premier_releve = true;      // force un releve des le premier passage dans loop()
uint32_t dernier_releve = 0;
uint32_t chrono = 0;
uint32_t derniere_activite_config = 0;   // dernier appui en mode configuration
const uint32_t DUREE_INACTIVITE_CONFIG = 30UL * 60UL * 1000UL;   // 30 min sans appui => retour en standard
uint8_t ETAT_BOUTON = BTN_N;

Adafruit_BME280 bme;
RTC_DS1307 rtc;
SoftwareSerial gpsSerial(PIN_GPS_RX, PIN_GPS_TX);
TinyGPSPlus gps;

ChainableLED ledRGB(PIN_LED_CLK, PIN_LED_DATA, 1);

typedef struct { uint8_t r, g, b; } Couleur;

const Couleur VERT   PROGMEM = { 0,   255, 0   };
const Couleur JAUNE  PROGMEM = { 255, 255, 0   };
const Couleur BLEU   PROGMEM = { 0,   0,   255 };
const Couleur ORANGE PROGMEM = { 255, 100, 0   };
const Couleur ROUGE  PROGMEM = { 255, 0,   0   };
const Couleur BLANC  PROGMEM = { 255, 255, 255 };
const Couleur ETEINT PROGMEM = { 0,   0,   0   };

void led_color(const Couleur *c)
{
  ledRGB.setColorRGB(0, pgm_read_byte(&c->r), pgm_read_byte(&c->g), pgm_read_byte(&c->b));
}

typedef struct { const Couleur *c1; const Couleur *c2; uint16_t d1; uint16_t d2; } Erreur;

const Erreur ERR_RTC         PROGMEM = { &ROUGE, &BLEU,  1000, 1000 };
const Erreur ERR_GPS         PROGMEM = { &ROUGE, &JAUNE, 1000, 1000 };
const Erreur ERR_ACCES_CAP   PROGMEM = { &ROUGE, &VERT,  1000, 1000 };
const Erreur ERR_VALEURS_CAP PROGMEM = { &ROUGE, &VERT,  1000, 2000 };
const Erreur ERR_FULL_SD     PROGMEM = { &ROUGE, &BLANC, 1000, 1000 };
const Erreur ERR_WRITE_SD    PROGMEM = { &ROUGE, &BLANC, 1000, 2000 };

const Erreur *erreur_courante = NULL;
bool     erreur_phase2 = false;       // false : couleur 1, true : couleur 2
uint32_t debut_phase_erreur = 0;

void SignalerErreur(const Erreur *e)
{
  if (erreur_courante == NULL)
  {
    erreur_courante = e;
    erreur_phase2 = false;
    debut_phase_erreur = millis();
    led_color((const Couleur *)pgm_read_word(&e->c1));
  }
}

// Clignotement non bloquant : les boutons restent lus pendant l'affichage de l'erreur
void erreur()
{
  uint16_t duree = pgm_read_word(erreur_phase2 ? &erreur_courante->d2 : &erreur_courante->d1);
  if (millis() - debut_phase_erreur >= duree)
  {
    erreur_phase2 = !erreur_phase2;
    debut_phase_erreur = millis();
    led_color((const Couleur *)pgm_read_word(erreur_phase2 ? &erreur_courante->c2 : &erreur_courante->c1));
  }
}

extern uint8_t __heap_start;
extern uint8_t *__brkval;
#define MOTIF_PILE 0xAA

void PeindrePile()
{
  uint8_t marqueur;
  uint8_t *p = __brkval ? __brkval : &__heap_start;
  while (p < &marqueur - 16) *p++ = MOTIF_PILE;
}

uint16_t PileMinLibre()
{
  uint8_t marqueur;
  uint8_t *p = __brkval ? __brkval : &__heap_start;
  uint16_t libre = 0;
  uint16_t max_libre = 0;
  for (; p < &marqueur; p++)
  {
    if (*p == MOTIF_PILE)
    {
      libre++;
      if (libre > max_libre) max_libre = libre;
    }
    else
    {
      libre = 0;
    }
  }
  return max_libre;
}

// Cablage des boutons :
//  - bouton entre la broche et GND (aucune resistance)    -> BTN_APPUYE LOW  (INPUT_PULLUP)
//  - bouton entre la broche et 5V + resistance 10k vers GND -> BTN_APPUYE HIGH (INPUT)
#define BTN_APPUYE  HIGH

bool Appuye(uint8_t pin)
{
  return digitalRead(pin) == BTN_APPUYE;
}

uint8_t BOUTON()
{
  uint8_t pin;
  if (Appuye(PIN_BTN_ROUGE)) pin = PIN_BTN_ROUGE;
  else if (Appuye(PIN_BTN_VERT)) pin = PIN_BTN_VERT;
  else return BTN_N;

  delay(50);
  if (!Appuye(pin)) return BTN_N;

  chrono = millis();
  while (millis() - chrono < 5000UL)
  {
    if (!Appuye(pin))
    {
      delay(50);
      return (pin == PIN_BTN_ROUGE) ? BTN_RC : BTN_VC;
    }
  }
  while (Appuye(pin)) {}
  delay(50);
  return (pin == PIN_BTN_ROUGE) ? BTN_RL : BTN_VL;
}

bool PresentI2C(uint8_t adr)
{
  Wire.beginTransmission(adr);
  return Wire.endTransmission() == 0;
}

bool rtc_initialisee = false;
bool bme_initialise = false;

bool TestRTC()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (PresentI2C(ADR_RTC))
    {
      if (!rtc_initialisee) rtc_initialisee = rtc.begin();
      if (rtc_initialisee)
      {
        if (!rtc.isrunning())
        {
          rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
      }
    }
  }
  return false;
}

bool TestBME280()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (PresentI2C(ADR_BME280))
    {
      if (!bme_initialise) bme_initialise = bme.begin(ADR_BME280);
      if (bme_initialise) return true;
    }
  }
  return false;
}

bool TestLuminosite()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    LUMIN = analogRead(PIN_LUMINOSITE);
    if (LUMIN <= 1023)
    {
      return true;
    }
  }
  return false;
}

bool TestGPS()
{
  uint32_t trames_avant = gps.passedChecksum();
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    while (gpsSerial.available())
    {
      if (gps.encode(gpsSerial.read()) && gps.location.isUpdated()) return true;
    }
    if (gps.passedChecksum() != trames_avant && millis() - chrono >= 2000UL) return true;
  }
  return gps.passedChecksum() != trames_avant;
}

bool sd_initialisee = false;

bool TestSDAcces()
{
  if (sd_initialisee) return true;
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (SD.begin(PIN_SD_CS))
    {
      sd_initialisee = true;
      return true;
    }
  }
  return false;
}

void NomFichier()
{
  FILE_PATH[0]  = '0' + (RTC_TIME.year() % 100) / 10;
  FILE_PATH[1]  = '0' + RTC_TIME.year() % 10;
  FILE_PATH[2]  = '0' + RTC_TIME.month() / 10;
  FILE_PATH[3]  = '0' + RTC_TIME.month() % 10;
  FILE_PATH[4]  = '0' + RTC_TIME.day() / 10;
  FILE_PATH[5]  = '0' + RTC_TIME.day() % 10;
  FILE_PATH[6]  = '_';
  FILE_PATH[7]  = '0' + num_revision % 10;
  FILE_PATH[8]  = '.';
  FILE_PATH[9]  = 'L';
  FILE_PATH[10] = 'O';
  FILE_PATH[11] = 'G';
  FILE_PATH[12] = '\0';
}

bool TemperatureCoherente()
{
  return TEMP_AIR >= MIN_TEMP_AIR && TEMP_AIR <= MAX_TEMP_AIR;
}

bool PressionCoherente()
{
  return PRESSURE >= PRESSURE_MIN && PRESSURE <= PRESSURE_MAX;
}

bool HumiditeCoherente()
{
  return HYGR >= 0.0f && HYGR <= 100.0f;
}

bool LuminositeCoherente()
{
  return LUMIN <= 1023;
}

void Mesurer(bool avec_gps)
{
  NA_FLAGS = 0;
  if (TestBME280())
  {
    TEMP_AIR = bme.readTemperature();
    PRESSURE = bme.readPressure() / 100.0f;
    HYGR = bme.readHumidity();
    if (!TemperatureCoherente())
    {
      SignalerErreur(&ERR_VALEURS_CAP);
      NA_FLAGS |= NA_TEMP;
    }
    if (!PressionCoherente())
    {
      SignalerErreur(&ERR_VALEURS_CAP);
      NA_FLAGS |= NA_PRESSURE;
    }
    if (!HumiditeCoherente())
    {
      SignalerErreur(&ERR_VALEURS_CAP);
      NA_FLAGS |= NA_HYGR;
    }
  }
  else
  {
    SignalerErreur(&ERR_ACCES_CAP);
    NA_FLAGS |= NA_TEMP | NA_PRESSURE | NA_HYGR;
  }
  if (TestLuminosite())
  {
    if (!LuminositeCoherente())
    {
      SignalerErreur(&ERR_VALEURS_CAP);
      NA_FLAGS |= NA_LUMIN;
    }
  }
  else
  {
    SignalerErreur(&ERR_ACCES_CAP);
    NA_FLAGS |= NA_LUMIN;
  }
  if (avec_gps)
  {
    if (TestGPS())
    {
      if (gps.location.isUpdated())
      {
        GPS_LAT = gps.location.lat();
        GPS_LON = gps.location.lng();
      }
      else
      {
        NA_FLAGS |= NA_GPS;
      }
    }
    else
    {
      SignalerErreur(&ERR_GPS);
      NA_FLAGS |= NA_GPS;
    }
  }
  if (TestRTC())
  {
    RTC_TIME = rtc.now();
  }
  else
  {
    SignalerErreur(&ERR_RTC);
    NA_FLAGS |= NA_RTC;
  }
}

void Ecrire(Print &sortie, bool avec_gps)
{
  sortie.print(F("Date/Heure: "));
  if (NA_FLAGS & NA_RTC) sortie.print(F("NA"));
  else
  {
    sortie.print(RTC_TIME.year(), DEC);
    sortie.print('/');
    sortie.print(RTC_TIME.month(), DEC);
    sortie.print('/');
    sortie.print(RTC_TIME.day(), DEC);
    sortie.print(F(" - "));
    sortie.print(RTC_TIME.hour(), DEC);
    sortie.print(':');
    sortie.print(RTC_TIME.minute(), DEC);
    sortie.print(':');
    sortie.print(RTC_TIME.second(), DEC);
  }
  if (avec_gps)
  {
    sortie.print(F(" | GPS lat, long : "));
    if (NA_FLAGS & NA_GPS) sortie.print(F("NA, NA"));
    else
    {
      sortie.print(GPS_LAT, 6);
      sortie.print(F(", "));
      sortie.print(GPS_LON, 6);
    }
  }
  sortie.print(F(" | Température : "));
  if (NA_FLAGS & NA_TEMP) sortie.print(F("NA")); else sortie.print(TEMP_AIR);
  sortie.print(F(" | Pression : "));
  if (NA_FLAGS & NA_PRESSURE) sortie.print(F("NA")); else sortie.print(PRESSURE);
  sortie.print(F(" | Humidité : "));
  if (NA_FLAGS & NA_HYGR) sortie.print(F("NA")); else sortie.print(HYGR);
  sortie.print(F(" | Luminosité : "));
  if (NA_FLAGS & NA_LUMIN) sortie.print(F("NA")); else sortie.print(LUMIN);
  sortie.println();
}

void EcrireSD(bool avec_gps)
{
  if (!TestSDAcces())
  {
    SignalerErreur(&ERR_WRITE_SD);
    return;
  }
  if (RTC_TIME.day() != jour_fichier)
  {
    jour_fichier = RTC_TIME.day();
    num_revision = 0;
  }
  NomFichier();
  while (num_revision < 9 && SD.exists(FILE_PATH))
  {
    File existant = SD.open(FILE_PATH, FILE_READ);
    uint32_t taille = existant ? existant.size() : 0;
    if (existant) existant.close();
    if (taille < FILE_MAX_SIZE) break;
    num_revision += 1;
    NomFichier();
  }
  File fichier = SD.open(FILE_PATH, FILE_WRITE);
  if (!fichier)
  {
    SignalerErreur(&ERR_WRITE_SD);
    return;
  }
  Ecrire(fichier, avec_gps);
  if (fichier.size() >= FILE_MAX_SIZE)
  {
    if (num_revision < 9) num_revision += 1;    // fichier plein => revision suivante
    else SignalerErreur(&ERR_FULL_SD);          // plus de numero de revision disponible
  }
  fichier.close();
}

void ModeStandard()
{
  led_color(&VERT);
  LOG_INTERVAL = 600;
  Mesurer(true);
  EcrireSD(true);
}

void ModeMaintenance()
{
  led_color(&ORANGE);
  Mesurer(true);
  Ecrire(Serial, true);
}

void ModeConfiguration()
{
  led_color(&JAUNE);
  LOG_INTERVAL = 1800;
  dernier_releve = millis();
}

void ModeEconomie()
{
  led_color(&BLEU);
  LOG_INTERVAL = 1200;
  bool avec_gps = (COMPTEUR_ECO % 2 == 0);
  Mesurer(avec_gps);
  EcrireSD(avec_gps);
  COMPTEUR_ECO += 1;
}

// Standard     : rouge long -> maintenance, vert long -> economie
// Economie     : rouge long -> standard
// Maintenance  : rouge long -> retour au mode precedent (standard)
// Configuration: uniquement au demarrage (rouge appuye), sortie apres 30 min sans appui
void ChoixMode()
{
  ETAT_BOUTON = BOUTON();
  if (MODE == 'S')
  {
    if (ETAT_BOUTON == BTN_RL)
    {
      MODE_MEMOIRE = 'S';
      MODE = 'M';
    }
    else if (ETAT_BOUTON == BTN_VL)
    {
      MODE = 'E';
    }
  }
  else if (MODE == 'E')
  {
    if (ETAT_BOUTON == BTN_RL)
    {
      MODE = 'S';
    }
  }
  else if (MODE == 'M')
  {
    if (ETAT_BOUTON == BTN_RL)
    {
      MODE = MODE_MEMOIRE;
    }
  }
  else if (MODE == 'C')
  {
    if (ETAT_BOUTON != BTN_N)
    {
      derniere_activite_config = millis();
    }
    else if (millis() - derniere_activite_config >= DUREE_INACTIVITE_CONFIG)
    {
      MODE = 'S';
    }
  }
}

void setup() {
  PeindrePile();
  ledRGB.init();
  Serial.begin(9600);
  gpsSerial.begin(9600);
  Wire.begin();
  pinMode(PIN_BTN_ROUGE, BTN_APPUYE == LOW ? INPUT_PULLUP : INPUT);
  pinMode(PIN_BTN_VERT, BTN_APPUYE == LOW ? INPUT_PULLUP : INPUT);
  pinMode(PIN_LUMINOSITE, INPUT);
  sd_initialisee = SD.begin(PIN_SD_CS);
  
  if (Appuye(PIN_BTN_ROUGE))
  {
    MODE = 'C';
    derniere_activite_config = millis();
    while (Appuye(PIN_BTN_ROUGE)) {}
    delay(50);
  }
  else
  {
    MODE = 'S';
  }
  Serial.print(F("Demarrage en mode "));
  Serial.println(MODE);
}

void loop()
{
  char ancien_mode = MODE;
  ChoixMode();
  if (ETAT_BOUTON != BTN_N)
  {
    Serial.print(F("Bouton : "));
    Serial.print(ETAT_BOUTON);
    Serial.print(F(" -> mode "));
    Serial.println(MODE);
  }
  if (premier_releve || MODE != ancien_mode || millis() - dernier_releve >= LOG_INTERVAL * 1000UL)
  {
    premier_releve = false;
    dernier_releve = millis();
    erreur_courante = NULL;
    if (MODE == 'S')
    {
      ModeStandard();
    }
    else if (MODE == 'E')
    {
      ModeEconomie();
    }
    else if (MODE == 'M')
    {
      ModeMaintenance();
    }
    else if (MODE == 'C')
    {
      ModeConfiguration();
    }
    if (MODE == 'S' || MODE == 'E')
    {
      Ecrire(Serial, true);
    }
    Serial.print(F("Le mode "));
    Serial.print(MODE);
    Serial.print(F(" est termine (pile : "));
    Serial.print(PileMinLibre());
    Serial.println(F(" octets libres au minimum)"));
  }
  if (erreur_courante != NULL)
  {
    erreur();
  }
  else
  {
    delay(100);
  }
}
