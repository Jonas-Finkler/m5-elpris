#include <Arduino.h>
#include "EspWizLight.h"
#include "M5EPD.h"
#include "WiFiCredentials.h"
#include "WiFi.h"
#include "ArduinoJson.h"
#include <HTTPClient.h>
#include "ElPris.h"

#include <Preferences.h>


const bool LOGGING_ENABLED = false;

const char* NTP_SERVER = "dk.pool.ntp.org";
const char* MY_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

M5EPD_Canvas canvas(&M5.EPD);
ElPris elPris;
Preferences preferences;

void connectWifi(const char* ssid, const char* password);
void connectNTP();
void sendLogMessage(String message);
void showWakeupIndicator();
void syncSystemClockFromRTC();
void updateRTCFromSystemClock();
void printTime(const char* message, time_t t);

void setup() {

  // TODO:
  // Optimize power
  // - disable Serial
  // - only call ntp once
  // Handle errors
  // - parsing (display on screen)
  // - WiFi and NTP (display on screen and sleep)

  // NOTE: To clear Preferences
  // preferences.begin("rtc", false);
  // preferences.clear();
  // preferences.end();
  // preferences.begin("elpris", false);
  // preferences.clear();
  // preferences.end();
  // return;


  M5.begin();
  M5.RTC.begin();
  Serial.begin(115200);

  canvas.createCanvas(960, 540);

  // Set timezone
  setenv("TZ", MY_TZ, 1); 
  tzset();

  // RTC Stuff
  syncSystemClockFromRTC();

  time_t now;
  time(&now);
  printTime("RTC Time: ", now);

  bool needsFetch = true;

  preferences.begin("rtc", true);
  if (preferences.isKey("lastSync")) {
    time_t lastSync;
    preferences.getBytes("lastSync", &lastSync, sizeof(lastSync));
    printTime("Last sync from NTP: ", lastSync);
    // if last fetch is less than 24h ago, no need to update rtc
    if (difftime(now, lastSync) < 24 * 60 * 60) {
      Serial.println("No need to sync NTP");
      needsFetch = false;
    }
  } else {
    Serial.println("No last sync time stored");
  }
  preferences.end();





  if (!needsFetch) {
    bool loaded = elPris.load();
    if (loaded) {
      Serial.println("Loaded prices from preferences");
      needsFetch = elPris.update();
    } else {
      Serial.println("No prices in preferences");
      needsFetch = true;
    }
  }

  if (needsFetch) {
    Serial.println("Need to fetch new data and sync NTP");
    // only show this if we update, otherwise we are done quick
    showWakeupIndicator();
    connectWifi(WIFI_SSID, WIFI_PASSWORD);
    sendLogMessage("Waking up");

    connectNTP();
    time_t now;
    struct tm timeinfo;
    time(&now);
    printTime("NTP Time: ", now);
    updateRTCFromSystemClock();

    preferences.begin("rtc", false);
    preferences.putBytes("lastSync", &now, sizeof(now));
    preferences.end();

    elPris.fetch();
    elPris.save();
  }







  /*M5.EPD.SetRotation(90);*/
  /*M5.TP.SetRotation(90);*/
  /*M5.EPD.Clear(true);*/
  elPris.draw(canvas);
  Serial.println("Setup done");


  delay(1000); // make sure everything is done before going to sleep

  time(&now);
  printTime("Going to sleep: ", now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  // It seems the M5s clock runs too fast (~130s/h) we therefore overshoot by 5 mins to avoid waking up twice per hour
  int sleepTime = 60 * 60 - timeinfo.tm_min * 60 - timeinfo.tm_sec + 5 * 60;
  Serial.print("Sleeping for ");
  Serial.println(sleepTime);
  sendLogMessage("Sleeping for: " + String(sleepTime));
  M5.shutdown(sleepTime);
}

void loop() {

}


void printTime(const char* message, time_t t) {
  Serial.print(message);
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  Serial.print(String(asctime(&timeinfo))); // Includes new line from asctime
}

void showWakeupIndicator() {
  // int rectSize = 40;
  // int x = random(0, canvas.width() - rectSize);
  // int y = random(0, canvas.height() - rectSize);
  // canvas.fillRect(x, y, rectSize, rectSize, 15);  // 15 = black, 0 = white
  // NOTE: Clearing is enough for our use case
  canvas.clear(); 
  canvas.setTextColor(10);
  canvas.setTextSize(2);
  canvas.drawString("Updating", 40, 20);
  // M5.EPD.WriteFullGram4bpp((uint8_t*)canvas.frameBuffer());
  // M5.EPD.UpdateArea(0,0,canvas.width() / 2, 45, UPDATE_MODE_GC16);
  canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
  canvas.clear();
}


void connectNTP() {
  // reset time to 0
  struct timeval tv = {0};
  settimeofday(&tv, nullptr);


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

void syncSystemClockFromRTC() {
  // Set timezone
  // setenv("TZ", MY_TZ, 1); 
  // tzset();

  rtc_time_t rtcTime;
  rtc_date_t rtcDate;

  M5.RTC.getTime(&rtcTime);
  M5.RTC.getDate(&rtcDate);

  struct tm timeinfo = {};

  timeinfo.tm_year = rtcDate.year - 1900;   
  timeinfo.tm_mon  = rtcDate.mon - 1;    // tm_mon is 0–11
  timeinfo.tm_mday = rtcDate.day;

  timeinfo.tm_hour = rtcTime.hour;
  timeinfo.tm_min  = rtcTime.min;
  timeinfo.tm_sec  = rtcTime.sec;

  timeinfo.tm_isdst = -1;  // let mktime determine if DST is in effect

  time_t t = mktime(&timeinfo);   // convert to Unix time

  struct timeval now;
  now.tv_sec = t;
  now.tv_usec = 0;

  // = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&now, nullptr);

  Serial.println("System clock updated from RTC.");
}

void updateRTCFromSystemClock() {
  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);
  rtc_time_t rtcTime;
  rtc_date_t rtcDate;
  rtcTime.hour = timeinfo->tm_hour;
  rtcTime.min  = timeinfo->tm_min;
  rtcTime.sec  = timeinfo->tm_sec;
  rtcDate.year = timeinfo->tm_year + 1900;  // tm_year since 1900 → RTC year since 2000
  rtcDate.mon  = timeinfo->tm_mon + 1;   // tm_mon is 0–11
  rtcDate.day  = timeinfo->tm_mday;
  M5.RTC.setTime(&rtcTime);
  M5.RTC.setDate(&rtcDate);
  Serial.println("RTC updated from system clock.");
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

const char* logServerIP = "192.168.1.89";  
const uint16_t logServerPort = 4210;

void sendLogMessage(String message) {
  if (!LOGGING_ENABLED) {
    return;
  }

  WiFiClient client;

  if (client.connect(logServerIP, logServerPort)) {
    client.println(message);
    client.stop();
    Serial.println("Message sent.");
  } else {
    Serial.println("Logging failed");
  }
}
