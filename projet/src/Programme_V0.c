#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define PIN_BTN_ROUGE     5
#define PIN_BTN_VERT      6
#define PIN_LED_CLK       7
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

uint16_t LOG_INTERVAL  = 600;
uint16_t FILE_MAX_SIZE = 2048;
uint8_t  TIMEOUT       = 30;

// Seuils fixes : const => pas de RAM utilisee
const int16_t LUMIN_LOW    = 255;  const int16_t LUMIN_HIGH   = 768;
const int8_t  MIN_TEMP_AIR = -10;  const int8_t  MAX_TEMP_AIR = 60;
const uint8_t HYGR_MINT    = 0;    const uint8_t HYGR_MAXT    = 50;
const int16_t PRESSURE_MIN = 850;  const int16_t PRESSURE_MAX = 1080;

// Mesures : une seule copie en RAM, ecrite directement par les fonctions
uint16_t LUMIN    = 0;
float    TEMP_AIR = 0.0f;
float    HYGR     = 0.0f;
float    PRESSURE = 0.0f;
float    GPS_LAT  = 0.0f;
float    GPS_LON  = 0.0f;
DateTime RTC_TIME;
uint8_t  NA_FLAGS = 0;              // 1 bit par mesure "NA" au lieu de chaines

char FILE_PATH[13] = "";            // AAMMJJ_0.LOG + '\0'
uint8_t BIN = 0;
char MODE = 'S';
char MODE_MEMOIRE = 'S';
bool debut = true;
uint32_t dernier_releve = 0;
uint32_t chrono = 0;                // chrono partage par BOUTON() et les Test*()
uint8_t ETAT_BOUTON = BTN_N;
File dataFile;

Adafruit_BME280 bme;
RTC_DS1307 rtc;
SoftwareSerial gpsSerial(PIN_GPS_RX, PIN_GPS_TX);
TinyGPSPlus gps;

ChainableLED ledRGB(8,9,1);

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

const Erreur *erreur_courante = NULL;   // pointeur (2 octets) vers l'erreur en flash

void erreur()
{
  led_color((const Couleur *)pgm_read_word(&erreur_courante->c1));
  delay(pgm_read_word(&erreur_courante->d1));
  led_color((const Couleur *)pgm_read_word(&erreur_courante->c2));
  delay(pgm_read_word(&erreur_courante->d2));
}

uint8_t BOUTON()
{
  chrono = millis();
  if (digitalRead(PIN_BTN_ROUGE) == LOW)
  {
    while (millis() - chrono < 5000UL)
    {
      if (digitalRead(PIN_BTN_ROUGE) == HIGH)
      {
        return BTN_RC;
      }
    }
    return BTN_RL;
  }
  if (digitalRead(PIN_BTN_VERT) == LOW)
  {
    while (millis() - chrono < 5000UL)
    {
      if (digitalRead(PIN_BTN_VERT) == HIGH)
      {
        return BTN_VC;
      }
    }
    return BTN_VL;
  }
  return BTN_N;
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
  return HYGR >= HYGR_MINT && HYGR <= HYGR_MAXT;
}

bool LuminositeCoherente()
{
  return LUMIN >= LUMIN_LOW && LUMIN <= LUMIN_HIGH;
}

bool TestRTC()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (rtc.begin())
    {
      return true;
    }
  }
  return false;
}

bool TestBME280()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (bme.begin(ADR_BME280))
    {
      return true;
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
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    while (gpsSerial.available())
    {
      if (gps.encode(gpsSerial.read()) && gps.location.isValid()) return true;
    }
  }
  return false;
}

