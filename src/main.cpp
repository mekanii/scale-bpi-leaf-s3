#include <Arduino.h>
#include "monofontoReg20.h"
#include "monofontoReg28.h"
#include "monofontoReg96.h"
#include <WiFi.h>
#include <WebServer.h>
#include "ArduinoJson.h"
#include "FS.h"
#include "SPIFFS.h"
#include "Encoder.h"
#include <RTClib.h>
#include <HX711_ADC.h>
#include "TFT_eSPI.h"

#define HYSTERESIS 5.0f
#define HYSTERESIS_STABLE_CHECK 1.0f
#define STABLE_READING_REQUIRED 20
float lastWeight = 0.0f;
int stableReadingsCount = 0;

#define MONOFONTO20 monofonto20
#define MONOFONTO28 monofonto28
#define MONOFONTO96 monofonto96

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define FORMAT_SPIFFS_IF_FAILED true

// Define pins for the rotary encoder
#define ENCODER_PIN_A 9
#define ENCODER_PIN_B 14
#define ENCODER_BUTTON_PIN 46

// Define pins for the Load Cell
#define HX711_DT 48
#define HX711_SCK 45

#define WIFI_SSID "creati"
#define WIFI_PASS "qwertyui"
#define SSID "BPI-LEAF-S3_01"
#define PASS "12345678"

const char* ssid = "BPI-LEAF-S3_01";
const char* password = "12345678";

WebServer server(80);

DynamicJsonDocument doc(1024);
JsonArray parts;

HX711_ADC LoadCell(HX711_DT, HX711_SCK);
unsigned long t = 0;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);

RTC_DS3231 rtc;

bool RTC_STATUS = false;

int selectorIndex = 0;
int lastSelectorIndex = -1;

int page1Name = -1;
int page2Name = -1;
int page3Name = -1;

String partName = "";
float partStd = 0;

int CHECK_STATUS = 0;
boolean CONN_STATUS = false;

const char* optsMenu[2] = {
  "START",
  "SETTINGS"
};

const char* optsOption[3] = {
  "CONTINUE",
  "TARE",
  "EXIT"
};

const char* optsExitDialog[2] = {
  "No, CANCEL",
  "Yes, EXIT",
};

float weight = 0.00;

