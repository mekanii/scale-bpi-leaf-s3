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
#include <esp32-hal-cpu.h>
#include "soc/rtc.h"

#define HX711_DT 35
#define HX711_SCK 45

HX711_ADC LoadCell(HX711_DT, HX711_SCK);

#define HYSTERESIS 5.0f
#define HYSTERESIS_KG 0.01f
#define HYSTERESIS_STABLE_CHECK 1.0f
#define STABLE_READING_REQUIRED 32

unsigned long t = 0;
float weight = 0.00;
float lastWeight = 0.0f;
int stableReadingsCount = 0;
float stableWeight = 0.0f;
boolean doCheckStableState = false;
int CHECK_STATUS = 0;

#define MONOFONTO20 monofonto20
#define MONOFONTO28 monofonto28
#define MONOFONTO96 monofonto96

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define FORMAT_SPIFFS_IF_FAILED true

// Define pins for the rotary encoder
#define ENCODER_PIN_A 9
#define ENCODER_PIN_B 14
#define ENCODER_BUTTON_PIN 46

#define WIFI_SSID "PemudaTanggapBerbagi_Lt1"
#define WIFI_PASS "lantaisatutanpaspasi"
#define SSID "BPI-LEAF-S3_01"
#define PASS "12345678"

const char* ssid = "BPI-LEAF-S3_01";
const char* password = "12345678";

WebServer server(80);

String DEVICE_IP_ADDRESS = "";
boolean CONN_STATUS = false;

DynamicJsonDocument doc(1024);
JsonArray parts;

String partName = "";
float partStd = 0;
String partUnit = "";

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

const char* optsMenu[2] = {
  "START",
  "SETTINGS"
};

const char* optsSettings[3] = {
  "CONNECT TO WIFI",
  "WIFI STATION MODE",
  "SERIAL PORT"
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

// BEGIN: PART MANAGER ///////////////////////////////////////////////////////////////////
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

void createPartList() {
  // Define the JSON content
  const char* jsonContent = R"rawliteral(
  {
    "partList": [
      { "id": 1,"name": "PART 01", "std": 100.00, "unit": "gr" },
      { "id": 2,"name": "PART 02", "std": 500.00, "unit": "gr" },
      { "id": 3,"name": "PART 03", "std": 1.25, "unit": "kg" }
    ]
  }
  )rawliteral";

  // Open file for writing
  File file = SPIFFS.open("/partList.json", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open partList.json for writing");
    return;
  }

  // Write the JSON content to the file
  if (file.print(jsonContent)) {
    Serial.println("partList.json created successfully");
  } else {
    Serial.println("Failed to write to partList.json");
  }

  // Close the file
  file.close();
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

void deletePartList() {
  if (SPIFFS.exists("/partList.json")) {
    if (SPIFFS.remove("/partList.json")) {
      Serial.println("Deleted partList.json successfully");
    } else {
      Serial.println("Failed to delete partList.json");
    }
  } else {
    Serial.println("partList.json does not exist");
  }
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
// END: PART MANAGER ///////////////////////////////////////////////////////////////////

// BEGIN: SCALE MANAGER ///////////////////////////////////////////////////////////////////
void tareScale() {
  // LoadCell.tareNoDelay();
  // while (!LoadCell.getTareStatus()) {
  //   LoadCell.update();
  //   delay(100);
  //   Serial.println("Taring...");
  // }
  LoadCell.tare();
  Serial.println("Tare complete");
}

float getCalibrationFactor() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return 1.0;  // Return the default calibration factor if file reading fails
  }

  DynamicJsonDocument temp(1024);
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    Serial.println("Failed to read file, using default calibration factor");
    return 1.0;  // Return the default calibration factor if file reading fails
  }

  // Check if the calFactor key exists and is a valid float
  if (!temp.containsKey("calFactor") || !temp["calFactor"].is<float>()) {
    Serial.println("calFactor key is missing or not a valid float, using default calibration factor");
    return 1.0;  // Return the default calibration factor if the key is missing or invalid
  }

  float calibrationFactor = temp["calFactor"].as<float>();
  Serial.println("Calibration factor read from config: " + String(calibrationFactor));

  return calibrationFactor;
}

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
    float calibrationFactor = getCalibrationFactor();
    Serial.println(calibrationFactor);
    if (calibrationFactor > 0) {
      LoadCell.setCalFactor(calibrationFactor);
    } else {
      LoadCell.setCalFactor(1.0);
    }
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
}

