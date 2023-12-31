// Smart WiFi weather station code
// By Piotersky 2024
// Owner of this code is Gaj Electronics. You can't copy, send, modify or anywhere use this code without permission!
// Właścicielem tego kodu jest Gaj Electronics. Nie możesz kopiować, wysyłać, modyfikować lub gdziekolwiek używać tego kodu bez zezwolenia!

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "weathericons.h"
#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "other.h"
#include <EEPROM.h>
#include <HTTPUpdate.h>
#include "cert.h"

TFT_eSPI tft = TFT_eSPI();
#define TFT_LED 5

#define CALIBRATION_FILE "/calibrationData"

const char *ssid = "";
const char *password = "";

String ssidStr = "";
String passwordStr = "";

const String openWeatherMapApiKey = "576f533d49fa2125b578e893f8d54c74";
String lat = "";
String lon = "";

String currentLink = "";
String forecastLink = "";

const String newestFirmwareVersionLink = "https://raw.githubusercontent.com/Piotersky/Inteligentna_Stacja_Pogodowa/main/latest.txt";
const String newestFirmwareLink = "https://raw.githubusercontent.com/Piotersky/Inteligentna_Stacja_Pogodowa/main/build/";
String apiLink;

unsigned short forecastHour = 0;
unsigned short forecast = 0;

int pressure;
int temp;

String serialNumber = "0";
const String type = "1";
const String firmwareVersion = "2.1";

unsigned short screen = 1;
unsigned short day = 0;
bool changedScreen;

bool clicked = false;
uint16_t x_click, y_click;
unsigned long lastTimeClicked = 0;

bool changing = false;
bool s = false;
bool apiReqeuest = false;

bool main_set = true;
unsigned short value = 9;

TFT_eSPI_Button key[31];
short pressed;

char smallKeyLabel[30][6] = { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "a", "s", "d", "f", "g", "h", "j", "k", "l", "z", "x", "c", "v", "b", "n", "m", "_____", "/\\", "123#", "<" };
char bigKeyLabel[30][6] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Z", "X", "C", "V", "B", "N", "M", "_____", "\\/", "123#", "<" };
char specialKeyLabel[31][4] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "!", "@", "#", "$", "^", "&", "*", "(", ")", "-", ",", "=", "+", ":", ";", "'", "?", "/", ".", "abc", "<" };

bool caps = false;
bool special = false;
String changingValue = "";

unsigned long lastTimeLoop = 60000;
unsigned long timerDelayLoop = 60000;

unsigned long forecastEpochTime;

short timezone = 0;
const String daysOfTheWeek[7] = { "niedziela", "poniedzialek", "wtorek", "sroda", "czwartek", "piatek", "sobota" };
const String daysOfTheWeekConjugationed[11] = { "poniedzialek", "wtorek", "srode", "czwartek", "piatek", "sobote", "niedziele", "poniedzialek", "wtorek", "srode", "czwartek" };

int Status;
const char* Date;
const char* Time;
int Hour;
int WDay;

String jsonBuffer;

String httpGETRequest(const char *serverName) {
  HTTPClient http;
  String payload;

  http.begin(serverName);

  int httpCode = http.GET();
  if (httpCode > 0) {
    payload = http.getString();
  } else {
      Serial.println("HTTP request failed: " + String(httpCode));
  }

  http.end();
  return payload;
}

String getApi() {
  DynamicJsonDocument doc(300);
  DeserializationError error = deserializeJson(doc, httpGETRequest(apiLink.c_str()));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
  }

  if(doc["Status"] == 9) { return ""; } // Error
  if(doc["Status"] == 2) { // Update
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(75, 75, 150, 150, logo);
    tft.drawString("Aktualizo", 30, 250, 4);
    tft.drawString("wanie...", 60, 290, 4);

    updateFirmware(doc["Version"]);
  }

  Date = doc["Date"];
  Time = doc["Time"];
  Hour = doc["Hour"];
  WDay = doc["W_Day"];

  Serial.println(Time);
  Serial.println(Date);
  Serial.println(Hour);
  Serial.println(WDay);
  return String(Time);
}