void rtcInit() {
  // rtc.adjust(DateTime(2023, 2, 11, 18, 48, 0));
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
//    while (1) delay(10);
  } else {
    RTC_STATUS = true;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

String dateTimeStart() {
  if (RTC_STATUS) {
    DateTime dateTime = rtc.now();
    return "START " + String(dateTime.year()) + "-" + String(dateTime.month()) + "-" + String(dateTime.day()) + " " + String(dateTime.hour()) + ":" + String(dateTime.minute());
  } else {
    return "RTC ERR: 0000-00-00 00:00";
  }
}

String dateTimeNow() {
  if (RTC_STATUS) {
    DateTime dateTime = rtc.now();
    return String(dateTime.year()) + "-" + String(dateTime.month()) + "-" + String(dateTime.day()) + " " + String(dateTime.hour()) + ":" + String(dateTime.minute());
  } else {
    return "RTC ERR: 0000-00-00 00:00";
  }
}

void displayMenu() {
  spr.loadFont(MONOFONTO28);
  for (int i = 0; i < ARRAY_SIZE(optsMenu); i++) {
    tft.setCursor(30, 10 + i * spr.fontHeight());
    if (i == selectorIndex) {
      spr.setTextColor(TFT_ORANGE, TFT_BLACK);
      spr.printToSprite(String(" > ") + optsMenu[i]);
    } else {
      spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      spr.printToSprite(String("    ") + optsMenu[i]);
    }
  }
  spr.unloadFont();
}

void dispalyMenuDisabled() {
  spr.loadFont(MONOFONTO28);
  for (int i = 0; i < ARRAY_SIZE(optsMenu); i++) {
    tft.setCursor(30, 10 + i * spr.fontHeight());
    if (i == selectorIndex) {
      spr.setTextColor(TFT_BROWN, TFT_BLACK);
      spr.printToSprite(String(" > ") + optsMenu[i]);
    } else {
      spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
      spr.printToSprite(String("    ") + optsMenu[i]);
    }
  }
  spr.unloadFont();
}

void displayPart() {
  spr.loadFont(MONOFONTO28);
  tft.setCursor(200, 10);
  if (selectorIndex == -1) {
    spr.setTextColor(TFT_ORANGE, TFT_BLACK);
    spr.printToSprite(String(" > back"));
  } else {
    spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    spr.printToSprite(String("    back"));
  }

  // Serial.println(partList.size());
  for (int i = 0; i < parts.size(); i++) {
    // Serial.println(partList[i]["name"].as<String>());
    tft.setCursor(200, 10 + ((i + 1) * spr.fontHeight()));
    if (i == selectorIndex) {
      spr.setTextColor(TFT_ORANGE, TFT_BLACK);
      spr.printToSprite(String(" > ") + parts[i]["name"].as<String>());
    } else {
      spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      spr.printToSprite(String("    ") + parts[i]["name"].as<String>());
    }
  }
  spr.unloadFont();
}

void displayMainFrame() {
  spr.loadFont(MONOFONTO20);
  spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(10, 10);
  spr.printToSprite(String(dateTimeStart()));
  
  spr.unloadFont();
  spr.loadFont(MONOFONTO28);
  spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(10, 10 + (1 * spr.fontHeight()));
  spr.printToSprite(partName);
  tft.setCursor(10, 10 + (2 * spr.fontHeight()));
  spr.printToSprite(String(partStd, 2));
  spr.unloadFont();
  
  spr.loadFont(MONOFONTO96);
  spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(372, 120);
  spr.printToSprite("gr");
  spr.deleteSprite();
  spr.unloadFont();
}

void displayMain(float sc) {
  spr.loadFont(MONOFONTO96);
  spr.createSprite(372, 96);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.fillSprite(TFT_BLACK);
  spr.drawFloat(sc, 2, 360, 0);
  spr.pushSprite(0, 120);
  spr.unloadFont();
}

void displayCheckStatus() {
  spr.loadFont(MONOFONTO96);
  spr.createSprite(108, 96);
  spr.setTextDatum(TL_DATUM);
  if (CHECK_STATUS != 0) {
    spr.fillSprite(TFT_BLACK);
    if (CHECK_STATUS == 1) {
      spr.setTextColor(TFT_GREEN, TFT_BLACK);
      spr.drawString("OK", 0, 0);
    } else if (CHECK_STATUS == 2) {
      spr.setTextColor(TFT_RED, TFT_BLACK);
      spr.drawString("NG", 0, 0);
    }
    spr.pushSprite(372, 216);
    delay(2000);
  } else {
    spr.fillSprite(TFT_BLACK);
    spr.pushSprite(372, 216);
  }
  spr.deleteSprite();
  spr.unloadFont();
}

void displayHeader(String text) {
  spr.loadFont(MONOFONTO28);
  spr.createSprite(480, spr.fontHeight());
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.fillSprite(TFT_BLACK);
  spr.drawString(text, 240, 0);
  spr.pushSprite(0, 40);
  tft.drawFastHLine(0, 80, 480, TFT_WHITE);
  spr.deleteSprite();
  spr.unloadFont();
}

void displayOption() {
  spr.loadFont(MONOFONTO28);
  spr.createSprite(480, spr.fontHeight());
  spr.setTextDatum(TC_DATUM);
  for (int i = 0; i < ARRAY_SIZE(optsOption); i++) {
    if (i == selectorIndex) {
      spr.setTextColor(TFT_ORANGE, TFT_BLACK);
      spr.fillSprite(TFT_BLACK);
      spr.drawString(optsOption[i], 240, 0);
      spr.pushSprite(0, 100 + (i * 40));
    } else {
      spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.fillSprite(TFT_BLACK);
      spr.drawString(optsOption[i], 240, 0);
      spr.pushSprite(0, 100 + (i * 40));
    }
  }
  spr.deleteSprite();
  spr.unloadFont();
}

void displayExitDialog() {
  spr.loadFont(MONOFONTO28);
  spr.createSprite(480, spr.fontHeight());
  spr.setTextDatum(TC_DATUM);
  for (int i = 0; i < ARRAY_SIZE(optsExitDialog); i++) {
    if (i == selectorIndex) {
      spr.setTextColor(TFT_ORANGE, TFT_BLACK);
      spr.fillSprite(TFT_BLACK);
      spr.drawString(optsExitDialog[i], 240, 0);
      spr.pushSprite(0, 100 + (i * 40));
    } else {
      spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.fillSprite(TFT_BLACK);
      spr.drawString(optsExitDialog[i], 240, 0);
      spr.pushSprite(0, 100 + (i * 40));
    }
  }
  spr.deleteSprite();
  spr.unloadFont();
}

bool constrainer(int *newPosition, int min, int arrSize) {
  bool state = false;
  selectorIndex = constrain(*newPosition, min, arrSize - 1);
  if (selectorIndex != lastSelectorIndex) {
    lastSelectorIndex = selectorIndex;
    state = true;
  } else if (selectorIndex == arrSize - 1) {
    *newPosition = selectorIndex;
    encoder.write(selectorIndex * 2);
  } else if (selectorIndex == min) {
    *newPosition = 0;
    encoder.write(0);
  }
  return state;
}

bool checkStableState(float wt) {
  if (wt >= wt - HYSTERESIS_STABLE_CHECK && wt <= wt + HYSTERESIS_STABLE_CHECK && abs(wt - lastWeight) <= HYSTERESIS_STABLE_CHECK) {
  // if (abs(wt - lastWeight) < HYSTERESIS_STABLE_CHECK) {
    stableReadingsCount++;
  } else {
    stableReadingsCount = 0;
  }
  lastWeight = wt;
    
  return stableReadingsCount >= STABLE_READING_REQUIRED;
}

void rotarySelector() {
  int newPosition = encoder.read() / 2;
  if (newPosition != selectorIndex) {
    switch(page1Name) {
      case 0: // menu
        switch(page2Name) {
          default:  // menu selection
            if (constrainer(&newPosition, 0, ARRAY_SIZE(optsMenu))) {
              displayMenu();
            }
            break;
          case 0:   // start
            switch(page3Name) {
              default:  // part selection
                if (constrainer(&newPosition, -1, parts.size())) {
                  displayPart();
                }
                break;
              case 0:   //main
                displayMain(getScale());
                break;
              case 1:   // options
                if (constrainer(&newPosition, 0, ARRAY_SIZE(optsOption))) {
                  displayOption();
                }
                break;
              case 2:   // exit dialog
                if (constrainer(&newPosition, 0, ARRAY_SIZE(optsExitDialog))) {
                  displayExitDialog();
                }
                break;
            }
            break;
          case 1:   // settings
            break;
        }
        break;
    }
  }
  else if (page1Name == 0 && page2Name == 0 && page3Name == 0) {
    float weight = getScale();
    displayMain(weight);

    if (checkStableState(weight)) {
      // if (weight >= 0 - HYSTERESIS && weight <= 0 + HYSTERESIS) {
      if (weight <= partStd * 0.1f) {
        CHECK_STATUS = 0;
      } else if (weight >= partStd - HYSTERESIS && weight <= partStd + HYSTERESIS && CHECK_STATUS == 0) {
        CHECK_STATUS = 1;
        // beep.OK();
      } else if (CHECK_STATUS == 0) {
        CHECK_STATUS = 2;
        // beep.NG();
      }
      displayCheckStatus();
    }
  }
}

void rotaryButton() {
  if (digitalRead(ENCODER_BUTTON_PIN) == LOW) {
    encoder.write(0);
    switch(page1Name) {
      case 0: // menu
        switch(page2Name) {
          default:  // menu selection
            switch (selectorIndex){
              case 0:
                selectorIndex = 0;
                dispalyMenuDisabled();
                displayPart();
                page2Name = 0;
                break;
              case 1:
                break;
            }
            break;
          case 0:   // start
            switch(page3Name) {
              default:  // part selection
                if (selectorIndex == -1) {
                  selectorIndex = 0;
                  tft.fillScreen(TFT_BLACK);
                  displayMenu();
                  page2Name = -1;
                } else {
                  tft.fillScreen(TFT_BLACK);
                  partName = parts[selectorIndex]["name"].as<String>();
                  partStd = parts[selectorIndex]["std"].as<float>();
                  selectorIndex = 0;
                  displayMainFrame();
                  displayMain(getScale());
                  page3Name = 0;
                }
                break;
              case 0:   //main
                selectorIndex = 0;
                tft.fillScreen(TFT_BLACK);
                displayHeader("OPTIONS");
                displayOption();
                page3Name = 1;
                break;
              case 1:   // options
                switch (selectorIndex) {
                  case 0:   // continue
                    selectorIndex = 0;
                    tft.fillScreen(TFT_BLACK);
                    displayMainFrame();
                    displayMain(getScale());
                    page3Name = 0;
                    break;
                  case 1:   // tare
                    boolean _resume = false;
                    while (_resume == false) {
                      LoadCell.update();
                      LoadCell.tareNoDelay();
                      
                      if (LoadCell.getTareStatus() == true) {
                        Serial.println("Tare complete");
                        _resume = true;
                      }
                    }
                    selectorIndex = 0;
                    tft.fillScreen(TFT_BLACK);
                    displayMainFrame();
                    displayMain(getScale());
                    page3Name = 0;
                    break;
                  case 2:   // exit
                    selectorIndex = 0;
                    tft.fillScreen(TFT_BLACK);
                    displayHeader("Are you sure?");
                    displayExitDialog();
                    page3Name = 2;
                    break;
                }
                break;
              case 2:   // exit dialog
                switch (selectorIndex) {
                  case 0:   // continue
                    selectorIndex = 0;
                    tft.fillScreen(TFT_BLACK);
                    displayMainFrame();
                    displayMain(getScale());
                    page3Name = 0;
                    break;
                  case 1:   // exit
                    selectorIndex = 0;
                    tft.fillScreen(TFT_BLACK);
                    displayMenu();
                    page1Name = 0;
                    page2Name = -1;
                    page3Name = -1;
                    break;
                }
                break;
            }
            break;
          case 1:   // settings
            break;
        }
        break;
    }
    selectorIndex = 0;
    lastSelectorIndex = -1;
    delay(500);
  }
}

DynamicJsonDocument getPartList() {
  const size_t capacity = JSON_ARRAY_SIZE(9) + JSON_OBJECT_SIZE(9) + 400;
  doc.clear(); // Clear the document before use

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return DynamicJsonDocument(0);
  }
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("Failed to read file, returning empty JSON document");
    return DynamicJsonDocument(0);
  }

  // Check if partList key exists and is an array
  if (!doc.containsKey("partList") || !doc["partList"].is<JsonArray>()) {
    Serial.println("partList key is missing or not an array");
    return DynamicJsonDocument(0);
  }

  // Print the part list to Serial Monitor
  JsonArray partListArray = doc["partList"].as<JsonArray>();
  if (partListArray.size() == 0) {
    Serial.println("partList array is empty");
  } else {
    for (JsonObject part : partListArray) {
      Serial.println("id: " + String(part["id"].as<int>()));
      Serial.println("name: " + part["name"].as<String>());
      Serial.println("std: " + String(part["std"].as<float>(), 2));
      Serial.println("unit: " + part["unit"].as<String>());
    }
  }

  // Return the document containing the part list array
  return doc;
}