bool checkStableState(float wt, String unit) {
  float hys = 0.0;
  if (doCheckStableState) {
    if (unit == "kg") {
      hys = HYSTERESIS_STABLE_CHECK / 1000.0;
    } else {
      hys = HYSTERESIS_STABLE_CHECK;
    }

    if (wt >= wt - hys && wt <= wt + hys && abs(wt - lastWeight) <= hys) {
    // if (abs(wt - lastWeight) < HYSTERESIS_STABLE_CHECK) {
      stableReadingsCount++;
    } else {
      stableReadingsCount = 0;
    }
    lastWeight = wt;
  }
  doCheckStableState = false;
    
  return (stableReadingsCount >= STABLE_READING_REQUIRED);
}

float getScale() {
  static boolean newDataReady = 0;

  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {
    if (millis() > t) {
      weight = LoadCell.getData();
      // Serial.print("Load_cell output val: ");
      // Serial.println(weight);
      newDataReady = 0;
      t = millis();
      doCheckStableState = true;
    }
  }

  return weight;
}

DynamicJsonDocument getScale2(){
  float wt = getScale();
  if (partUnit == "kg") {
    wt = wt / 1000.0;
  }

  if (checkStableState(wt, partUnit)) {
    float hys = 0.0;
    if (partUnit == "kg") {
      hys = HYSTERESIS_KG;
    } else {
      hys = HYSTERESIS;
    }

    if (partUnit == "kg" && wt <= 0.01f) {
      CHECK_STATUS = 0;
      stableWeight = 0.0;
    } else if (partUnit == "gr" && wt <= 2.0f) {
      CHECK_STATUS = 0;
      stableWeight = 0.0;
    } else if (wt >= partStd - hys && wt <= partStd + hys && CHECK_STATUS == 0) {
      CHECK_STATUS = 1;
      stableWeight = wt;
    } else if (CHECK_STATUS == 0) {
      CHECK_STATUS = 2;
      stableWeight = wt;
    }
  } else if (stableWeight != 0.0 && (wt <= stableWeight * 0.9f || wt >= stableWeight * 1.1f)) {
    CHECK_STATUS = 0;
    stableWeight = 0.0;
  }

  DynamicJsonDocument responseDoc(1024);
  responseDoc["weight"] = wt;
  responseDoc["check"] = CHECK_STATUS;
  return responseDoc;
}

bool initCalibration() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell an a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");

  tareScale();

  Serial.println("Now, place your known mass on the loadcell.");
  
  return true;
}

float createCalibrationFactor(float knownWeight) {
  // Refresh the dataset to be sure that the known mass is measured correctly
  LoadCell.refreshDataSet();
  Serial.println(knownWeight);

  // Get the new calibration value
  float newCalibrationFactor = LoadCell.getNewCalibration(knownWeight);

  // Check if the new calibration factor is valid
  if (isnan(newCalibrationFactor)) {
    Serial.println("Failed to get new calibration factor, result is NaN");
    return NAN;
  }

  Serial.print("New calibration factor has been set to: ");
  Serial.println(newCalibrationFactor);
  Serial.println("Use this as calibration factor (calFactor) in your project sketch.");

  Serial.println("Save this value to config.json");

  // Create a new JSON document to store the calibration factor
  StaticJsonDocument<200> temp;
  temp["calFactor"] = newCalibrationFactor;

  // Open the file for writing
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return newCalibrationFactor;  // Return the new calibration factor even if file writing fails
  }

  // Write the updated JSON to the file
  serializeJson(temp, file);
  file.close();

  Serial.println("End calibration");
  Serial.println("***");

  return newCalibrationFactor;
}

float getStableWeight() {
  float data = 0.0;
  boolean stable = false;
  while (!stable) {
    float newWeight = getScale();
    if (checkStableState(newWeight, "gr")) {
      data = newWeight;
      stable = true;
    }
  }

  return data;
}
// END: SCALE MANAGER ///////////////////////////////////////////////////////////////////

// BEGIN: API HANDLER ///////////////////////////////////////////////////////////////////
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

void handleTare() {
  tareScale();

  DynamicJsonDocument responseDoc(1024);
  String response;

  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Tare success";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleGetScale() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  if (!server.hasArg("std") || !server.hasArg("unit")) {
    responseDoc["data"] = nullptr;
    responseDoc["message"] = "Missing parameters";
    responseDoc["status"] = 400;
    serializeJson(responseDoc, response);
    server.send(400, "application/json", response);
    return;
  }
  
  partStd = server.arg("std").toFloat();
  partUnit = server.arg("unit");

  DynamicJsonDocument data = getScale2();

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
  float calibrationFactor = createCalibrationFactor(knownWeight);

  Serial.println(calibrationFactor);

  responseDoc["data"] = calibrationFactor;
  responseDoc["message"] = "Calibration Factor created";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}