void updateFirmware(String ver) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  t_httpUpdate_return ret = httpUpdate.update(client, newestFirmwareLink + ver + "/test.ino.bin");
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void drawDate() {
  tft.drawString(String(Date), 40, 45, 4);
  tft.drawString(daysOfTheWeek[WDay], 30, 85, 4);
}

void drawCurrentWeather() {
  DynamicJsonDocument doc(1024);
  String input = httpGETRequest(currentLink.c_str());
  DeserializationError error = deserializeJson(doc, input);
  input = "";

  drawTemp(doc["main"]["temp"], 140, false);
  drawTemp(doc["main"]["feels_like"], 195, true);

  drawWeatherImage(doc["weather"][0]["icon"], 130);

  pressure = doc["main"]["pressure"];
  tft.pushImage(160, 250, 40, 40, barometer);
  tft.drawString(String(pressure), 205, 250, 4);
}

void drawForecast() {
  DynamicJsonDocument doc(24576);
  String input = httpGETRequest(forecastLink.c_str());
  DeserializationError error = deserializeJson(doc, input);
  input = "";

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (day == 0) {
    forecastEpochTime = doc["list"][forecast]["dt"];
    forecastEpochTime += timezone * 3600;

    forecastHour = (forecastEpochTime % 86400) / 3600;

    if (forecastHour - Hour <= 1) {
      forecast++;
      forecastEpochTime = doc["list"][forecast]["dt"];
      forecastEpochTime += timezone * 3600;

      forecastHour = (forecastEpochTime % 86400) / 3600;
    }

    if (forecastHour < 10) {
      tft.drawString("0" + String(forecastHour) + ":00 :", 0, 280, 4);
    } else {
      tft.drawString(String(forecastHour) + ":00 :", 0, 280, 4);
    }

    drawWeatherImage(doc["list"][forecast]["weather"][0]["icon"], 330);
    drawTemp(doc["list"][forecast]["main"]["temp"], 380, false);
    drawTemp(doc["list"][forecast]["main"]["feels_like"], 430, true);

  } else {
    forecast = (day * 8) - 3;

    while (forecastHour < 10 && forecastHour < 18 && forecast < 40) {
      forecastEpochTime = doc["list"][forecast]["dt"];
      forecastEpochTime += timezone * 3600;

      forecastHour = (forecastEpochTime % 86400) / 3600;

      forecast++;
    }

    if (day == 5) tft.drawString(String(forecastHour) + ":00 :", 0, 105, 4);
    else {
      tft.drawString(String(forecastHour) + ":00 i " + String(forecastHour + 6) + ":00 :", 0, 105, 4);
    }

    drawWeatherImage(doc["list"][forecast]["weather"][0]["icon"], 160);
    drawTemp(doc["list"][forecast]["main"]["temp"], 170, false);
    drawTemp(doc["list"][forecast]["main"]["feels_like"], 215, true);

    pressure = doc["list"][forecast]["main"]["pressure"];
    tft.pushImage(160, 265, 40, 40, barometer);
    tft.drawString(String(pressure), 205, 265, 4);

    if (day == 5) {
    } else {
      forecast = forecast + 2;

      drawWeatherImage(doc["list"][forecast]["weather"][0]["icon"], 330);
      drawTemp(doc["list"][forecast]["main"]["temp"], 360, false);
      drawTemp(doc["list"][forecast]["main"]["feels_like"], 415, true);
    }
  }
  forecast = 0;
  forecastHour = 0;
}

void drawWeatherImage(String icon, int y) {
  if (icon == "01d" || icon == "01n") tft.pushImage(20, y, 128, 128, sunny);
  if (icon == "02d" || icon == "03d" || icon == "02n" || icon == "03n") tft.pushImage(20, y, 128, 128, few_clouds);
  if (icon == "04d" || icon == "04n") tft.pushImage(20, y, 128, 128, cloudy);
  if (icon == "09d" || icon == "09n") tft.pushImage(20, y, 128, 128, rain);
  if (icon == "10d" || icon == "10n") tft.pushImage(20, y, 128, 128, rain);
  if (icon == "11d" || icon == "11n") tft.pushImage(20, y, 128, 128, storm);
  if (icon == "13d" || icon == "13n") tft.pushImage(20, y, 128, 128, snow);
}