JsonObject getPart(int id) {
  DynamicJsonDocument temp(1024);

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return JsonObject();
  }
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    Serial.println("Failed to read file, returning empty JSON document");
    return JsonObject();
  }

  JsonArray partListArray = temp["partList"].as<JsonArray>();
  for (JsonObject part : partListArray) {
    if (part["id"].as<int>() == id) {
      Serial.println("id: " + String(part["id"].as<int>()));
      Serial.println("name: " + part["name"].as<String>());
      Serial.println("std: " + String(part["std"].as<float>(), 2));
      Serial.println("unit: " + part["unit"].as<String>());
      return part;
    }
  }
  return JsonObject();
}

void createPart(String name, float std, String unit) {
  const size_t capacity = JSON_ARRAY_SIZE(9) + JSON_OBJECT_SIZE(9) + 400;
  DynamicJsonDocument temp(capacity);

  // Try to read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading, creating new file");
    // Create a new file
    file = SPIFFS.open("/partList.json", "w");
    if (!file) {
      Serial.println("Failed to create new file");
      return;
    }
    // Initialize the JSON structure
    JsonArray partListArray = temp.createNestedArray("partList");
    file.close();
  } else {
    DeserializationError error = deserializeJson(temp, file);
    file.close();
    if (error) {
      Serial.println("Failed to read file, using empty JSON");
    }
  }

  JsonArray partListArray;
  if (!temp.containsKey("partList") || !temp["partList"].is<JsonArray>()) {
    partListArray = temp.createNestedArray("partList");
  } else {
    partListArray = temp["partList"].as<JsonArray>();
  }

  int id = partListArray[partListArray.size() - 1]["id"].as<int>() + 1;

  // Check if part with the same ID already exists
  for (JsonObject part : partListArray) {
    if (part["id"].as<int>() == id) {
      Serial.println("Part with this ID already exists");
      return;
    }
  }

  JsonObject newPart = partListArray.createNestedObject();
  newPart["id"] = id;
  newPart["name"] = name;
  newPart["std"] = std;
  newPart["unit"] = unit;

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();

  Serial.println("id: " + String(newPart["id"].as<int>()));
  Serial.println("name: " + newPart["name"].as<String>());
  Serial.println("std: " + String(newPart["std"].as<float>(), 2));
  Serial.println("unit: " + newPart["unit"].as<String>());
}

