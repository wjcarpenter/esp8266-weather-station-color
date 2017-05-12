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

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "WundergroundClient.h"

WundergroundClient::WundergroundClient(boolean _isMetric) {
  isMetric = _isMetric;
}


// end add fowlerk, 12/22/16

void WundergroundClient::updateConditions(CurrentCondition *condition, String apiKey, String language, String country, String city) {
  isCurrentCondition = true;
  isForecast = false;
  this->currentCondition = condition;
  //condition->temp = "12.2";
  doUpdate("/api/" + apiKey + "/conditions/lang:" + language + "/q/" + country + "/" + city + ".json");
}

void WundergroundClient::updateForecast(Forecast *forecasts, uint8_t numberOfConditions, String apiKey, String language, String country, String city) {
  isForecast = true;
  isCurrentCondition = false;
  isSimpleForecast = false;
  this->forecasts = forecasts;
  this->numberOfConditions = numberOfConditions;
  doUpdate("/api/" + apiKey + "/forecast/lang:" + language + "/q/" + country + "/" + city + ".json");
}

void WundergroundClient::doUpdate(String url) {
  JsonStreamingParser parser;
  parser.setListener(this);
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect("api.wunderground.com", httpPort)) {
    Serial.println("connection failed");
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.wunderground.com\r\n" +
               "Connection: close\r\n\r\n");
  int retryCounter = 0;
  while(!client.available()) {
    delay(1000);
    retryCounter++;
    if (retryCounter > 10) {
      return;
    }
  }

  int pos = 0;
  boolean isBody = false;
  char c;

  int size = 0;
  client.setNoDelay(false);
  while(client.connected()) {
    while((size = client.available()) > 0) {
      c = client.read();
      if (c == '{' || c == '[') {
        isBody = true;
      }
      if (isBody) {
        parser.parse(c);
      }
    }
  }
}

void WundergroundClient::whitespace(char c) {
  Serial.println("whitespace");
}

void WundergroundClient::startDocument() {
  Serial.println("start document");
}

void WundergroundClient::key(String key) {
  currentKey = String(key);
  if (key == "simpleforecast") {
    isSimpleForecast = true;
  }
}

void WundergroundClient::value(String value) {
  //Serial.println(currentKey + ": " + value);
  if (isCurrentCondition) {
    processCurrentCondition(value);
  }
  if (isForecast) {
    processForecast(String value);
  }

}

void WundergroundClient::processCurrentCondition(String value) {
    if (currentKey == "temp_f" && !isMetric) {
      this->currentCondition->temp = value;
    }
    if (currentKey == "temp_c" && isMetric) {
      this->currentCondition->temp = value;
    }
    if (currentKey == "icon") {
        this->currentCondition->icon = value;
    }
    if (currentKey == "weather") {
      this->currentCondition->text = value;
    }
}

void WundergroundClient::processForecast(String value) {
  /*
    period: 1,
    icon: "nt_chancerain",
    icon_url: "http://icons.wxug.com/i/c/k/nt_chancerain.gif",
    title: "Thursday Night",
    fcttext: "Cloudy with occasional rain showers. Low 51F. Winds light and variable. Chance of rain 50%.",
    fcttext_metric: "Considerable cloudiness with occasional rain showers. Low 11C. Winds light and variable. Chance of rain 50%.",
    pop: "50"
   */
  if (currentKey == "period") {
    this->currentPeriod = value.toInt();
  }
  if (curretPeriod < numberOfConditions) {
    if (currentKey == "icon") {
      forecasts[current]->icon = value;
    }
  }
}

void WundergroundClient::endArray() {

}


void WundergroundClient::startObject() {
  currentParent = currentKey;
}

void WundergroundClient::endObject() {
  currentParent = "";
}

void WundergroundClient::endDocument() {

}

void WundergroundClient::startArray() {

}


