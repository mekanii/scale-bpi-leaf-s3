#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "ArduinoJson.h"
#include "FS.h"
#include "SPIFFS.h"
#include <HX711_ADC.h>

#define HX711_DT 18
#define HX711_SCK 19

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
  serializeJson(part, response);
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

void setup() {
  Serial.begin(9600);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

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
    }
  }

  server.on("/parts", HTTP_GET, handleGetPartList);
  server.on("/parts/", HTTP_GET, handleGetPart);
  server.on("/parts", HTTP_POST, handleCreatePart);
  server.on("/parts/", HTTP_DELETE, handleDeletePart);
  server.on("/parts/", HTTP_PUT, handleUpdatePart);
  server.begin();

  LoadCell.begin();

  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 696.0; // uncomment this if you want to set the calibration value in the sketch

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }
}

void loop() {
  server.handleClient();

  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(i);
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }
}