void deletePart(int id) {
  DynamicJsonDocument temp(1024);

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    Serial.println("Failed to read file, returning");
    return;
  }

  JsonArray partListArray = temp["partList"].as<JsonArray>();
  for (size_t i = 0; i < partListArray.size(); i++) {
    if (partListArray[i]["id"].as<int>() == id) {
      Serial.println("id: " + String(partListArray[i]["id"].as<int>()));
      Serial.println("name: " + partListArray[i]["name"].as<String>());
      Serial.println("std: " + String(partListArray[i]["std"].as<float>(), 2));
      Serial.println("unit: " + partListArray[i]["unit"].as<String>());
      partListArray.remove(i);
      break;
    }
  }

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();
}

void updatePart(int id, String name, float std, String unit) {
  DynamicJsonDocument temp(1024);

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    Serial.println("Failed to read file, using empty JSON");
  }

  JsonArray partListArray = temp["partList"].as<JsonArray>();

  // Update part if it exists
  for (JsonObject part : partListArray) {
    if (part["id"].as<int>() == id) {
      part["name"] = name;
      part["std"] = std;
      part["unit"] = unit;
      Serial.println("id: " + String(part["id"].as<int>()));
      Serial.println("name: " + part["name"].as<String>());
      Serial.println("std: " + String(part["std"].as<float>(), 2));
      Serial.println("unit: " + part["unit"].as<String>());
      break;
    }
  }

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();
}

