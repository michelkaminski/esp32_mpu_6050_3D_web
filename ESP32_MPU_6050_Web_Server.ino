#include "SD.h"



/*********
  Read Gyroskope data, save into SD card, and show in 3d based on project from Rui Santos: https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include "SPIFFS.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid = "YourWifiSSID";
const char* password = "YourWifiPassword";
const char* sdfile = "gyro_data.csv";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;  
unsigned long lastTimeAcc = 0;
unsigned long gyroDelay = 10;
unsigned long accelerometerDelay = 200;

// Create a sensor object
Adafruit_MPU6050 mpu;

sensors_event_t a, g, temp;

float gyroX, gyroY, gyroZ;
float accX, accY, accZ;

// Init MPU6050
void initMPU(){
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    Serial.print("Accelerometer range set to: ");
    switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("+-2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("+-4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("+-8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("+-16G");
      break;
    }
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.print("Gyro range set to: ");
    switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:
      Serial.println("+- 250 deg/s");
      break;
    case MPU6050_RANGE_500_DEG:
      Serial.println("+- 500 deg/s");
      break;
    case MPU6050_RANGE_1000_DEG:
      Serial.println("+- 1000 deg/s");
      break;
    case MPU6050_RANGE_2000_DEG:
      Serial.println("+- 2000 deg/s");
      break;
    }

    mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
    Serial.print("Filter bandwidth set to: ");
    switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260 Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184 Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94 Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44 Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21 Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10 Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5 Hz");
      break;
    }

    Serial.println("");
    delay(100);

    Serial.println("MPU6050 Found and Initialized!");
}

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.println(WiFi.localIP());
}

// Initalize SD, creating a csv file with header
void initSD() {
  if (!SD.begin()) { 
    Serial.println(F("Error initializing SD Card."));
    while (1);
  } else {
    if (!SD.exists(sdfile)) {
      File myFile = SD.open(sdfile, FILE_WRITE);
      if (myFile) {
        Serial.println(F("Writing csv header..."));
        myFile.println(F("gX,gY,gZ,aX,aY,aZ")); 
        myFile.close(); 
        Serial.println(F("CSV file created.")); 
      } else {     // In case we can't create the file
        Serial.println(F("Error creating csv file"));
        while (1);
      }
    }
  }
}

void writeToSD() {
  File myFile = SD.open(sdfile, FILE_APPEND); // Open csv file to write gyro data
  if (myFile) { 
    myFile.println(String(gyroX) + "," + String(gyroY) + "," + String(gyroZ) + "," + String(accX) + "," + String(accY) + "," + String(accZ)); 
    myFile.close();    
  } else {     
    Serial.println(F("Error opening csv file")); 
    while (1);
  }
}


String getGyroReadings(){

  
  
  readings["gyroX"] = String(g.gyro.x);
  readings["gyroY"] = String(g.gyro.y);
  readings["gyroZ"] = String(g.gyro.z);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

String getAccReadings() {
  readings["accX"] = String(a.acceleration.x);
  readings["accY"] = String(a.acceleration.y);
  readings["accZ"] = String(a.acceleration.z);
  String accString = JSON.stringify (readings);
  return accString;
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initSPIFFS();
  initMPU();
  initSD();

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX=0;
    gyroY=0;
    gyroZ=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetX", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetY", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroY=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetZ", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroZ=0;
    request->send(200, "text/plain", "OK");
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  mpu.getEvent(&a, &g, &temp);
  events.send(getGyroReadings().c_str(),"gyro_readings",millis());
  events.send(getAccReadings().c_str(),"accelerometer_readings",millis());
  writeToSD();  
  delay(500);
}