void handleGetStableWeight() {
  DynamicJsonDocument responseDoc(1024);
  String response;

  float data = getStableWeight();

  responseDoc["data"] = data;
  responseDoc["message"] = "Scale retrieved successfully";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
}
// END: API HANDLER ///////////////////////////////////////////////////////////////////

// BEGIN: SERVER MANAGER //////////////////////////////////////////////////////////////
void initServer() {
  server.on("/parts", HTTP_GET, handleGetPartList);
  server.on("/parts/", HTTP_GET, handleGetPart);
  server.on("/parts", HTTP_POST, handleCreatePart);
  server.on("/parts/", HTTP_DELETE, handleDeletePart);
  server.on("/parts/", HTTP_PUT, handleUpdatePart);

  server.on("/scale", HTTP_POST, handleGetScale);
  
  server.on("/tare", HTTP_GET, handleTare);

  server.on("/initCalibration", HTTP_GET, handleInitCalibration);
  
  server.on("/calibrationFactor", HTTP_POST, handleCreateCalibrationFactor);

  server.on("/getStableWeight", HTTP_GET, handleGetStableWeight);

  server.begin();

  CONN_STATUS = true;
}

void terminateServer() {
  server.close();

  CONN_STATUS = false;
}

boolean connectWiFi() {
  int trial = 0;
  while (WiFi.status() != WL_CONNECTED && trial < 10) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(1000);
    Serial.println("Connecting to WiFi...");
    trial++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DEVICE_IP_ADDRESS = WiFi.localIP().toString();
    Serial.println("Connected to WiFi");
    Serial.println("IP address: ");
    Serial.println(DEVICE_IP_ADDRESS);
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }

  initServer();

  return true;
}

boolean wiFiStationMode() {
  if (WiFi.softAP(SSID, PASS)) {
    DEVICE_IP_ADDRESS = WiFi.softAPIP().toString();
    Serial.println("Starting SoftAP");
    Serial.println("SoftAP IP address: ");
    Serial.println(DEVICE_IP_ADDRESS);
  } else {
    Serial.println("Failed to start SoftAP");
    return false;
  }

  initServer();

  return true;
}
// END: SERVER MANAGER ///////////////////////////////////////////////////////////////////

// BEGIN: DISPLAY MANAGER //////////////////////////////////////////////////////////////
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
  spr.printToSprite(partUnit);
  spr.deleteSprite();
  spr.unloadFont();
}

