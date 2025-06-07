

#include <Arduino.h>
#include "HTTPClient.h"
#include "M5EPD_Canvas.h"

struct Price {
  float price;
  struct tm time;
};

struct Prices {
  bool available = false;
  int nPrices = 0;
  Price prices[25]; // max 25 hours per day (DLST)
};

class ElPris {
public:
  ElPris();
  void draw(M5EPD_Canvas canvas);
  void update();
private:
  Prices pricesYesterday;
  Prices pricesToday;
  Prices pricesTomorrow;
  struct tm lastUpdate; // TODO: should probably be time_t
  struct tm getNow();
  struct tm getDay(int dayOffset);
  static struct tm parseApiTime(const char* timeStr);
  struct Prices fetchPrices(int dayOffset = 0);
};


ElPris::ElPris() {
}


struct tm ElPris::getNow() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return timeinfo;
}

struct tm ElPris::getDay(int dayOffset) {
  struct tm timeinfo = getNow();
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t now = mktime(&timeinfo);
  now += dayOffset * 24 * 60 * 60;
  localtime_r(&now, &timeinfo);
  return timeinfo;
}

void ElPris::update() {
  struct tm now = getNow();
  // check if day has changed
  if (pricesToday.available && now.tm_mday != lastUpdate.tm_mday) {
    pricesYesterday = pricesToday;
    pricesToday = pricesTomorrow;
  }
  // TOOO: No need to get this if it's not displayed
  if (!pricesYesterday.available) {
    pricesYesterday = fetchPrices(-1);
  }

  if (!pricesToday.available) {
    pricesToday = fetchPrices();
  }

  if (!pricesTomorrow.available && now.tm_hour >= 13) {  // they are released at 13:00
    pricesTomorrow = fetchPrices(1);

  }
  lastUpdate = now;
};

void ElPris::draw(M5EPD_Canvas canvas) {
  struct tm now = getNow();  // TODO: cache this to save power
  struct Price allPrices[50];
  int iPriceNow = -1; 
  int nPrices;
  if (pricesTomorrow.available) {
    if (!(pricesToday.available && pricesTomorrow.available)) {
      Serial.println("Prices not there :(");
      return;
    }
    for (int i = 0; i < pricesToday.nPrices; i++) {
      allPrices[i] = pricesToday.prices[i];
      if (pricesToday.prices[i].time.tm_hour == now.tm_hour) {
        iPriceNow = i;
      }
    }
    for (int i = 0; i < pricesTomorrow.nPrices; i++) {
      allPrices[i + pricesToday.nPrices] = pricesTomorrow.prices[i];
    }
    nPrices = pricesToday.nPrices + pricesTomorrow.nPrices;
  } else {
    if (!(pricesToday.available && pricesYesterday.available)) {
      Serial.println("Prices not there :(");
      return;
    }
    for (int i = 0; i < pricesYesterday.nPrices; i++) {
      allPrices[i] = pricesYesterday.prices[i];
    }
    for (int i = 0; i < pricesToday.nPrices; i++) {
      allPrices[i + pricesYesterday.nPrices] = pricesToday.prices[i];
      if (pricesToday.prices[i].time.tm_hour == now.tm_hour) {
        iPriceNow = i + pricesYesterday.nPrices;
      }
    }
    nPrices = pricesYesterday.nPrices + pricesToday.nPrices;
  }

  // TODO: Think about negative prices 
  // Add line at 0, draw negative lines downwards

  // 0 is always in the plot
  float minPrice = 0.;
  float maxPrice = 0.;
  for (int i = 0; i < nPrices; i++) {
    minPrice = min(minPrice, allPrices[i].price);
    maxPrice = max(maxPrice, allPrices[i].price);
  }
  int w = canvas.width();
  int h = canvas.height();
  int borderL = 40;
  int borderR = 110;
  int borderT = 60;
  int borderB = 50;
  int graphW = w - borderL - borderR;
  int graphH = h - borderT - borderB;

  canvas.setTextSize(2);
  for (int i = 0; i < nPrices; i++) {
    if ((allPrices[i].time.tm_hour % 6 == 0 && abs(i - iPriceNow) > 2)
      || (i == iPriceNow)) {
      int blackness = i == iPriceNow ? 15 : 10;
      int x = borderL + (i + 1) * graphW / (nPrices + 1);
      int y = borderT + graphH - graphH * (allPrices[i].price - minPrice) / (maxPrice - minPrice);
      int hour = allPrices[i].time.tm_hour;
      /*hour = hour == 0 ? 24 : hour;*/
      int strWidth = canvas.textWidth(String(hour));
      canvas.setTextColor(blackness);
      canvas.drawString(String(hour), x - strWidth / 2, h - borderB + 10);
    }
  }
  int strHeight = canvas.fontHeight();
  int yNow = borderT + graphH - graphH * (allPrices[iPriceNow].price - minPrice) / (maxPrice - minPrice);
  if (yNow - borderT > 2 * strHeight) {
    canvas.setTextColor(10);
    canvas.drawString(String(maxPrice, 3), w - borderR + 10, borderT - strHeight / 2);
  }
  if (h - borderB - yNow > 2 * strHeight) {
    canvas.setTextColor(10);
    canvas.drawString(String(minPrice, 3), w - borderR + 10, h - borderB - strHeight / 2);
  }
  canvas.drawLine(borderL, yNow, w - borderR, yNow, 2, 5);
  canvas.setTextColor(15);
  canvas.drawString(String(allPrices[iPriceNow].price, 3), w - borderR + 10, yNow - strHeight / 2);

  for (int i = 0; i < nPrices; i++) {
    int blackness = i == iPriceNow ? 15 : 7;
    int x = borderL + (i + 1) * graphW / (nPrices + 1);
    int y = borderT + graphH - graphH * (allPrices[i].price - minPrice) / (maxPrice - minPrice);
    /*canvas.fillCircle(x, y, 6, blackness);*/
    /*canvas.fillCircle(x, h - borderB, 6, blackness);*/
    canvas.drawLine(x, y, x, h - borderB, 12, blackness);
  }

  canvas.drawLine(borderL, borderT, w - borderR, borderT, 2, 15);
  canvas.drawLine(borderL, h - borderB, w - borderR, h - borderB, 2, 15);
  canvas.drawLine(borderL, borderT, borderL, h - borderB, 2, 15);
  canvas.drawLine(w - borderR, borderT, w - borderR, h - borderB, 2, 15);
  
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%H:%M:%S %d/%m/%Y", &now);
  canvas.setTextColor(15);
  canvas.drawString(dateStr, borderL, 20);

  // print battery voltage
  float batteryVoltage = M5.getBatteryVoltage() / 1000.0;
  String batteryStr = String(batteryVoltage, 3) + " V";
  int strWidth = canvas.textWidth(batteryStr);
  canvas.drawString(batteryStr, w - borderR - strWidth, 20);

  /*canvas.setTextSize(2);*/
  /*canvas.setTextColor(M5EPD_BLACK);*/
  /*canvas.fillScreen(M5EPD_WHITE);*/
  /*canvas.drawString("Elpris", 10, 10);*/
  /*for (int i = 0; i < nPrices; i++) {*/
  /*  canvas.drawString(String(prices[i]), 10, 30 + i * 20);*/
  /*}*/
  canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
}