void drawTemp(double temp_double, int y, bool is_feels) {
  temp_double = round(temp_double);
  temp = int(temp_double);

  if (is_feels) {
    if (temp > 9) {
      tft.drawString(String(temp) + " C", 205, y, 4);
      drawDegree(265, y);
      tft.pushImage(165, y - 5, 16, 46, feels_like_icon);
    }
    if (temp > -1 && temp < 10) {
      tft.drawString(String(temp) + " C", 225, y, 4);
      drawDegree(255, y);
      tft.pushImage(190, y - 5, 16, 46, feels_like_icon);
    }
    if (temp < 0 && temp > -10) {
      tft.drawString(String(temp) + " C", 210, y, 4);
      drawDegree(260, y);
      tft.pushImage(175, y - 5, 16, 46, feels_like_icon);
    }
    if (temp < -9) {
      tft.drawString(String(temp) + " C", 195, y, 4);
      drawDegree(275, y);
      tft.pushImage(160, y - 5, 16, 46, feels_like_icon);
    }
  } else {
    if (temp > 9) {
      tft.drawString(String(temp) + " C", 205, y, 4);
      drawDegree(265, y);
      tft.pushImage(165, y - 5, 18, 44, temperature_icon);
    }
    if (temp > -1 && temp < 10) {
      tft.drawString(String(temp) + " C", 225, y, 4);
      drawDegree(255, y);
      tft.pushImage(190, y - 5, 18, 44, temperature_icon);
    }
    if (temp < 0 && temp > -10) { 
      tft.drawString(String(temp) + " C", 210, y, 4);
      drawDegree(260, y);
      tft.pushImage(175, y - 5, 18, 44, temperature_icon);
    }
    if (temp < -9) {
      tft.drawString(String(temp) + " C", 195, y, 4);
      drawDegree(275, y);
      tft.pushImage(160, y - 5, 18, 44, temperature_icon);
    }
  }
}

void drawDegree(int x, int y) {
    for (int i = 4; i < 7; i++) {
      tft.drawCircle(x, y, i, TFT_WHITE);
    }
  }

