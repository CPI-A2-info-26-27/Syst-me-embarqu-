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

uint32_t LOG_INTERVAL  = 10;
uint32_t FILE_MAX_SIZE = 2048;
uint32_t TIMEOUT       = 30;
uint8_t LUMIN    = 1;  LUMIN_LOW    = 255;  LUMIN_HIGH   = 768;
uint8_t TEMP_AIR = 1;  MIN_TEMP_AIR = -10;  MAX_TEMP_AIR = 60;
uint8_t HYGR     = 1;  HYGR_MINT    = 0;    HYGR_MAXT    = 50;
uint8_t PRESSURE = 1;  PRESSURE_MIN = 850;  PRESSURE_MAX = 1080;

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

void erreur(Couleur couleur1, Couleur couleur2, unsigned long duree1, unsigned long duree2)
{
  led_color(couleur1.r, couleur1.g, couleur1.b);
  delay(duree1);
  led_color(couleur2.r, couleur2.g, couleur2.b);
  delay(duree2);
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
    if (gps.available() > 0 && gps.read() == '$') return true;
  }
  return false;
}

bool TestSDAcces(int TIMEOUT)
{
  uint32_t debut = millis();
  uint32_t timeoutMs = TIMEOUT * 1000UL;

  while (millis() - debut < timeoutMs)
  {
    SD.end();
    if (SD.begin(PIN_SD_CS))
    {
      SD.remove("TEST.TMP");
      File f = SD.open("TEST.TMP", FILE_WRITE);
      if (f)
      {
        size_t n = f.print("OK");
        f.close();
        SD.remove("TEST.TMP");
        if (n == 2) return true;
      }
    }
  }
  return false;
}

bool TestSDPlace(void)
{
  SD.remove("PLACE.TMP");
  File f = SD.open("PLACE.TMP", FILE_WRITE);
  if (!f) return false;

  uint8_t bloc[64];
  memset(bloc, '0', sizeof(bloc));

  uint32_t ecrit = 0;
  while (ecrit < FILE_MAX_SIZE)
  {
    size_t n = f.write(bloc, sizeof(bloc));
    if (n != sizeof(bloc)) break;
    ecrit += n;
  }

  f.close();
  SD.remove("PLACE.TMP");
  return ecrit >= FILE_MAX_SIZE;
}

bool TemperatureCoherente(float t)
{
  if (isnan(t)) return false;
  return t >= MIN_TEMP_AIR && t <= MAX_TEMP_AIR;
}

bool PressionCoherente(float p)               // hPa
{
  if (isnan(p)) return false;
  return p >= PRESSURE_MIN && p <= PRESSURE_MAX;
}

bool HumiditeCoherente(float t)
{
  if (isnan(t)) return false;
  return t >= HYGR_MINT && t <= HYGR_MAXT;
}

bool LuminositeCoherente(uint16_t v)
{
  return v <= 1023;
}

void ModeStandard(void)
{
  led_color(VERT.r, VERT.g, VERT.b);
  if (TestBME280()) 
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
  if (TestLuminosite()) 
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

  TestGPS();
  TestRTC();
  TestSDAcces();
  TestSDPlace();
}