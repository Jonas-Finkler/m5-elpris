#include <Arduino.h>
#include "EspWizLight.h"
#include "M5EPD.h"
#include "WiFiCredentials.h"
#include "WiFi.h"
#include "ArduinoJson.h"
#include <HTTPClient.h>
#include "ElPris.h"




const char* NTP_SERVER = "dk.pool.ntp.org";
const char* MY_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

M5EPD_Canvas canvas(&M5.EPD);
ElPris elPris;

void connectWifi(const char* ssid, const char* password);
void connectNTP();

void setup() {

  // TODO: Use the RTC to check if anything needs to be updated before proceeding
  // Also optimize power
  // - disable Serial
  // - only call ntp once
  // Also Handle errors
  // - parsing (display on screen)
  // - WiFi and NTP (display on screen and sleep)



  M5.begin();
  Serial.begin(115200);
  connectWifi(WIFI_SSID, WIFI_PASSWORD);
  connectNTP();

  /*M5.EPD.SetRotation(90);*/
  /*M5.TP.SetRotation(90);*/
  /*M5.EPD.Clear(true);*/
  canvas.createCanvas(960, 540);

  elPris.update();
  elPris.draw(canvas);
  Serial.println("Setup done");

  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);


  delay(1000); // make sure everything is done before going to sleep
  int sleepTime = 60 * 60 - timeinfo.tm_min * 60 - timeinfo.tm_sec + 30;
  Serial.print("Sleeping for ");
  Serial.println(sleepTime);
  M5.shutdown(sleepTime);
}

void loop() {

}


void connectNTP() {
  configTime(0, 0, NTP_SERVER);
  // set the time zone (has to be done after connecting to ntp)
  setenv("TZ", MY_TZ, 1);  //  Now adjust the time zone
  tzset();
  time_t now;
  time(&now);
  Serial.print("Waiting for NTP");
  while (now < 10000) {
    delay(100);
    Serial.print(".");
    time(&now);
  }
  Serial.println();
}

void connectWifi(const char* ssid, const char* password) {
  Serial.print("Connecting to WiFi ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}