/////////////////////////////////////////////////////////////////////
void initScale() {
  LoadCell.begin();

  // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  unsigned long stabilizingtime = 2000;

  //set this to false if you don't want tare to be performed in the next step
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    // user set calibration value (float), initial value 1.0 may be used for this sketch
    // float calibrationFactor = getCalibrationFactor
    // LoadCell.setCalFactor(calibrationFator);
    LoadCell.setCalFactor(1.0);
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  // calibrate(); //start calibration procedure
}

float getScale() {
  static boolean newDataReady = 0;

  //increase value to slow down serial print activity
  const int serialPrintInterval = 0;

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      weight = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(weight);
      newDataReady = 0;
      t = millis();
    }
  }

  return weight;

  // // receive command from serial terminal, send 't' to initiate tare operation:
  // if (Serial.available() > 0) {
  //   char inByte = Serial.read();
  //   if (inByte == 't') LoadCell.tareNoDelay();
  // }

  // // check if last tare operation is complete:
  // if (LoadCell.getTareStatus() == true) {
  //   Serial.println("Tare complete");
  // }
}

bool initCalibration() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell an a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    LoadCell.tareNoDelay();
    
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      _resume = true;
    }
  }

  Serial.println("Now, place your known mass on the loadcell.");
  
  return true;
}