void displayMain(float sc) {
  spr.loadFont(MONOFONTO96);
  spr.createSprite(372, 96);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.fillSprite(TFT_BLACK);

  if (partUnit == "kg") {
    spr.drawFloat(sc, 2, 360, 0);
  } else if (partUnit == "gr") {
    spr.drawNumber((int)sc, 360, 0);
  }

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

void displaySettings() {
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
  for (int i = 0; i < ARRAY_SIZE(optsSettings); i++) {
    // Serial.println(partList[i]["name"].as<String>());
    tft.setCursor(200, 10 + ((i + 1) * spr.fontHeight()));
    if (i == selectorIndex) {
      spr.setTextColor(TFT_ORANGE, TFT_BLACK);
      spr.printToSprite(String(" > ") + optsSettings[i]);
    } else {
      spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      spr.printToSprite(String("    ") + optsSettings[i]);
    }
  }
  spr.unloadFont();
}

void displayIPAddress() {
  spr.loadFont(MONOFONTO28);
  spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(10, 10);
  spr.printToSprite(DEVICE_IP_ADDRESS);
  spr.unloadFont();
}

boolean displayWiFi(int optIndex) {
  spr.loadFont(MONOFONTO28);
  spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  
  switch (optIndex) {
    case 0:
      tft.setCursor(10, 10 + (1 * spr.fontHeight()));
      spr.printToSprite("SSID: " + String(WIFI_SSID));
  
      tft.setCursor(10, 10 + (2 * spr.fontHeight()));
      spr.printToSprite("PASS: " + String(WIFI_PASS));

      break;
    case 1:
      tft.setCursor(10, 10 + (1 * spr.fontHeight()));
      spr.printToSprite("SSID: " + String(SSID));
  
      tft.setCursor(10, 10 + (2 * spr.fontHeight()));
      spr.printToSprite("PASS: " + String(PASS));

      break;
  }

  spr.unloadFont();

  return true;
}

void displayFailedWiFi() {
  spr.loadFont(MONOFONTO28);
  spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(10, 10);
  spr.printToSprite("FAILED TO INITIALIZE WIFI");
  spr.unloadFont();
}
// END: DISPLAY MANAGER //////////////////////////////////////////////////////////////////

// BEGIN: ENCODER MANAGER ////////////////////////////////////////////////////////////////
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
              case 0:   // main
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
            switch(page3Name) {
              default:  // settings selection
                if (constrainer(&newPosition, -1, ARRAY_SIZE(optsSettings))) {
                  displaySettings();
                }
                break;
              case 0:   // Connect To WiFi
                // displayMain(getScale());
                break;
              case 1:   // WiFi Station Mode
                // if (constrainer(&newPosition, 0, ARRAY_SIZE(optsOption))) {
                //   displayOption();
                // }
                break;
              case 2:   // Serial Port
                // if (constrainer(&newPosition, 0, ARRAY_SIZE(optsExitDialog))) {
                //   displayExitDialog();
                // }
                break;
            }
            break;
        }
        break;
    }
  }
  else if (page1Name == 0 && page2Name == 0 && page3Name == 0) {
    float wt = getScale();
    if (partUnit == "kg") {
      wt = wt / 1000.0;
    }
    
    displayMain(weight);

    if (checkStableState(wt, partUnit)) {
      float hys = 0.0;
      if (partUnit == "kg") {
        hys = HYSTERESIS_KG;
      } else {
        hys = HYSTERESIS;
      }

      if (partUnit == "kg" && wt <= 0.01f) {
        CHECK_STATUS = 0;
        stableWeight = 0.0;
      } else if (partUnit == "gr" && wt <= 2.0f) {
        CHECK_STATUS = 0;
        stableWeight = 0.0;
      } else if (wt >= partStd - hys && wt <= partStd + hys && CHECK_STATUS == 0) {
        CHECK_STATUS = 1;
        stableWeight = wt;
      } else if (CHECK_STATUS == 0) {
        CHECK_STATUS = 2;
        stableWeight = wt;
      }
      displayCheckStatus();
    } else if (stableWeight != 0.0 && (wt <= stableWeight * 0.9f || wt >= stableWeight * 1.1f)) {
      CHECK_STATUS = 0;
      stableWeight = 0.0;
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
                dispalyMenuDisabled();
                selectorIndex = 0;
                displayPart();
                page2Name = 0;
                break;
              case 1:
                dispalyMenuDisabled();
                selectorIndex = 0;
                displaySettings();
                page2Name = 1;
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
                  partUnit = parts[selectorIndex]["unit"].as<String>();
                  selectorIndex = 0;
                  
                  // initScale();
                  tareScale();

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
                    {
                      tareScale();

                      selectorIndex = 0;
                      tft.fillScreen(TFT_BLACK);
                      displayMainFrame();
                      displayMain(getScale());
                      page3Name = 0;
                    }
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
            switch(page3Name) {
              default:  // settings selection
                tft.fillScreen(TFT_BLACK);
                switch (selectorIndex) {
                  case -1:
                    selectorIndex = 1;
                    displayMenu();
                    page2Name = -1;
                    break;
                  case 0:
                    if (connectWiFi()) {
                      selectorIndex = 0;
                      displayIPAddress();
                      displayWiFi(0);
                      initServer();
                      page3Name = 0;
                    } else {
                      displayFailedWiFi();
                      page3Name = 1;
                    }
                    break;
                  case 1:
                    if (wiFiStationMode()) {
                      selectorIndex = 0;
                      displayIPAddress();
                      displayWiFi(1);
                      initServer();
                      page3Name = 0;
                    } else {
                      displayFailedWiFi();
                      page3Name = 1;
                    }
                    break;
                  case 2:
                    break;
                }
                break;
              case 0:
                break;
              case 1: // display failed WiFi
                tft.fillScreen(TFT_BLACK);
                selectorIndex = 1;
                dispalyMenuDisabled();
                selectorIndex = 0;
                displaySettings();
                page3Name = -1;

                terminateServer();
                break;
            }
            break;
        }
        break;
    }
    selectorIndex = 0;
    lastSelectorIndex = -1;
    delay(500);
  }
}
// END: ENCODER MANAGER //////////////////////////////////////////////////////////////////

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(9600);

  rtcInit();

  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

  delay(1000);

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // deletePartList();
  // createPartList();

  doc = getPartList();
  parts = doc["partList"].as<JsonArray>();

  initScale();

  tft.init();
  tft.setRotation(1);
  spr.setColorDepth(16);

  page1Name = 0;
  tft.fillScreen(TFT_BLACK);
  displayMenu();
}

void loop() {
  rotarySelector();
  rotaryButton();
  if (CONN_STATUS) {
    server.handleClient();
  }
}