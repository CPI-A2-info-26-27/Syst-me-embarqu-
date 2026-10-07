#include <Adafruit_BME280.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>

#define PIN_BTN_ROUGE     D5
#define PIN_BTN_VERT      D6
#define PIN_LED_CLK       D7
#define PIN_LED_DATA      D9
#define PIN_GPS_RX        PA10
#define PIN_GPS_TX        PA9 
#define PIN_SD_CS         D4    
#define ADR_RTC           0x68
#define ADR_BME280        0x76
#define PIN_LUMINOSITE    A0

int LOG_INTERVAL  = 600;
int FILE_MAX_SIZE = 2048;
int TIMEOUT       = 30;
uint8_t LUMIN    = 1;  LUMIN_LOW    = 255;  LUMIN_HIGH   = 768;
uint8_t TEMP_AIR = 1;  MIN_TEMP_AIR = -10;  MAX_TEMP_AIR = 60;
uint8_t HYGR     = 1;  HYGR_MINT    = 0;    HYGR_MAXT    = 50;
uint8_t PRESSURE = 1;  PRESSURE_MIN = 850;  PRESSURE_MAX = 1080;
float GPS_LAT = 0.0f;
float GPS_LON = 0.0f;
char FILE_PATH[12] = "";
int BIN = 0;
char MODE = 'S';
char MODE_MEMOIRE = '';
bool debut = true;
uint32_t dernier_releve = 0;
char retour_erreur = "";

Adafruit_BME280 bme;
RTC_DS1307 rtc;

ChainableLED ledRGB(8,9,1);

typedef struct { uint8_t r, g, b; } Couleur;

const Couleur VERT   = { 0,   255, 0   };
const Couleur JAUNE  = { 255, 255, 0   };
const Couleur BLEU   = { 0,   0,   255 };
const Couleur ORANGE = { 255, 100, 0   };
const Couleur ROUGE  = { 255, 0,   0   };
const Couleur BLANC  = { 255, 255, 255 };
const Couleur ETEINT = { 0,   0,   0   };

void led_color(uint8_t r, uint8_t g, uint8_t b)
{
  ledRGB.setColorRGB(0, r, g, b);
}

typedef struct { char c1, char c2, int d1, int d2; } Erreur;

const Erreur ERR_RTC   = { ROUGE, BLEU, 1000, 1000 };
const Erreur ERR_GPS   = { ROUGE, JAUNE, 1000, 1000 };
const Erreur ERR_ACCES_CAP   = { ROUGE, VERT, 1000, 1000 };
const Erreur ERR_VALEURS_CAP   = { ROUGE, VERT, 1000, 2000 };
const Erreur ERR_FULL_SD    = { ROUGE, BLANC, 1000, 1000 };
const Erreur ERR_WRITE_SD    = { ROUGE, BLANC, 1000, 2000 };

char erreur = "";

typedef struct { char Mode;} Modes;

const Modes S = { ModeStandard() };
const Modes M = { ModeMaintenance() };
const Modes C = { ModeConfiguration() };
const Modes E = { ModeEconomie() };

void erreur(Couleur couleur1, Couleur couleur2, unsigned long duree1, unsigned long duree2)
{
  led_color(couleur1.r, couleur1.g, couleur1.b);
  delay(duree1);
  led_color(couleur2.r, couleur2.g, couleur2.b);
  delay(duree2);
}

char BOUTON()
{
  uint32_t debut = millis();
  uint32_t timeoutMs = 5 * 1000UL;
  if (digitalRead(PIN_BTN_ROUGE) == LOW) 
  {
    while (millis() - debut < timeoutMs) 
    {
      if (digitalRead(PIN_BTN_ROUGE) == HIGH) 
      {
        return 'RC';
      }
    }
    return 'RL';
  }
  if (digitalRead(PIN_BTN_VERT) == LOW) 
  {
    while (millis() - debut < timeoutMs) 
    {
      if (digitalRead(PIN_BTN_VERT) == HIGH) 
      {
        return 'VC';
      }
    }
    return 'VL';
  }
  return 'N';
}

bool TestRTC(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    if (rtc.begin()) 
    {
      return true;
    }
  }
  return false;
}

