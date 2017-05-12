/**The MIT License (MIT)
Copyright (c) 2015 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at http://blog.squix.ch
*/


// Wunderground client consumes too much memory by default


#include <Arduino.h>
#include "settings.h"

#include <MiniGrafx.h>
#include <ILI9341_SPI.h>
#include <SPI.h>
#include <Carousel.h>

// Fonts created by http://oleddisplay.squix.ch/
#include "VarelaRoundPlain14.h"
#include "VarelaRoundPlain36.h"
#include "OpenSansPlain14.h"
#include "OpenSansPlain36.h"


#include <ESP8266WiFi.h>
//#include <ArduinoOTA.h>
//#include <ESP8266mDNS.h>
//#include <DNSServer.h>
//#include <ESP8266WebServer.h>

// Helps with connecting to internet
//#include <WiFiManager.h>

// check settings.h for adapting to your needs

#include <JsonListener.h>
// Using local copy due to limited memory. 
// TODO: make memory consumption of Wunderground configurable
#include "WundergroundClient.h"
#include <time.h>
#include <simpleDSTadjust.h>
#include "WeatherStationFonts.h"

// HOSTNAME for OTA update
#define HOSTNAME "ESP8266-OTA-"

/*****************************
 * Important: see settings.h to configure your settings!!!
 * ***************************/

#define TFT_DC D2
#define TFT_CS D1
#define TFT_LED D8

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
int BITS_PER_PIXEL = 4; // 2^4 = 16 colors

#define MINI_BLACK     0
#define MINI_WHITE     1
#define MINI_NAVY      2
#define MINI_DARKCYAN  3
#define MINI_DARKGREEN 4
#define MINI_MAROON    5
#define MINI_PURPLE    6
#define MINI_OLIVE     7
#define MINI_LIGHTGREY 8
#define MINI_DARKGREY  9
#define MINI_BLUE     10
#define MINI_GREEN    11
#define MINI_CYAN     12
#define MINI_RED      13
#define MINI_MAGENTA  14
#define MINI_ORANGE  15
// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                      ILI9341_NAVY, // 2
                      ILI9341_DARKCYAN, // 3
                      ILI9341_DARKGREEN, // 4
                      ILI9341_MAROON, // 5
                      ILI9341_PURPLE, // 6
                      ILI9341_OLIVE, // 7
                      ILI9341_LIGHTGREY, // 8
                      0x39E7, //ILI9341_DARKGREY, // 9
                      ILI9341_BLUE, // 10
                      ILI9341_GREEN, // 11
                      ILI9341_CYAN, // 12
                      ILI9341_RED, // 13
                      ILI9341_MAGENTA, // 14
                      0xFD80}; // 15

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient* wunderground;

// Initialize the driver
ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, SCREEN_WIDTH, SCREEN_HEIGHT, BITS_PER_PIXEL, palette);
Carousel carousel(&gfx, 90, 0, 240, 80);//(5 + x, 90 + y, 240-10, 80);

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

