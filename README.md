# scale-bpi-leaf-s3

## Overview
`scale-bpi-leaf-s3` is a project developed by mekanii that utilizes an ESP32 microcontroller to create a smart scale with WiFi connectivity. The project includes a web server to handle various API endpoints for managing parts and scale operations. All necessary libraries are included in the `/lib` directory, so no additional installation is required.

## Hardware Used
- **ESP32 Development Board**: The main microcontroller used for this project.
- **HX711 Load Cell Amplifier**: Used to interface with the load cell.
- **Load Cell**: The sensor used to measure weight.
- **TFT Display**: For displaying information such as weight, status, and menu options.
- **Rotary Encoder**: For user input and navigation through the menu.
- **RTC Module**: For keeping track of time.
- **Push Button**: For additional user input.

## Directory Structure

```
scale-bpi-leaf-s3/
├── lib/
│   ├── WiFi/
│   ├── WebServer/
│   ├── ArduinoJson/
│   ├── TFT_eSPI/
│   ├── HX711_ADC/
│   ├── RTClib/
│   └── Encoder/
├── src/
│   ├── main.cpp
│   ├── WiFiManager.cpp
│   ├── WiFiManager.h
│   ├── ScaleManager.cpp
│   ├── ScaleManager.h
│   ├── DisplayManager.cpp
│   ├── DisplayManager.h
│   ├── PartManager.cpp
│   ├── PartManager.h
├── platformio.ini
└── README.md
```

## Parameters to be Set in Code

### WiFiManager.h

- **WIFI_SSID**: The SSID of the WiFi network to connect to.
- **WIFI_PASS**: The password of the WiFi network.

### ScaleManager.h

- **HX711_DT**: The data pin for the HX711.
- **HX711_SCK**: The clock pin for the HX711.
- **HYSTERESIS_STABLE_CHECK**: The hysteresis value for stable weight check.
- **HYSTERESIS_KG**: The hysteresis value for kilograms.
- **HYSTERESIS**: The hysteresis value for grams.
- **STABLE_READING_REQUIRED**: The number of stable readings required.

### DisplayManager.h

- **MONOFONTO28**: Font for the display.
- **MONOFONTO96**: Font for the display.

### PartManager.h

- **PART_LIST_FILE**: The file path for storing the part list.

## API Endpoints

### GET /parts

Retrieve the list of parts.

**Response:**
```json
{
  "partList": [
    { "id": 1, "name": "PART 01", "std": 100.00, "unit": "gr" },
    { "id": 2, "name": "PART 02", "std": 500.00, "unit": "gr" },
    { "id": 3, "name": "PART 03", "std": 1.25, "unit": "kg" }
  ]
}
```

### GET /parts/{id}

Retrieve a specific part by ID.

**Response:**
```json
{
  "id": 1,
  "name": "PART 01",
  "std": 100.00,
  "unit": "gr"
}
```

### POST /parts

Create a new part.

**Request:**
```json
{
  "name": "PART 04",
  "std": 200.00,
  "unit": "gr"
}
```

**Response:**
```json
{
  "id": 4,
  "name": "PART 04",
  "std": 200.00,
  "unit": "gr"
}
```

### DELETE /parts/{id}

Delete a part by ID.

**Response:**
```json
{
  "message": "Part deleted successfully"
}
```

### PUT /parts/{id}

Update a part by ID.

**Request:**
```json
{
  "name": "PART 01",
  "std": 150.00,
  "unit": "gr"
}
```

**Response:**
```json
{
  "id": 1,
  "name": "PART 01",
  "std": 150.00,
  "unit": "gr"
}
```

### POST /scale

Retrieve the current weight from the scale.

**Response:**
```json
{
  "weight": 123.45,
  "unit": "gr"
}
```

### GET /tare

Tare the scale.

**Response:**
```json
{
  "message": "Scale tared successfully"
}
```

### GET /initCalibration

Initialize the calibration process.

**Response:**
```json
{
  "message": "Calibration initialized"
}
```

### POST /calibrationFactor

Set the calibration factor.

**Request:**
```json
{
  "knownWeight": 500.00
}
```

**Response:**
```json
{
  "calFactor": 123.45
}
```

### GET /getStableWeight

Retrieve the stable weight from the scale.

**Response:**
```json
{
  "stableWeight": 123.45,
  "unit": "gr"
}
```

## Usage

1. Clone the repository.
2. Open the project in PlatformIO.
3. Ensure your hardware is connected correctly.
4. Build and upload the project to your ESP32 board.
5. Access the web server via the IP address displayed on the TFT screen.

## License

This project is licensed under the MIT License.