bool TestSDAcces()
{
  chrono = millis();

  while (millis() - chrono < TIMEOUT * 1000UL)
  {
    if (SD.begin(PIN_SD_CS)) return true;
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
  FILE_PATH[7]  = '0';
  FILE_PATH[8]  = '.';
  FILE_PATH[9]  = 'L';
  FILE_PATH[10] = 'O';
  FILE_PATH[11] = 'G';
  FILE_PATH[12] = '\0';
}

void ModeStandard()
{
  led_color(&VERT);
  LOG_INTERVAL = 600;
  NA_FLAGS = 0;
  if (TestBME280())
  {
    TEMP_AIR = bme.readTemperature();
    PRESSURE = bme.readPressure() / 100.0f;
    HYGR = bme.readHumidity();
    if (!TemperatureCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_TEMP;
    }
    if (!PressionCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_PRESSURE;
    }
    if (!HumiditeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_HYGR;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_TEMP | NA_PRESSURE | NA_HYGR;
  }
  if (TestLuminosite())
  {
    if (!LuminositeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_LUMIN;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_LUMIN;
  }
  if (TestGPS())
  {
    GPS_LAT = gps.location.lat();
    GPS_LON = gps.location.lng();
  }
  else
  {
    erreur_courante = &ERR_GPS;
    NA_FLAGS |= NA_GPS;
  }
  if (TestRTC())
  {
    RTC_TIME = rtc.now();
  }
  else
  {
    erreur_courante = &ERR_RTC;
    NA_FLAGS |= NA_RTC;
  }
  if (TestSDAcces())
  {
    NomFichier();
    dataFile = SD.open(FILE_PATH, FILE_WRITE);
    if (!dataFile)
    {
      erreur_courante = &ERR_WRITE_SD;
      return;
    }
    dataFile.print(F("Date/Heure: "));
    dataFile.print(RTC_TIME.year(), DEC);
    dataFile.print('/');
    dataFile.print(RTC_TIME.month(), DEC);
    dataFile.print('/');
    dataFile.print(RTC_TIME.day(), DEC);
    dataFile.print(F(" - "));
    dataFile.print(RTC_TIME.hour(), DEC);
    dataFile.print(':');
    dataFile.print(RTC_TIME.minute(), DEC);
    dataFile.print(':');
    dataFile.print(RTC_TIME.second(), DEC);
    dataFile.print(F(" | GPS lat, long : "));
    if (NA_FLAGS & NA_GPS) dataFile.print(F("NA, NA"));
    else
    {
      dataFile.print(GPS_LAT, 6);
      dataFile.print(F(", "));
      dataFile.print(GPS_LON, 6);
    }
    dataFile.print(F(" | Température : "));
    if (NA_FLAGS & NA_TEMP) dataFile.print(F("NA")); else dataFile.print(TEMP_AIR);
    dataFile.print(F(" | Pression : "));
    if (NA_FLAGS & NA_PRESSURE) dataFile.print(F("NA")); else dataFile.print(PRESSURE);
    dataFile.print(F(" | Humidité : "));
    if (NA_FLAGS & NA_HYGR) dataFile.print(F("NA")); else dataFile.print(HYGR);
    dataFile.print(F(" | Luminosité : "));
    if (NA_FLAGS & NA_LUMIN) dataFile.print(F("NA")); else dataFile.print(LUMIN);
    dataFile.println();
    if (dataFile.size() >= FILE_MAX_SIZE)
    {
      erreur_courante = &ERR_FULL_SD;
    }
    dataFile.close();
  }
  else
  {
    erreur_courante = &ERR_WRITE_SD;
  }
}

void ModeMaintenance()
{
  led_color(&ORANGE);
  NA_FLAGS = 0;
  if (TestBME280())
  {
    TEMP_AIR = bme.readTemperature();
    PRESSURE = bme.readPressure() / 100.0f;
    HYGR = bme.readHumidity();
    if (!TemperatureCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_TEMP;
    }
    if (!PressionCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_PRESSURE;
    }
    if (!HumiditeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_HYGR;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_TEMP | NA_PRESSURE | NA_HYGR;
  }
  if (TestLuminosite())
  {
    if (!LuminositeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_LUMIN;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_LUMIN;
  }
  if (TestGPS())
  {
    GPS_LAT = gps.location.lat();
    GPS_LON = gps.location.lng();
  }
  else
  {
    erreur_courante = &ERR_GPS;
    NA_FLAGS |= NA_GPS;
  }
  if (TestRTC())
  {
    RTC_TIME = rtc.now();
  }
  else
  {
    erreur_courante = &ERR_RTC;
    NA_FLAGS |= NA_RTC;
  }
  Serial.print(F("Date/Heure: "));
  Serial.print(RTC_TIME.year(), DEC);
  Serial.print('/');
  Serial.print(RTC_TIME.month(), DEC);
  Serial.print('/');
  Serial.print(RTC_TIME.day(), DEC);
  Serial.print(F(" - "));
  Serial.print(RTC_TIME.hour(), DEC);
  Serial.print(':');
  Serial.print(RTC_TIME.minute(), DEC);
  Serial.print(':');
  Serial.print(RTC_TIME.second(), DEC);
  Serial.println();
  Serial.print(F(" | GPS lat, long : "));
  if (NA_FLAGS & NA_GPS) Serial.print(F("NA, NA"));
  else
  {
    Serial.print(GPS_LAT, 6);
    Serial.print(F(", "));
    Serial.print(GPS_LON, 6);
  }
  Serial.print(F(" | Température : "));
  if (NA_FLAGS & NA_TEMP) Serial.print(F("NA")); else Serial.print(TEMP_AIR);
  Serial.print(F(" | Pression : "));
  if (NA_FLAGS & NA_PRESSURE) Serial.print(F("NA")); else Serial.print(PRESSURE);
  Serial.print(F(" | Humidité : "));
  if (NA_FLAGS & NA_HYGR) Serial.print(F("NA")); else Serial.print(HYGR);
  Serial.print(F(" | Luminosité : "));
  if (NA_FLAGS & NA_LUMIN) Serial.print(F("NA")); else Serial.print(LUMIN);
  Serial.println();
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
  NA_FLAGS = 0;
  if (TestBME280())
  {
    TEMP_AIR = bme.readTemperature();
    PRESSURE = bme.readPressure() / 100.0f;
    HYGR = bme.readHumidity();
    if (!TemperatureCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_TEMP;
    }
    if (!PressionCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_PRESSURE;
    }
    if (!HumiditeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_HYGR;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_TEMP | NA_PRESSURE | NA_HYGR;
  }
  if (TestLuminosite())
  {
    if (!LuminositeCoherente())
    {
      erreur_courante = &ERR_VALEURS_CAP;
      NA_FLAGS |= NA_LUMIN;
    }
  }
  else
  {
    erreur_courante = &ERR_ACCES_CAP;
    NA_FLAGS |= NA_LUMIN;
  }
  if (BIN%2 == 0)
  {
    if (TestGPS())
    {
      GPS_LAT = gps.location.lat();
      GPS_LON = gps.location.lng();
    }
    else
    {
      erreur_courante = &ERR_GPS;
      NA_FLAGS |= NA_GPS;
    }
  }
  if (TestRTC())
  {
    RTC_TIME = rtc.now();
  }
  else
  {
    erreur_courante = &ERR_RTC;
    NA_FLAGS |= NA_RTC;
  }
  if (TestSDAcces())
  {
    NomFichier();
    dataFile = SD.open(FILE_PATH, FILE_WRITE);
    if (!dataFile)
    {
      erreur_courante = &ERR_WRITE_SD;
      BIN += 1;
      return;
    }
    dataFile.print(F("Date/Heure: "));
    dataFile.print(RTC_TIME.year(), DEC);
    dataFile.print('/');
    dataFile.print(RTC_TIME.month(), DEC);
    dataFile.print('/');
    dataFile.print(RTC_TIME.day(), DEC);
    dataFile.print(F(" - "));
    dataFile.print(RTC_TIME.hour(), DEC);
    dataFile.print(':');
    dataFile.print(RTC_TIME.minute(), DEC);
    dataFile.print(':');
    dataFile.print(RTC_TIME.second(), DEC);
    if (BIN%2 == 0)
    {
    dataFile.print(F(" | GPS lat, long : "));
    if (NA_FLAGS & NA_GPS) dataFile.print(F("NA, NA"));
    else
    {
      dataFile.print(GPS_LAT, 6);
      dataFile.print(F(", "));
      dataFile.print(GPS_LON, 6);
    }
    }
    dataFile.print(F(" | Température : "));
    if (NA_FLAGS & NA_TEMP) dataFile.print(F("NA")); else dataFile.print(TEMP_AIR);
    dataFile.print(F(" | Pression : "));
    if (NA_FLAGS & NA_PRESSURE) dataFile.print(F("NA")); else dataFile.print(PRESSURE);
    dataFile.print(F(" | Humidité : "));
    if (NA_FLAGS & NA_HYGR) dataFile.print(F("NA")); else dataFile.print(HYGR);
    dataFile.print(F(" | Luminosité : "));
    if (NA_FLAGS & NA_LUMIN) dataFile.print(F("NA")); else dataFile.print(LUMIN);
    dataFile.println();
    if (dataFile.size() >= FILE_MAX_SIZE)
    {
      erreur_courante = &ERR_FULL_SD;
    }
    dataFile.close();
  }
  else
  {
    erreur_courante = &ERR_WRITE_SD;
  }
  BIN += 1;
}

void ChoixMode()
{
  ETAT_BOUTON = BOUTON();
  if (ETAT_BOUTON == BTN_RL && (MODE == 'E' || MODE == 'S'))
  {
    MODE_MEMOIRE = MODE;
    MODE = 'M';
  }
  else if (ETAT_BOUTON == BTN_RL && MODE == 'M')
  {
    MODE = MODE_MEMOIRE;
  }
  else if (ETAT_BOUTON == BTN_RC)
  {
    MODE = 'C';
  }
  else if (ETAT_BOUTON == BTN_VL && MODE == 'S')
  {
    MODE = 'E';
  }
  else if (ETAT_BOUTON == BTN_RL && MODE == 'E')
  {
    MODE = 'S';
  }
}

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  Wire.begin();
  rtc.begin();
  bme.begin(ADR_BME280);
  ledRGB.init();
  pinMode(PIN_BTN_ROUGE, INPUT_PULLUP);
  pinMode(PIN_BTN_VERT, INPUT_PULLUP);
  pinMode(PIN_LUMINOSITE, INPUT);
  SD.begin(PIN_SD_CS);
}

void loop()
{
  if (debut || millis() - dernier_releve >= LOG_INTERVAL * 1000UL)
  {
    debut = false;
    dernier_releve = millis();
    if (MODE == 'S' || MODE == 'C')
    {
      ModeStandard();
    }
    else if (MODE == 'E')
    {
      ModeEconomie();
    }
  }
  ChoixMode();
  if (erreur_courante != NULL)
  {
    while (true)
    {
      erreur();
    }
  }
   delay(100);
}