void changeSettings() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (!s && y_click > 109 || y_click < 320 && !s) {
    if (y_click >= 110 && y_click < 140) {
      value = 0;
      changingValue = ssidStr;
    }
    if (y_click >= 140 && y_click < 180) {
      value = 1;
      changingValue = passwordStr;
    }
    if (y_click >= 240 && y_click < 280) {
      value = 2;
      changingValue = lat;
    }
    if (y_click >= 280 && y_click < 320) {
      value = 3;
      changingValue = lon;
    }
    if (value == 9) { return; }
    if(value != 9) {
      main_set = false;
    }

    s = true;
  }
  changing = true;

  if (x_click < 50 && y_click > 250 && s) {
    if (value == 0) {
      ssidStr = changingValue;
      EEPROM.writeString(0, ssidStr);
    }
    if (value == 1) {
      passwordStr = changingValue;
      EEPROM.writeString(ssidStr.length() + 1, passwordStr);
    }
    if (value == 2) {
      lat = changingValue;
      EEPROM.writeString(ssidStr.length() + passwordStr.length() + 2, lat);
    }
    if (value == 3) {
      lon = changingValue;
      EEPROM.writeString(ssidStr.length() + passwordStr.length() + lat.length() + 3, lon);
    }

    changing = false;
    s = false;
    main_set = true;
    EEPROM.commit();
    ESP.restart();
  }

  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  tft.setTextSize(3);
  tft.drawString("zapisz", 0, 0, 2);
  tft.setTextSize(2);

  pressed = -1;

  if (x_click > 140 && x_click < 410) {
    for (uint8_t col = 0; col < 8; col++) {
      if (col * 40 < y_click && (col + 1) * 40 > y_click) {
        for (uint8_t row = 0; row < 3; row++) {
          if (row * 85 + 140 <= x_click && x_click < (row + 1) * 85 + 140) {
            pressed = (8 * (row + 1)) - col - 1;
          }
        }
      }
    }
  }
  if (x_click >= 410) {
    if (!special) {
      if (y_click > 290) { pressed = 24; }
      if (y_click <= 290 && y_click > 240) { pressed = 25; }
      if (y_click <= 240 && y_click > 160) { pressed = 26; }
      if (y_click <= 160 && y_click > 120) { pressed = 27; }
      if (y_click <= 120 && y_click > 50) { pressed = 28; }
      if (y_click <= 50) { pressed = 29; }
    }
    if (special) {
      if (y_click > 290) { pressed = 24; }
      if (y_click <= 290 && y_click > 250) { pressed = 25; }
      if (y_click <= 250 && y_click > 210) { pressed = 26; }
      if (y_click <= 210 && y_click > 170) { pressed = 27; }
      if (y_click <= 170 && y_click > 130) { pressed = 28; }
      if (y_click <= 130 && y_click > 70) { pressed = 29; }
      if (y_click <= 70) { pressed = 30; }
    }
  }

  for (uint8_t b = 0; b < 31; b++) {
    key[b].press(false);
  }

  if (pressed != -1) {
    key[pressed].press(true);
    for (uint8_t b = 0; b < 31; b++) {
      if (key[b].justPressed()) {
        if (!special) {
          if (b < 26) {
            if (caps) {
              changingValue = changingValue + bigKeyLabel[b];
            }
            if (!caps) {
              changingValue = changingValue + smallKeyLabel[b];
            }
          }
          if (b == 26) {
            changingValue = changingValue + " ";
          }
          if (b == 27) caps = !caps;
          if (b == 28) {
            special = true;
            b = -2;
          }
          if (b == 29) {
            changingValue.remove(changingValue.length() - 1, 1);
          }
        }
        if (special) {
          if (b < 29 && b > -1) {
            changingValue = changingValue + specialKeyLabel[b];
          }
          if (b == 30) {
            changingValue.remove(changingValue.length() - 1, 1);
          }
          if (b == 29) {
            special = false;
          }
        }
      }
    }
  }

  if (value == 0) {
    tft.drawString("nazwa sieci :", 120, 0, 4);
    tft.drawString(changingValue, 0, 50, 4);
  }
  if (value == 1) {
    tft.drawString("haslo sieci :", 120, 0, 4);
    tft.drawString(changingValue, 0, 50, 4);
  }
  if (value == 2) {
    tft.drawString("szerokosc geo. :", 120, 0, 4);
    tft.drawString(changingValue, 0, 50, 4);
  }
  if (value == 3) {
    tft.drawString("dlugosc geo. :", 120, 0, 4);
    tft.drawString(changingValue, 0, 50, 4);
  }

  if (value == 2 || value == 3) {
    special = true;
  }

  for (uint8_t row = 0; row < 4; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint8_t b = col + row * 8;

      if (!special) {
        if (b == 30) { return; }
        if (b == 26) { key[b].initButton(&tft, 
                                        30 + 22 + col * 62,
                                        125 + row * (30 + 25),
                                        110, 45,
                                        TFT_WHITE, TFT_BLUE, TFT_WHITE,
                                        smallKeyLabel[b], 3); }
        if (b == 28) { key[b].initButton(&tft, 85 + 22 + col * 62, 125 + row * (30 + 25), 100, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, smallKeyLabel[b], 3); }
        if (b == 29) { key[b].initButton(&tft, 115 + 22 + col * 62, 125 + row * (30 + 25), 60, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, smallKeyLabel[b], 3); }

        if (!caps) {
          if (b == 27) { key[b].initButton(&tft, 60 + 22 + col * 62, 125 + row * (30 + 25), 50, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, smallKeyLabel[b], 3); }
          if (b != 26 && b != 27 && b != 28 && b != 29) {
            key[b].initButton(&tft, 22 + col * 62, 125 + row * (30 + 25), 45, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, smallKeyLabel[b], 3); 
          }
        }
        if (caps) {
          if (b == 27) { key[b].initButton(&tft, 60 + 22 + col * 62, 125 + row * (30 + 25), 50, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, bigKeyLabel[b], 3); }
          if (b != 26 && b != 27 && b != 28 && b != 29) {
            key[b].initButton(&tft, 22 + col * 62, 125 + row * (30 + 25), 45, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, bigKeyLabel[b], 3);
          }
        }
      }
      if (special) {
        if (b == 31) { return; }
        if (b == 29) { key[b].initButton(&tft, 15 + 22 + col * 62, 125 + row * (30 + 25), 80, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, specialKeyLabel[b], 3); }
        if (b == 30) { key[b].initButton(&tft, 40 + 22 + col * 62, 125 + row * (30 + 25), 80, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, specialKeyLabel[b], 3); }
        if (b < 29) { key[b].initButton(&tft, 22 + col * 62, 125 + row * (30 + 25), 45, 45, TFT_WHITE, TFT_BLUE, TFT_WHITE, specialKeyLabel[b], 3); }
      }
      key[b].drawButton();
    }
  }

  Serial.println(changing);
  Serial.println(s);
  if(!changing && !s) {
    Serial.println("s");
  }
}