struct Prices ElPris::fetchPrices(int dayOffset) {
  Prices prices;
  HTTPClient http;
  
  // Get current date in YYYY/MM-DD format
  struct tm timeinfo = getDay(dayOffset);

  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y/%m-%d", &timeinfo);
  
  // Construct URL with current date
  String url = "https://www.elprisenligenu.dk/api/v1/prices/";
  url += dateStr;
  url += "_DK1.json";

  Serial.print("Fetching: ");
  Serial.println(url);
  
  http.begin(url);
  
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed, error: %s\n", 
    http.errorToString(httpCode).c_str());
    http.end();
    return prices;
  }
  String payload = http.getString();
  http.end();
  Serial.print("Got: ");
  Serial.println(payload);
  
  // Parse JSON
  JsonDocument doc; // Adjust size if needed
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("JSON parsing failed!");
    return prices;
  }

  Serial.println("JSON parsed");
  
  JsonArray array = doc.as<JsonArray>();
  int c = 0;
  for (JsonVariant value : array) {
    prices.prices[c].price = value["DKK_per_kWh"].as<float>();
    Serial.print("Price: ");
    Serial.print(prices.prices[c].price);
    struct tm ti = parseApiTime(value["time_start"]);
    prices.prices[c].time = ti;
    c++;
    Serial.print(" at ");
    Serial.println(value["time_start"].as<const char*>());
    Serial.println(c);
  }
  prices.nPrices = c;
  prices.available = true;
  return prices;
  
}

struct tm ElPris::parseApiTime(const char* timeStr) {
  struct tm timeinfo;
  // API time format is like "2024-01-23T00:00:00"
  int year, month, day, hour, min, sec;
  sscanf(timeStr, "%d-%d-%dT%d:%d:%d", 
         &year, &month, &day, &hour, &min, &sec);
  
  timeinfo.tm_year = year - 1900;  // Years since 1900
  timeinfo.tm_mon = month - 1;     // Months are 0-11
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hour;
  timeinfo.tm_min = min;
  timeinfo.tm_sec = sec;
  timeinfo.tm_isdst = -1;          // Let system determine DST
  return timeinfo;
}