float createCalibrationFactor(float knownWeight) {
  // refresh the dataset to be sure that the known mass is measured correct
  LoadCell.refreshDataSet();

  // get the new calibration value
  float newCalibrationFactor = LoadCell.getNewCalibration(knownWeight);

  Serial.print("New calibration factor has been set to: ");
  Serial.println(newCalibrationFactor);
  Serial.println("Use this as calibration factor (calFactor) in your project sketch.");

  Serial.println("Save this value to config.json ");

  StaticJsonDocument<200> temp;
  temp["calFactor"] = newCalibrationFactor;

  // Try to read the existing file
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading, creating new file");
    // Create a new file
    file = SPIFFS.open("/config.json", "w");
    if (!file) {
      Serial.println("Failed to create new file");
      return;
    }
    file.close();
  } else {
    DeserializationError error = deserializeJson(temp, file);
    file.close();
    if (error) {
      Serial.println("Failed to read file, using empty JSON");
    }
  }

  // Write the updated file
  file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();

  Serial.println("End calibration");
  Serial.println("***");

  return newCalibrationFactor;
}
/////////////////////////////////////////////////////////////////////

void handleGetPartList() {
  DynamicJsonDocument data = getPartList();

  DynamicJsonDocument responseDoc(2048);
  String response;
  responseDoc["data"] = data["partList"];
  responseDoc["message"] = "Part list retrieved successfully";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleGetPart() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("id")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing id parameter";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }

  int id = server.arg("id").toInt();
  JsonObject part = getPart(id);
  
  if (part.isNull()) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Part not found";
    responseDoc["status"] = 404;
    serializeJson(responseDoc, response);
    server.send(404, "application/json", response);
    return;
  }
  
  responseDoc["data"] = part;
  responseDoc["message"] = "Part retrieved successfully";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleCreatePart() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("name") || !server.hasArg("std") || !server.hasArg("unit")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing parameters";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }

  String name = server.arg("name");
  float std = server.arg("std").toFloat();
  String unit = server.arg("unit");
  createPart(name, std, unit);

  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part created";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleDeletePart() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("id")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing id parameter";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }

  int id = server.arg("id").toInt();
  deletePart(id);
  
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part deleted";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleUpdatePart() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("id") || !server.hasArg("name") || !server.hasArg("std") || !server.hasArg("unit")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing parameters";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }
  
  int id = server.arg("id").toInt();
  String name = server.arg("name");
  float std = server.arg("std").toFloat();
  String unit = server.arg("unit");
  updatePart(id, name, std, unit);
  
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part updated";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleGetScale() {
  float data = getScale();

  DynamicJsonDocument responseDoc(1024);
  String response;

  responseDoc["data"] = data;
  responseDoc["message"] = "Scale retrieved successfully";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleInitCalibration() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  initCalibration();

  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Initialize calibration success.";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleCreateCalibrationFactor() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("knownWeight")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing parameters";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }

  float knownWeight = server.arg("knownWeight").toFloat();
  createCalibrationFactor(knownWeight);

  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Calibration Factor created";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

boolean initWifi() {
  int trial = 0;
  while (WiFi.status() != WL_CONNECTED && trial < 10) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(1000);
    Serial.println("Connecting to WiFi...");
    trial++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP().toString());
  } else {
    if (WiFi.softAP(SSID, PASS)) {
      Serial.println("Failed to connect to WiFi, starting SoftAP");
      Serial.println("SoftAP IP address: ");
      Serial.println(WiFi.softAPIP().toString());
    } else {
      Serial.println("Failed to start SoftAP");
      return false;
    }
  }

  server.on("/parts", HTTP_GET, handleGetPartList);
  server.on("/parts/", HTTP_GET, handleGetPart);
  server.on("/parts", HTTP_POST, handleCreatePart);
  server.on("/parts/", HTTP_DELETE, handleDeletePart);
  server.on("/parts/", HTTP_PUT, handleUpdatePart);

  server.on("/scale", HTTP_GET, handleUpdatePart);

  server.on("/initCalibration", HTTP_GET, handleInitCalibration);
  
  server.on("/calibrationFactor", HTTP_POST, handleCreateCalibrationFactor);

  server.begin();

  return true;
}

void setup() {
  Serial.begin(9600);

  rtcInit();

  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

  delay(1000);

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  doc = getPartList();
  parts = doc["partList"].as<JsonArray>();

  initScale();
}

void loop() {
  if (CONN_STATUS) {
    server.handleClient();
  }
}