void setup(void) {
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(2);
  tft.setSwapBytes(true);

  Serial.begin(115200);
  Serial.println("By Piotersky - Gaj Electronics");

  tft.pushImage(1, 1, 48, 48, settings_icon);
  tft.pushImage(75, 75, 150, 150, logo);
  tft.setTextSize(2);
  tft.drawString("Ladowanie...", 10, 250, 4);

  tft.setTextPadding(0);

  uint16_t calibrationData[5];
  uint8_t calDataOK = 0;

  if (!EEPROM.begin(1000)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  ssidStr = EEPROM.readString(0);
  passwordStr = EEPROM.readString(ssidStr.length() + 1);
  lat = EEPROM.readString(ssidStr.length() + passwordStr.length() + 2);
  lon = EEPROM.readString(ssidStr.length() + passwordStr.length() + lat.length() + 3);
  timezone = EEPROM.readShort(ssidStr.length() + passwordStr.length() + lat.length() + lon.length() + 4);
  // EEPROM.writeString(ssidStr.length() + passwordStr.length() + lat.length() + lon.length() + 2 + 5, serialNumber);
  serialNumber = EEPROM.readString(ssidStr.length() + passwordStr.length() + lat.length() + lon.length() + 2 + 5);
  
  apiLink = "https://gaj-electronics-api.onrender.com/" + serialNumber + "/" + type + "/" + firmwareVersion + "/" + lat + "/" + lon + "/" + timezone;

  currentLink = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&" + "lon=" + lon + "&APPID=" + openWeatherMapApiKey + "&units=metric";
  forecastLink = "http://api.openweathermap.org/data/2.5/forecast?lat=" + lat + "&" + "lon=" + lon + "&APPID=" + openWeatherMapApiKey + "&units=metric";

  ssid = ssidStr.c_str();
  password = passwordStr.c_str();

  if (!SPIFFS.begin()) {
    Serial.println("formating file system");

    SPIFFS.format();
    SPIFFS.begin();
  }

  if (SPIFFS.exists(CALIBRATION_FILE)) {
    File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f) {
      if (f.readBytes((char *)calibrationData, 14) == 14)
        calDataOK = 1;
      f.close();
    }
  }
  if (calDataOK) {
    tft.setTouch(calibrationData);
  }
  if (!calDataOK) { 
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Skalibruj...", 50, 210, 4);
    tft.calibrateTouch(calibrationData, TFT_WHITE, TFT_RED, 15);

    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calibrationData, 14);
      f.close();
    }
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Ladowanie...", 10, 210, 4);
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED && screen != 2) {
    delay(200);
    Serial.print(".");
    if (tft.getTouch(&x_click, &y_click, 50) && x_click <= 50 && y_click <= 50) {
      screen = 2;
      changedScreen = true;
      delay(200);
    }
  }
  if (screen != 2) {
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  if (tft.getTouch(&x_click, &y_click, 50)) {
    clicked = true;
    lastTimeClicked = millis();
  } else {
    clicked = false;
  }

  if (screen == 0 && clicked) {
    screen = 1;
    digitalWrite(TFT_LED, HIGH);
    x_click = 1;
    y_click = 482;
  }

  if((millis() - lastTimeLoop) > timerDelayLoop) {
    if(screen != 1) { lastTimeLoop = millis(); };
    getApi();
    apiReqeuest = true;
  }

  if (screen == 1 && !changing) {
    if (clicked && x_click <= 50 && y_click <= 50 && day == 0) {
      screen = 2;
      changedScreen = true;
    }
    if (clicked && x_click > 160 && day < 4 && screen == 1 && !changedScreen) day++;
    if (clicked && x_click <= 160 && day != 0 && screen == 1) day--;
    if ((millis() - lastTimeLoop) > timerDelayLoop || apiReqeuest || clicked == true && screen == 1) {
      changedScreen = false;
      if (millis() > 86400000) ESP.restart();
      tft.fillScreen(TFT_BLACK);
      if (WiFi.status() == WL_CONNECTED) {
        tft.setTextSize(2);
        if (day == 0) {
          tft.pushImage(1, 1, 48, 48, settings_icon);
          tft.drawString(String(Time), 95, 0, 4);
          drawDate();
          drawCurrentWeather();
        } else {
          tft.drawString("Pogoda na", 40, 0, 4);
          tft.drawString(daysOfTheWeekConjugationed[WDay + day - 1], 20, 50, 4);
        }
        drawForecast();
      } else {
        tft.pushImage(1, 1, 48, 48, settings_icon);
        tft.setTextSize(2);
        tft.drawString("Nie mozna", 0, 150, 4);
        tft.drawString("polaczyc sie", 0, 190, 4);
        tft.drawString("z siecia WiFi...", 0, 240, 4);
        Serial.println("WiFi Disconnected");
      }
      lastTimeLoop = millis();
      apiReqeuest = false;
    }
    if ((millis() - lastTimeClicked) > 90000) {
      screen = 0;
      day = 0;
      tft.fillScreen(TFT_BLACK);
      digitalWrite(TFT_LED, LOW);
    }
  }
  if (screen == 2) {
    if (clicked && x_click > 300 && y_click < 30 && !changing) {
      EEPROM.writeShort(ssidStr.length() + passwordStr.length() + lat.length() + lon.length() + 4, timezone);
      EEPROM.writeString(ssidStr.length() + passwordStr.length() + lat.length() + lon.length() + 2 + 5, serialNumber);
      EEPROM.commit();
      apiLink = "https://gaj-electronics-api.onrender.com/" + serialNumber + "/" + type + "/" + firmwareVersion + "/" + lat + "/" + lon + "/" + timezone;

      screen = 1;
      changedScreen = true;
    }
    if (clicked && !changedScreen && screen == 2) {
      changeSettings();
      
      if (y_click > 390 && y_click < 420) {
        if (timezone < 12) {
          timezone += 1;
        } else {
          timezone = -12;
        }
      }
    }
    if ((millis() - lastTimeLoop) > timerDelayLoop && !changing || clicked && screen == 2 && !changing) {
      changedScreen = false;

      tft.fillScreen(TFT_BLACK);
      tft.setTextSize(2);
      tft.drawString("Ustawienia", 20, 0, 4);
      tft.drawString("X", 290, 0, 4);

      tft.drawString("siec", 110, 60, 4);
      tft.drawString("nazwa:", 0, 100, 4);
      tft.drawString("haslo:", 0, 140, 4);
      tft.drawString("lokalizacja", 50, 200, 4);
      tft.drawString("szerokosc:", 0, 240, 4);
      tft.drawString("dlugosc:", 0, 280, 4);
      tft.drawString("czas", 100, 330, 4);
      tft.drawString("strefa:", 0, 380, 4);

      if(timezone >= 0) {
        tft.drawString("GMT+" + String(timezone), 150, 380, 4); 
      }
      else {
        tft.drawString("GMT" + String(timezone), 150, 380, 4);
      }
        
      tft.setTextSize(1);
      tft.drawString(ssidStr, 180, 115, 4);
      tft.drawString(passwordStr, 170, 155, 4);
      tft.drawString(lat, 245, 255, 4);
      tft.drawString(lon, 200, 295, 4);
      tft.drawString("By Piotersky 2024 " + serialNumber + "v" + firmwareVersion, 0, 455, 4);

      lastTimeLoop = millis();
    }
  }

  delay(100);
}