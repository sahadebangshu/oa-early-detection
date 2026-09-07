/*
  OSTEOTRACK - ESP32 + MAX30102 + Firebase

  Hardware:
  - ESP32 DevKit
  - MAX30102

  Firebase structure:

  sensorData
    └── ESP32_01
        ├── heartRate
        ├── spo2
        ├── hpfDegradation
        ├── hypervascularization
        ├── presymptomaticIndex
        └── timestamp

  IMPORTANT:
  MAX30102 does NOT diagnose osteoarthritis.
  The OA-related values are experimental placeholders
  until validated formulas are provided.
*/

#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include <MAX30105.h>
#include "spo2_algorithm.h"

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// =====================================================
// WIFI SETTINGS
// =====================================================

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"


// =====================================================
// FIREBASE SETTINGS
// =====================================================

#define API_KEY         "AIzaSyAqV7ug-2GweTmsuVsw4-jfPZZN89Ixro4"

#define DATABASE_URL    "https://oa-early-detection-5c20b-default-rtdb.europe-west1.firebasedatabase.app"

#define FIREBASE_USER_EMAIL     "YOUR_FIREBASE_EMAIL"
#define FIREBASE_USER_PASSWORD  "YOUR_FIREBASE_PASSWORD"


// =====================================================
// DEVICE SETTINGS
// =====================================================

#define DEVICE_ID "ESP32_01"


// =====================================================
// I2C PINS
// =====================================================

#define SDA_PIN 21
#define SCL_PIN 22


// =====================================================
// SENSOR SETTINGS
// =====================================================

#define BUFFER_SIZE 100

#define SAMPLE_RATE 100

#define FINGER_THRESHOLD 50000


// =====================================================
// DESATURATION / ODI SETTINGS
// =====================================================

#define DESAT_DROP_PERCENT 3
#define DESAT_MIN_DURATION_S 10
#define ODI_ALERT_THRESHOLD 5


// =====================================================
// OBJECTS
// =====================================================

MAX30105 particleSensor;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// =====================================================
// SENSOR BUFFERS
// =====================================================

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];


// =====================================================
// SENSOR VALUES
// =====================================================

int32_t spo2;
int8_t validSPO2;

int32_t heartRate;
int8_t validHeartRate;


// =====================================================
// ODI VARIABLES
// =====================================================

int odiCount = 0;

float baselineSpO2 = 0;

unsigned long desatStartTime = 0;

bool desaturationActive = false;


// =====================================================
// TIMING
// =====================================================

unsigned long lastFirebaseSend = 0;

const unsigned long FIREBASE_INTERVAL = 10000;


// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void connectWiFi();

void initializeFirebase();

void initializeMAX30102();

void collectSensorData();

void processReading();

void sendDataToFirebase();

void resetODIWindow();

void printSensorData();


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("       OSTEOTRACK");
  Serial.println(" ESP32 + MAX30102 + Firebase");
  Serial.println("=================================");


  // ---------------------------------------------------
  // I2C
  // ---------------------------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("I2C initialized");


  // ---------------------------------------------------
  // MAX30102
  // ---------------------------------------------------

  initializeMAX30102();


  // ---------------------------------------------------
  // WIFI
  // ---------------------------------------------------

  connectWiFi();


  // ---------------------------------------------------
  // TIME
  // ---------------------------------------------------

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.println("Waiting for time synchronization...");

  delay(2000);


  // ---------------------------------------------------
  // FIREBASE
  // ---------------------------------------------------

  initializeFirebase();


  Serial.println();
  Serial.println("System initialization complete.");
  Serial.println("=================================");
}


// =====================================================
// LOOP
// =====================================================

void loop()
{

  collectSensorData();

  processReading();


  // ---------------------------------------------------
  // SEND DATA TO FIREBASE
  // ---------------------------------------------------

  if (millis() - lastFirebaseSend >= FIREBASE_INTERVAL)
  {
    sendDataToFirebase();

    lastFirebaseSend = millis();
  }


  // ---------------------------------------------------
  // RESET ODI EVERY HOUR
  // ---------------------------------------------------

  static unsigned long lastODIReset = 0;

  if (millis() - lastODIReset >= 3600000UL)
  {
    resetODIWindow();

    lastODIReset = millis();
  }


  delay(100);
}