bool TestBME280(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    if (bme.begin(ADR_BME280)) 
    {
      return true;
    }
  }
  return false;
}

bool TestLuminosite(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    if (digiRead(PIN_LUMINOSITE) >= 0)
    {
      return true;
    }
  }
  return false;
}

bool TestGPS(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    if (gps.location.isValid() > 0 && gps.read() == '$') return true;
  }
  return false;
}

bool TestSDAcces(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    if (SD.begin()) return true;
  }
  return false;
}

void ModeStandard(int TIMEOUT = 30, int FILE_MAX_SIZE = 2048)
{
  led_color(VERT.r, VERT.g, VERT.b);
  LOG_INTERVAL = 600;
  if (TestBME280(TIMEOUT)) 
  {
    if (!TemperatureCoherente(bme.readTemperature()))
    {
      erreur = "ERR_VALEURS_CAP";
      TEMP_AIR = "NA";
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
    if (!PressionCoherente(bme.readPressure() / 100.0f))
    {
      erreur = "ERR_VALEURS_CAP";
      PRESSURE = "NA";
      TEMP_AIR = bme.readTemperature();
      HYGR = bme.readHumidity();
    }
    if (!HumiditeCoherente(bme.readHumidity()))
    {
      erreur = "ERR_VALEURS_CAP";
      HYGR = "NA";
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
    }
    else
    {
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    TEMP_AIR = "NA";
    PRESSURE = "NA";
    HYGR = "NA";
  }
  if (TestLuminosite(TIMEOUT)) 
  {
    if (!LuminositeCoherente(analogRead(PIN_LUMINOSITE())))
    {
      erreur = "ERR_VALEURS_CAP";
      LUMIN = "NA";
    }
    else
    {
      LUMIN = analogRead(PIN_LUMINOSITE());
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    LUMIN = "NA";
  }
  if (TestGPS(TIMEOUT)) 
  {
    GPS_LAT = gps.location.lat();
    GPS_LON = gps.location.lng();
  }
  else
  {
    erreur = "ERR_GPS";
    GPS_LAT = "NA";
    GPS_LON = "NA";
  }
  if (TestRTC(TIMEOUT)) 
  {
    RTC_TIME = rtc.now();
    Year = RTC_TIME.year();
    Month = RTC_TIME.month();
    Day = RTC_TIME.day();
    Hour = RTC_TIME.hour();
    Minute = RTC_TIME.minute();
    Second = RTC_TIME.second();
  }
  else
  {
    erreur = "ERR_RTC";
    RTC_TIME = "NA";
  }
  if (TestSDAcces(TIMEOUT)) 
  {
    FILE_PATH = String(Year, DEC) + String(Month, DEC) + String(Day, DEC) + "_" + "0" + ".LOG";
    File dataFile = SD.open(FILE_PATH, FILE_WRITE);
    dataFile.print("Date/Heure: ");
    dataFile.print(Year, DEC);
    dataFile.print('/');
    dataFile.print(Month, DEC);
    dataFile.print('/');
    dataFile.print(Day, DEC);
    dataFile.print(" - ");
    dataFile.print(Hour, DEC);
    dataFile.print(':');
    dataFile.print(Minute, DEC);
    dataFile.print(':');
    dataFile.print(Second, DEC);
    dataFile.print(" | GPS lat, long : ");
    dataFile.print(GPS_LAT, 6);
    dataFile.print(", ");
    dataFile.print(GPS_LON, 6);
    dataFile.print(" | Température : ");
    dataFile.print(TEMP_AIR);
    dataFile.print(" | Pression : ");
    dataFile.print(PRESSURE);
    dataFile.print(" | Humidité : ");
    dataFile.print(HYGR);
    dataFile.print(" | Luminosité : ");
    dataFile.print(LUMIN);
    dataFile.println();
    dataFile.close();
    if (dataFile.size() >= FILE_MAX_SIZE)
    {
      erreur = "ERR_FULL_SD";
    }
  }
  else
  {
    erreur = "ERR_SD";
  }
  return erreur;
}

void ModeMaintenance(int TIMEOUT = 30)
{
  led_color(ORANGE.r, ORANGE.g, ORANGE.b);
  if (TestBME280(TIMEOUT)) 
  {
    if (!TemperatureCoherente(bme.readTemperature()))
    {
      erreur = "ERR_VALEURS_CAP";
      TEMP_AIR = "NA";
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
    if (!PressionCoherente(bme.readPressure() / 100.0f))
    {
      erreur = "ERR_VALEURS_CAP";
      PRESSURE = "NA";
      TEMP_AIR = bme.readTemperature();
      HYGR = bme.readHumidity();
    }
    if (!HumiditeCoherente(bme.readHumidity()))
    {
      erreur = "ERR_VALEURS_CAP";
      HYGR = "NA";
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
    }
    else
    {
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    TEMP_AIR = "NA";
    PRESSURE = "NA";
    HYGR = "NA";
  }
  if (TestLuminosite(TIMEOUT)) 
  {
    if (!LuminositeCoherente(analogRead(PIN_LUMINOSITE())))
    {
      erreur = "ERR_VALEURS_CAP";
      LUMIN = "NA";
    }
    else
    {
      LUMIN = analogRead(PIN_LUMINOSITE());
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    LUMIN = "NA";
  }
  if (TestGPS(TIMEOUT)) 
  {
    GPS_LAT = gps.location.lat();
    GPS_LON = gps.location.lng();
  }
  else
  {
    erreur = "ERR_GPS";
    GPS_LAT = "NA";
    GPS_LON = "NA";
  }
  if (TestRTC(TIMEOUT)) 
  {
    RTC_TIME = rtc.now();
    Year = RTC_TIME.year();
    Month = RTC_TIME.month();
    Day = RTC_TIME.day();
    Hour = RTC_TIME.hour();
    Minute = RTC_TIME.minute();
    Second = RTC_TIME.second();
  }
  else
  {
    erreur = "ERR_RTC";
    RTC_TIME = "NA";
  }
  if 
  Serial.print("Date/Heure: ");
  Serial.print(Year, DEC);
  Serial.print('/');
  Serial.print(Month, DEC);
  Serial.print('/');
  Serial.print(Day, DEC);
  Serial.print(" - ");
  Serial.print(Hour, DEC);
  Serial.print(':');
  Serial.print(Minute, DEC);
  Serial.print(':');
  Serial.print(Second, DEC);
  Serial.println();
  Serial.print(" | GPS lat, long : ");
  Serial.print(GPS_LAT, 6);
  Serial.print(", ");
  Serial.print(GPS_LON, 6);
  Serial.print(" | Température : ");
  Serial.print(TEMP_AIR);
  Serial.print(" | Pression : ");
  Serial.print(PRESSURE);
  Serial.print(" | Humidité : ");
  Serial.print(HYGR);
  Serial.print(" | Luminosité : ");
  Serial.print(LUMIN);
  Serial.println();
  Serial.close();
  return erreur;
}

void ModeConfiguration(int TIMEOUT = 30)
{
  led_color(JAUNE.r, JAUNE.g, JAUNE.b);
  LOG_INTERVAL = 1800;
  dernier_releve = millis();
  return erreur;
}

void ModeEconomie(int TIMEOUT = 30, int FILE_MAX_SIZE = 2048, int BIN)
{ 
  led_color(BLEU.r, BLEU.g, BLEU.b);
  LOG_INTERVAL = 1200;
  if (TestBME280(TIMEOUT)) 
  {
    if (!TemperatureCoherente(bme.readTemperature()))
    {
      erreur = "ERR_VALEURS_CAP";
      TEMP_AIR = "NA";
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
    if (!PressionCoherente(bme.readPressure() / 100.0f))
    {
      erreur = "ERR_VALEURS_CAP";
      PRESSURE = "NA";
      TEMP_AIR = bme.readTemperature();
      HYGR = bme.readHumidity();
    }
    if (!HumiditeCoherente(bme.readHumidity()))
    {
      erreur = "ERR_VALEURS_CAP";
      HYGR = "NA";
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
    }
    else
    {
      TEMP_AIR = bme.readTemperature();
      PRESSURE = bme.readPressure() / 100.0f;
      HYGR = bme.readHumidity();
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    TEMP_AIR = "NA";
    PRESSURE = "NA";
    HYGR = "NA";
  }
  if (TestLuminosite(TIMEOUT)) 
  {
    if (!LuminositeCoherente(analogRead(PIN_LUMINOSITE())))
    {
      erreur = "ERR_VALEURS_CAP";
      LUMIN = "NA";
    }
    else
    {
      LUMIN = analogRead(PIN_LUMINOSITE());
    }
  }
  else
  {
    erreur = "ERR_ACCES_CAP";
    LUMIN = "NA";
  }
  if (BIN%2 == 0) 
  {
    if (TestGPS(TIMEOUT)) 
    {
      GPS_LAT = gps.location.lat();
      GPS_LON = gps.location.lng();
    }
    else
    {
      erreur = "ERR_GPS";
      GPS_LAT = "NA";
      GPS_LON = "NA";
    }
  }
  if (TestRTC(TIMEOUT)) 
  {
    RTC_TIME = rtc.now();
    Year = RTC_TIME.year();
    Month = RTC_TIME.month();
    Day = RTC_TIME.day();
    Hour = RTC_TIME.hour();
    Minute = RTC_TIME.minute();
    Second = RTC_TIME.second();
  }
  else
  {
    erreur = "ERR_RTC";
    RTC_TIME = "NA";
  }
  if (TestSDAcces(TIMEOUT)) 
  {
    FILE_PATH = String(Year, DEC) + String(Month, DEC) + String(Day, DEC) + "_" + "0" + ".LOG";
    File dataFile = SD.open(FILE_PATH, FILE_WRITE);
    dataFile.print("Date/Heure: ");
    dataFile.print(Year, DEC);
    dataFile.print('/');
    dataFile.print(Month, DEC);
    dataFile.print('/');
    dataFile.print(Day, DEC);
    dataFile.print(" - ");
    dataFile.print(Hour, DEC);
    dataFile.print(':');
    dataFile.print(Minute, DEC);
    dataFile.print(':');
    dataFile.print(Second, DEC);
    if (BIN%2 == 0) 
    {
    dataFile.print(" | GPS lat, long : ");
    dataFile.print(GPS_LAT, 6);
    dataFile.print(", ");
    dataFile.print(GPS_LON, 6);
    }
    dataFile.print(" | Température : ");
    dataFile.print(TEMP_AIR);
    dataFile.print(" | Pression : ");
    dataFile.print(PRESSURE);
    dataFile.print(" | Humidité : ");
    dataFile.print(HYGR);
    dataFile.print(" | Luminosité : ");
    dataFile.print(LUMIN);
    dataFile.println();
    dataFile.close();
    if (dataFile.size() >= FILE_MAX_SIZE)
    {
      erreur = "ERR_FULL_SD";
    }
  }
  else
  {
    erreur = "ERR_SD";
  }
  BIN += 1;
  return erreur;
}

void ChoixMode(char MODE)
{
  if (BOUTON() == 'RL' && (MODE == 'E' || MODE == 'S')) 
  {
    MODE_MEMOIRE = MODE;
    MODE = 'M';
  }
  else if (BOUTON() == 'RL' && MODE == 'M') 
  {
    MODE = MODE_MEMOIRE;
  }
  else if (BOUTON() == 'RC') 
  {
    MODE = 'C';
  }
  else if (BOUTON() == 'VL' && MODE == 'S') 
  {
    MODE = 'E';
  }
  else if (BOUTON() == 'RL' && MODE == 'E') 
  {
    MODE = 'S';
  }
  return MODE.Mode;
}

void setup() {
  Serial.begin(9600);
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
      retour_erreur = ModeStandard(TIMEOUT, FILE_MAX_SIZE);
    }
    else if (MODE == 'E') 
    {
      retour_erreur = ModeEconomie(TIMEOUT, FILE_MAX_SIZE, BIN);
    }
  }
  if 
  retour_erreur = ChoixMode(MODE);
  if (retour_erreur != "") 
  {
    while (true) 
    {
      erreur(retour_erreur.c1, retour_erreur.c2, retour_erreur.d1, retour_erreur.d2);
    }
  }
   delay(100);
}