//declaring prototypes
//void configModeCallback (WiFiManager *myWiFiManager);
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal);
//ProgressCallback _downloadCallback = downloadCallback;
void downloadResources();
void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawCurrentWeather(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
String getMeteoconIcon(String iconText);
void drawAstronomy();
void drawSeparator(uint16_t y);

FrameCallback frames[] = { drawCurrentWeather, drawForecast };
const int frameCount = 2;

long lastDownloadUpdate = millis();

CurrentCondition currentCondition;
Forecast forecast[3];

void setupWiFi() {
  //WiFiManager *wifiManager = new WiFiManager();
  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  //wifiManager->setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect("AutoConnectAP");


  //Manual Wifi
  //WiFi.begin("mobiletest", "Zaimi%n4");
  WiFi.begin("squixp", "ale130804");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting...");

    // Turn on the background LED
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  gfx.init();
  gfx.fillBuffer(MINI_BLACK);

  gfx.setColor(MINI_ORANGE);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(120, 160, "Connecting to WiFi");
  gfx.commit();
  //SPIFFS.format();

  //Local intialization. Once its business is done, there is no need to keep it around

  Serial.println(ESP.getFreeHeap());
  setupWiFi();
  Serial.println(ESP.getFreeHeap());
  // OTA Setup
  //String hostname(HOSTNAME);
  //hostname += String(ESP.getChipId(), HEX);
  //WiFi.hostname(hostname);
  gfx.fillBuffer(MINI_BLACK);
  gfx.setColor(MINI_ORANGE);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(120, 160, "Connected!");
  gfx.commit();
  carousel.setFrames(frames, frameCount);
  /*ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
  SPIFFS.begin();*/
  
  //Uncomment if you want to update all internet resources
  //SPIFFS.format();

  // download images from the net. If images already exist don't download
  //downloadResources();
  delay(2000);
  // load the weather information
  Serial.println(ESP.getFreeHeap());
  updateData();
  Serial.println(ESP.getFreeHeap());
}

long lastDrew = 0;
void loop() {
  gfx.fillBuffer(0);
  carousel.update();
  // Handle OTA update requests
  //ArduinoOTA.handle();
 

  drawTime();



  // Check if we should update weather information
  if (millis() - lastDownloadUpdate > 1000 * UPDATE_INTERVAL_SECS) {
      updateData();
      lastDownloadUpdate = millis();
  }
  gfx.commit();

}

// Called if WiFi has not been configured yet
/*oid configModeCallback (WiFiManager *myWiFiManager) {
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  //tft.setFont(&ArialRoundedMTBold_14);
  gfx.setColor(MINI_ORANGE);
  gfx.drawString(120, 28, "Wifi Manager");
  gfx.drawString(120, 42, "Please connect to AP");
  gfx.setColor(MINI_WHITE);
  gfx.drawString(120, 56, myWiFiManager->getConfigPortalSSID());
  gfx.setColor(MINI_ORANGE);
  gfx.drawString(120, 70, "To setup Wifi Configuration");
}*/

// callback called during download of files. Updates progress bar
void downloadCallback(String filename, int16_t bytesDownloaded, int16_t bytesTotal) {
  /*Serial.println(String(bytesDownloaded) + " / " + String(bytesTotal));

  int percentage = 100 * bytesDownloaded / bytesTotal;
  if (percentage == 0) {
    ui.drawString(120, 160, filename);
  }
  if (percentage % 5 == 0) {
    ui.setTextAlignment(CENTER);
    ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
    //ui.drawString(120, 160, String(percentage) + "%");
    ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
  }*/

}

// Download the bitmaps
void downloadResources() {
  /*tft.fillScreen(ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);
  char id[5];
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/" + wundergroundIcons[i] + ".bmp", wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 21; i++) {
    sprintf(id, "%02d", i);
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/wunderground/mini/" + wundergroundIcons[i] + ".bmp", "/mini/" + wundergroundIcons[i] + ".bmp", _downloadCallback);
  }
  for (int i = 0; i < 24; i++) {
    tft.fillRect(0, 120, 240, 40, ILI9341_BLACK);
    webResource.downloadFile("http://www.squix.org/blog/moonphase_L" + String(i) + ".bmp", "/moon" + String(i) + ".bmp", _downloadCallback);
  }*/
}

// Update the internet based information and update screen
void updateData() {
  Serial.println(ESP.getFreeHeap());
  if (!wunderground) {
    wunderground = new WundergroundClient(IS_METRIC);
  }
  Serial.println(ESP.getFreeHeap());
  //tft.fillScreen(ILI9341_BLACK);
  //tft.setFont(&ArialRoundedMTBold_14);
  drawProgress(20, "Updating time...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  drawProgress(50, "Updating conditions...");
  wunderground->updateConditions(&currentCondition, WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  Serial.println(currentCondition.temp);
  Serial.println(currentCondition.icon);
  Serial.println(currentCondition.text);
  drawProgress(70, "Updating forecasts...");
  //wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(90, "Updating astronomy...");
  //wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  //lastUpdate = timeClient.getFormattedTime();
  //readyForWeatherUpdate = false;
  drawProgress(100, "Done...");
  delay(1000);

}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  /*ui.setTextAlignment(CENTER);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  tft.fillRect(0, 140, 240, 45, ILI9341_BLACK);
  ui.drawString(120, 160, text);
  ui.drawProgressBar(10, 165, 240 - 20, 15, percentage, ILI9341_WHITE, ILI9341_BLUE);
  */
}

// draws the clock
void drawTime() {

  gfx.setColor(MINI_DARKGREY);
  gfx.fillRect(5, 5, 240-10, 80);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  gfx.setFont(OpenSansPlain14);

  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  
  struct tm * timeinfo = localtime (&now);
  String date = ctime(&now);
  date = date.substring(0,11) + String(1900+timeinfo->tm_year);
  gfx.drawString(120, 10, date);
  char time[11];
  
#ifdef STYLE_24HR
  int hour = timeinfo->tm_hour;
  sprintf(time, "%02d %02d",hour, timeinfo->tm_min, timeinfo->tm_sec);
#else
  int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  sprintf(time, "%02d %02d",hour, timeinfo->tm_min, timeinfo->tm_sec);
#endif
  gfx.setColor(MINI_ORANGE);
  gfx.setFont(OpenSansPlain36);
  gfx.drawString(120, 28, time);
  if (timeinfo->tm_sec % 2 == 0) {
    gfx.drawString(120, 28, ":");
  }
    
  gfx.setFont(OpenSansPlain14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
#ifndef STYLE_24HR
  sprintf(time, "%s",timeinfo->tm_hour>=12?"pm":"am");
  gfx.drawString(176, 35, time);
#endif

}

// draws current weather information
void drawCurrentWeather(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  
  display->setColor(MINI_DARKGREY);
  display->fillRect(5 + x, 90 + y, 240-10, 80);
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->setColor(MINI_WHITE);
  display->setFont(OpenSansPlain14);
  display->drawString(124 + x, 104 + y, currentCondition.text);
  
  display->setColor(MINI_ORANGE);
  display->setFont(OpenSansPlain36);
  display->drawString(120 + x, 115 + y, currentCondition.temp + "°C");

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setColor(MINI_WHITE);
  display->setFont(Meteocons_Plain_42);
  display->drawString(60 + x, 110 + y, getMeteoconIcon(currentCondition.icon));
  // Weather Icon
  /*String weatherIcon = getMeteoconIcon(wunderground.getTodayIcon());
  ui.drawBmp(weatherIcon + ".bmp", 0, 55);
  
  // Weather Text
  tft.setFont(&ArialRoundedMTBold_14);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  ui.drawString(220, 90, wunderground.getWeatherText());

  tft.setFont(&ArialRoundedMTBold_36);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.setTextAlignment(RIGHT);
  String degreeSign = "F";
  if (IS_METRIC) {
    degreeSign = "C";
  }
  String temp = wunderground.getCurrentTemp() + degreeSign;
  ui.drawString(220, 125, temp);
  drawSeparator(135);*/

}

void drawForecastDetail2(MiniGrafx *display, CarouselState* state, uint16_t x, uint16_t y, String day, String temp) {
 /* ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  display->setFont(OpenSansPlain14);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  //String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  ui.drawString(x + 25, y, day);

  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(x + 25, y + 14, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));*/
  
  //String weatherIcon = getMeteoconIcon(wunderground.getForecastIcon(dayIndex));
  //ui.drawBmp("/mini/" + weatherIcon + ".bmp", x, y + 15);

  display->setColor(MINI_ORANGE);
  display->setFont(OpenSansPlain14);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  //String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  //day.toUpperCase();
  display->setFont(OpenSansPlain14);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(x, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x, y + 15, getMeteoconIcon(currentCondition.icon));

  display->setFont(OpenSansPlain14);
  display->setColor(MINI_WHITE);
  display->drawString(x, y + 35, temp);


    
}

// draws the three forecast columns
void drawForecast(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
  display->setColor(MINI_DARKGREY);
  display->fillRect(5 + x, 90 + y, 240-10, 80);
  display->setColor(MINI_ORANGE);

  drawForecastDetail2(display, state,  30 + x, 100 + y, "Wed", "9|15");
  drawForecastDetail2(display, state, 125 + x, 100 + y, "Thu", "11|17");
  drawForecastDetail2(display, state, 210 + x, 100 + y, "Fri", "8|13");


}

// helper for the forecast columns


// draw moonphase and sunrise/set and moonrise/set
void drawAstronomy() {
  /*int moonAgeImage = 24 * wunderground.getMoonAge().toInt() / 30.0;
  ui.drawBmp("/moon" + String(moonAgeImage) + ".bmp", 120 - 30, 255);
  
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&ArialRoundedMTBold_14);  
  ui.setTextAlignment(LEFT);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.drawString(20, 270, "Sun");
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(20, 285, wunderground.getSunriseTime());
  ui.drawString(20, 300, wunderground.getSunsetTime());

  ui.setTextAlignment(RIGHT);
  ui.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  ui.drawString(220, 270, "Moon");
  ui.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  ui.drawString(220, 285, wunderground.getMoonriseTime());
  ui.drawString(220, 300, wunderground.getMoonsetTime());*/
  
}

String getMeteoconIcon(String iconText) {
  if (iconText == "chanceflurries") return "F";
  if (iconText == "chancerain") return "Q";
  if (iconText == "chancesleet") return "W";
  if (iconText == "chancesnow") return "V";
  if (iconText == "chancetstorms") return "S";
  if (iconText == "clear") return "B";
  if (iconText == "cloudy") return "Y";
  if (iconText == "flurries") return "F";
  if (iconText == "fog") return "M";
  if (iconText == "hazy") return "E";
  if (iconText == "mostlycloudy") return "Y";
  if (iconText == "mostlysunny") return "H";
  if (iconText == "partlycloudy") return "H";
  if (iconText == "partlysunny") return "J";
  if (iconText == "sleet") return "W";
  if (iconText == "rain") return "R";
  if (iconText == "snow") return "W";
  if (iconText == "sunny") return "B";
  if (iconText == "tstorms") return "0";

  if (iconText == "nt_chanceflurries") return "F";
  if (iconText == "nt_chancerain") return "7";
  if (iconText == "nt_chancesleet") return "#";
  if (iconText == "nt_chancesnow") return "#";
  if (iconText == "nt_chancetstorms") return "&";
  if (iconText == "nt_clear") return "2";
  if (iconText == "nt_cloudy") return "Y";
  if (iconText == "nt_flurries") return "9";
  if (iconText == "nt_fog") return "M";
  if (iconText == "nt_hazy") return "E";
  if (iconText == "nt_mostlycloudy") return "5";
  if (iconText == "nt_mostlysunny") return "3";
  if (iconText == "nt_partlycloudy") return "4";
  if (iconText == "nt_partlysunny") return "4";
  if (iconText == "nt_sleet") return "9";
  if (iconText == "nt_rain") return "7";
  if (iconText == "nt_snow") return "#";
  if (iconText == "nt_sunny") return "4";
  if (iconText == "nt_tstorms") return "&";

  return ")";
}



// if you want separators, uncomment the tft-line
void drawSeparator(uint16_t y) {
   //tft.drawFastHLine(10, y, 240 - 2 * 10, 0x4228);
}