// =====================================================
// INITIALIZE MAX30102
// =====================================================

void initializeMAX30102()
{

  Serial.println("Initializing MAX30102...");


  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {

    Serial.println("ERROR: MAX30102 not found!");

    while (1)
    {
      delay(1000);
    }
  }


  Serial.println("MAX30102 detected.");


  // Sensor configuration

  byte ledBrightness = 60;

  byte sampleAverage = 4;

  byte ledMode = 2;

  int sampleRate = 100;

  int pulseWidth = 411;

  int adcRange = 4096;


  particleSensor.setup(
    ledBrightness,
    sampleAverage,
    ledMode,
    sampleRate,
    pulseWidth,
    adcRange
  );


  particleSensor.setPulseAmplitudeRed(0x3F);

  particleSensor.setPulseAmplitudeIR(0x3F);

  particleSensor.setPulseAmplitudeGreen(0);


  Serial.println("MAX30102 configuration complete.");
}


// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi()
{

  Serial.println();

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);


  int attempts = 0;


  while (WiFi.status() != WL_CONNECTED)
  {

    delay(500);

    Serial.print(".");

    attempts++;


    if (attempts > 40)
    {

      Serial.println();

      Serial.println("Wi-Fi connection failed.");

      return;
    }
  }


  Serial.println();

  Serial.println("Wi-Fi connected.");

  Serial.print("IP address: ");

  Serial.println(WiFi.localIP());
}


// =====================================================
// INITIALIZE FIREBASE
// =====================================================

void initializeFirebase()
{

  Serial.println("Initializing Firebase...");


  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;


  auth.user.email = FIREBASE_USER_EMAIL;

  auth.user.password = FIREBASE_USER_PASSWORD;


  config.token_status_callback = tokenStatusCallback;


  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);


  Serial.println("Firebase initialized.");
}


// =====================================================
// COLLECT SENSOR DATA
// =====================================================

void collectSensorData()
{

  Serial.println();

  Serial.println("Collecting MAX30102 samples...");


  for (int i = 0; i < BUFFER_SIZE; i++)
  {

    while (particleSensor.available() == false)
    {
      particleSensor.check();
    }


    redBuffer[i] = particleSensor.getRed();

    irBuffer[i] = particleSensor.getIR();


    particleSensor.nextSample();


    delay(10);
  }


  Serial.println("100 samples collected.");
}


// =====================================================
// PROCESS SENSOR READING
// =====================================================

void processReading()
{

  uint32_t averageIR = 0;


  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    averageIR += irBuffer[i];
  }


  averageIR /= BUFFER_SIZE;


  // ---------------------------------------------------
  // FINGER DETECTION
  // ---------------------------------------------------

  if (averageIR < FINGER_THRESHOLD)
  {

    Serial.println("No finger detected.");

    heartRate = -1;

    spo2 = -1;

    validHeartRate = 0;

    validSPO2 = 0;

    return;
  }


  // ---------------------------------------------------
  // CALCULATE HEART RATE AND SPO2
  // ---------------------------------------------------

  maxim_heart_rate_and_oxygen_saturation(
    irBuffer,
    BUFFER_SIZE,
    redBuffer,
    &spo2,
    &validSPO2,
    &heartRate,
    &validHeartRate
  );


  // ---------------------------------------------------
  // PRINT VALUES
  // ---------------------------------------------------

  printSensorData();


  // ---------------------------------------------------
  // DESATURATION LOGIC
  // ---------------------------------------------------

  if (validSPO2 && spo2 > 0)
  {

    if (baselineSpO2 == 0)
    {
      baselineSpO2 = spo2;
    }


    float dropPercent =
      ((baselineSpO2 - spo2) / baselineSpO2) * 100.0;


    if (dropPercent >= DESAT_DROP_PERCENT)
    {

      if (!desaturationActive)
      {

        desaturationActive = true;

        desatStartTime = millis();
      }


      if (
        millis() - desatStartTime >=
        DESAT_MIN_DURATION_S * 1000UL
      )
      {

        odiCount++;

        desaturationActive = false;

        Serial.println("Desaturation event detected.");

        Serial.print("ODI count: ");

        Serial.println(odiCount);
      }
    }
    else
    {

      desaturationActive = false;
    }
  }
}


// =====================================================
// PRINT SENSOR DATA
// =====================================================

void printSensorData()
{

  Serial.println();
  Serial.println("--------- SENSOR DATA ---------");


  Serial.print("Heart Rate: ");

  if (validHeartRate)
  {
    Serial.print(heartRate);
    Serial.println(" BPM");
  }
  else
  {
    Serial.println("Invalid");
  }


  Serial.print("SpO2: ");

  if (validSPO2)
  {
    Serial.print(spo2);
    Serial.println(" %");
  }
  else
  {
    Serial.println("Invalid");
  }


  Serial.print("ODI: ");

  Serial.println(odiCount);


  Serial.println("-------------------------------");
}


// =====================================================
// SEND DATA TO FIREBASE
// =====================================================

void sendDataToFirebase()
{

  if (WiFi.status() != WL_CONNECTED)
  {

    Serial.println("Wi-Fi not connected.");

    return;
  }


  if (!Firebase.ready())
  {

    Serial.println("Firebase not ready.");

    return;
  }


  String basePath =
    "/sensorData/" + String(DEVICE_ID);


  Serial.println();

  Serial.println("Sending data to Firebase...");


  // ---------------------------------------------------
  // HEART RATE
  // ---------------------------------------------------

  if (validHeartRate)
  {

    if (
      Firebase.RTDB.setInt(
        &fbdo,
        basePath + "/heartRate",
        heartRate
      )
    )
    {
      Serial.println("Heart rate uploaded.");
    }
    else
    {
      Serial.print("Heart rate upload failed: ");

      Serial.println(fbdo.errorReason());
    }
  }


  // ---------------------------------------------------
  // SPO2
  // ---------------------------------------------------

  if (validSPO2)
  {

    if (
      Firebase.RTDB.setInt(
        &fbdo,
        basePath + "/spo2",
        spo2
      )
    )
    {
      Serial.println("SpO2 uploaded.");
    }
    else
    {
      Serial.print("SpO2 upload failed: ");

      Serial.println(fbdo.errorReason());
    }
  }


  // ---------------------------------------------------
  // OA EXPERIMENTAL VALUES
  // ---------------------------------------------------
  //
  // These values are NOT calculated because validated
  // formulas have not been provided.
  //
  // -1 = unavailable / not calculated
  //

  Firebase.RTDB.setInt(
    &fbdo,
    basePath + "/hpfDegradation",
    -1
  );


  Firebase.RTDB.setInt(
    &fbdo,
    basePath + "/hypervascularization",
    -1
  );


  Firebase.RTDB.setInt(
    &fbdo,
    basePath + "/presymptomaticIndex",
    -1
  );


  // ---------------------------------------------------
  // TIMESTAMP
  // ---------------------------------------------------

  time_t now;

  time(&now);


  if (
    Firebase.RTDB.setInt(
      &fbdo,
      basePath + "/timestamp",
      (long)now
    )
  )
  {
    Serial.println("Timestamp uploaded.");
  }
  else
  {
    Serial.print("Timestamp upload failed: ");

    Serial.println(fbdo.errorReason());
  }


  Serial.println("Firebase upload complete.");
}


// =====================================================
// RESET ODI WINDOW
// =====================================================

void resetODIWindow()
{

  Serial.println();

  Serial.println("Resetting hourly ODI window.");


  odiCount = 0;

  baselineSpO2 = 0;

  desaturationActive = false;

  desatStartTime = 0;
}


// =====================================================
// END
// =====================================================