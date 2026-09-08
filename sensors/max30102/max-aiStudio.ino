/*
  ESP32 DevKitV1 + MAX30102 — OA Knee-Patch Prototype
  ----------------------------------------------------

  ORIGINAL SENSOR / CALCULATION LOGIC PRESERVED.

  Added:
    - WiFi connection
    - Firebase Authentication
    - Firebase Realtime Database
    - Automatic WiFi reconnect
    - Real-time Firebase upload
    - Single Firebase update packet
    - Device contact status
    - Device timestamp

  Firebase structure:

  devices
    └── ESP32_01
         └── live
              ├── hpfDegradation
              ├── vasodilation
              ├── liveIndex
              ├── bpm
              ├── contact
              └── timestamp

  The webpage can listen to:

      /devices/ESP32_01

  Required libraries:

    1. SparkFun MAX3010x Pulse and Proximity Sensor Library

    2. Firebase Arduino Client Library for ESP8266 and ESP32

  Open Serial Monitor:
    115200 baud
*/


// ============================================================
// LIBRARIES
// ============================================================

#include <WiFi.h>
#include <Wire.h>

#include "MAX30105.h"
#include "heartRate.h"

#include <Firebase_ESP_Client.h>


// ============================================================
// WIFI SETTINGS
// ============================================================

// PUT YOUR WIFI DETAILS HERE

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"


// ============================================================
// FIREBASE SETTINGS
// ============================================================

// Your Firebase Web API Key

#define API_KEY \
"AIzaSyAqV7ug-2GweTmsuVsw4-jfPZZN89Ixro4"


// Your Firebase Realtime Database URL

#define DATABASE_URL \
"https://oa-early-detection-5c20b-default-rtdb.europe-west1.firebasedatabase.app/"


// ============================================================
// FIREBASE AUTHENTICATION
// ============================================================

// Firebase Authentication EMAIL

#define USER_EMAIL \
"YOUR_FIREBASE_EMAIL"


// Firebase Authentication PASSWORD

#define USER_PASSWORD \
"YOUR_FIREBASE_PASSWORD"


// ============================================================
// DEVICE ID
// ============================================================

#define DEVICE_ID "ESP32_01"


// ============================================================
// FIREBASE OBJECTS
// ============================================================

FirebaseData fbdo;

FirebaseAuth auth;

FirebaseConfig config;


// ============================================================
// SENSOR
// ============================================================

MAX30105 particleSensor;


// ============================================================
// HEART-RATE DETECTION
// ============================================================

const byte RATE_SIZE = 4;

byte rates[RATE_SIZE];

byte rateSpot = 0;

long lastBeat = 0;

float beatsPerMinute;

int beatAvg = 0;


// ============================================================
// WINDOWED WAVEFORM ANALYSIS
// ============================================================

const int WINDOW_SIZE = 50;

long irWindow[WINDOW_SIZE];

int windowPos = 0;

bool windowFull = false;


float vasodilationIndex = 0;


// Maps typical PI% (~0-20) onto 0-100

float PERF_INDEX_SCALE = 5.0;


float hpfDegradationIndex = 0;


// ============================================================
// COMBINED HEADLINE INDEX
// ============================================================

float WEIGHT_HPF = 0.5;

float WEIGHT_VASO = 0.5;

float preSymptomaticIndex = 0;


// ============================================================
// CONTACT GATING
// ============================================================

const long CONTACT_THRESHOLD = 50000;

bool contactDetected = false;


// ============================================================
// FIREBASE UPLOAD SETTINGS
// ============================================================

// Upload once every 2 seconds

const unsigned long FIREBASE_UPLOAD_INTERVAL = 2000;

unsigned long lastFirebaseUpload = 0;


// ============================================================
// WIFI RECONNECT SETTINGS
// ============================================================

unsigned long lastWiFiCheck = 0;

const unsigned long WIFI_CHECK_INTERVAL = 5000;


// ============================================================
// SENSOR METRICS
// ============================================================

void updateWindowMetrics(long irValue) {

  irWindow[windowPos] = irValue;

  windowPos++;


  if (windowPos >= WINDOW_SIZE) {

    windowPos = 0;

    windowFull = true;


    long irMin = irWindow[0];

    long irMax = irWindow[0];

    long irSum = 0;


    int minIdx = 0;

    int maxIdx = 0;


    for (int i = 0; i < WINDOW_SIZE; i++) {

      irSum += irWindow[i];


      if (irWindow[i] < irMin) {

        irMin = irWindow[i];

        minIdx = i;

      }


      if (irWindow[i] > irMax) {

        irMax = irWindow[i];

        maxIdx = i;

      }

    }


    float irDC =
      irSum / (float)WINDOW_SIZE;


    float irAC =
      irMax - irMin;


    // ========================================================
    // CONTACT DETECTION
    // ========================================================

    contactDetected =
      (irDC >= CONTACT_THRESHOLD);


    // ========================================================
    // CALCULATIONS
    // ========================================================

    if (contactDetected) {


      // ------------------------------------------------------
      // VASODILATION / PERFUSION INDEX
      // ------------------------------------------------------

      float perfusionIndexPct =
        (irAC / irDC) * 100.0;


      vasodilationIndex =
        constrain(
          perfusionIndexPct * PERF_INDEX_SCALE,
          0,
          100
        );


      // ------------------------------------------------------
      // HPF DEGRADATION
      // ------------------------------------------------------

      int sampleGap =
        maxIdx - minIdx;


      if (sampleGap < 0) {

        sampleGap += WINDOW_SIZE;

      }


      float riseTimeFraction =
        sampleGap / (float)WINDOW_SIZE;


      /*
        EXPERIMENTAL PROXY.

        This is NOT a clinically validated
        HPF degradation measurement.
      */

      hpfDegradationIndex =
        constrain(
          (1.0 - riseTimeFraction) * 100.0,
          0,
          100
        );


      // ------------------------------------------------------
      // OA PRE-SYMPTOMATIC INDEX
      // ------------------------------------------------------

      preSymptomaticIndex =
        constrain(
          WEIGHT_HPF * hpfDegradationIndex +
          WEIGHT_VASO * vasodilationIndex,
          0,
          100
        );

    }

  }

}


// ============================================================
// WIFI CONNECTION
// ============================================================

void connectWiFi() {

  Serial.println();

  Serial.println("Connecting to WiFi...");

  Serial.print("SSID: ");

  Serial.println(WIFI_SSID);


  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  int attempts = 0;


  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 40
  ) {

    delay(500);

    Serial.print(".");

    attempts++;

  }


  Serial.println();


  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");

    Serial.print("ESP32 IP: ");

    Serial.println(
      WiFi.localIP()
    );

    Serial.print("Signal RSSI: ");

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(" dBm");

  }

  else {

    Serial.println(
      "WiFi connection failed."
    );

  }

}


// ============================================================
// CHECK WIFI
// ============================================================

void checkWiFi() {

  if (
    millis() - lastWiFiCheck <
    WIFI_CHECK_INTERVAL
  ) {

    return;

  }


  lastWiFiCheck =
    millis();


  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println();

    Serial.println(
      "WiFi disconnected. Reconnecting..."
    );


    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );


    unsigned long startAttempt =
      millis();


    while (
      WiFi.status() != WL_CONNECTED &&
      millis() - startAttempt < 10000
    ) {

      delay(500);

      Serial.print(".");

    }


    Serial.println();


    if (
      WiFi.status() == WL_CONNECTED
    ) {

      Serial.println(
        "WiFi reconnected!"
      );

      Serial.print(
        "IP: "
      );

      Serial.println(
        WiFi.localIP()
      );

    }

    else {

      Serial.println(
        "WiFi reconnect failed."
      );

    }

  }

}


// ============================================================
// FIREBASE INITIALIZATION
// ============================================================

void initializeFirebase() {

  Serial.println();

  Serial.println(
    "Initializing Firebase..."
  );


  // ----------------------------------------------------------
  // API KEY
  // ----------------------------------------------------------

  config.api_key =
    API_KEY;


  // ----------------------------------------------------------
  // DATABASE URL
  // ----------------------------------------------------------

  config.database_url =
    DATABASE_URL;


  // ----------------------------------------------------------
  // USER LOGIN
  // ----------------------------------------------------------

  auth.user.email =
    USER_EMAIL;


  auth.user.password =
    USER_PASSWORD;


  // ----------------------------------------------------------
  // START FIREBASE
  // ----------------------------------------------------------

  Firebase.begin(
    &config,
    &auth
  );


  Firebase.reconnectWiFi(
    true
  );


  Serial.println(
    "Firebase initialization started."
  );

}


// ============================================================
// FIREBASE UPLOAD
// ============================================================

void uploadFirebaseData(
  long irValue
) {


  // ----------------------------------------------------------
  // CHECK WIFI
  // ----------------------------------------------------------

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "Firebase upload skipped: WiFi disconnected."
    );

    return;

  }


  // ----------------------------------------------------------
  // CHECK FIREBASE
  // ----------------------------------------------------------

  if (
    !Firebase.ready()
  ) {

    Serial.println(
      "Firebase not ready yet."
    );

    return;

  }


  // ----------------------------------------------------------
  // FIREBASE PATH
  // ----------------------------------------------------------

  String devicePath =
    "/devices/";

  devicePath +=
    DEVICE_ID;


  // ----------------------------------------------------------
  // CREATE ONE JSON PACKET
  // ----------------------------------------------------------

  FirebaseJson json;


  /*
    Everything inside "live" is sent together.

    This is much better than making many separate
    Firebase requests.
  */


  json.set(
    "live/hpfDegradation",
    windowFull && contactDetected
      ? hpfDegradationIndex
      : 0
  );


  json.set(
    "live/vasodilation",
    windowFull && contactDetected
      ? vasodilationIndex
      : 0
  );


  json.set(
    "live/liveIndex",
    windowFull && contactDetected
      ? preSymptomaticIndex
      : 0
  );


  json.set(
    "live/bpm",
    beatAvg
  );


  json.set(
    "live/contact",
    contactDetected
  );


  json.set(
    "live/ir",
    irValue
  );


  // ----------------------------------------------------------
  // TIMESTAMP
  // ----------------------------------------------------------

  json.set(
    "live/timestamp/.sv",
    "timestamp"
  );


  // ----------------------------------------------------------
  // UPLOAD COMPLETE PACKET
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "Uploading sensor data to Firebase..."
  );


  if (
    Firebase.RTDB.updateNode(
      &fbdo,
      devicePath.c_str(),
      &json
    )
  ) {

    Serial.println(
      "Firebase upload SUCCESS"
    );

    Serial.print(
      "Path: "
    );

    Serial.println(
      devicePath
    );

  }

  else {

    Serial.println(
      "Firebase upload FAILED"
    );

    Serial.print(
      "Reason: "
    );

    Serial.println(
      fbdo.errorReason()
    );

  }

}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(200);


  Serial.println();

  Serial.println(
    "============================================"
  );

  Serial.println(
    "ESP32 + MAX30102"
  );

  Serial.println(
    "OA Knee-Patch Prototype"
  );

  Serial.println(
    "============================================"
  );


  Serial.println();

  Serial.println(
    "MAX30102 stands in for the intended"
  );

  Serial.println(
    "NIRS / impedance module."
  );


  Serial.println();

  Serial.println(
    "OA Pre-symptomatic Index is an"
  );

  Serial.println(
    "EXPERIMENTAL PROXY, not a diagnosis."
  );


  // ==========================================================
  // WIFI
  // ==========================================================

  connectWiFi();


  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(
    21,
    22
  );


  // ==========================================================
  // MAX30102
  // ==========================================================

  Serial.println();

  Serial.println(
    "Starting MAX30102..."
  );


  if (
    !particleSensor.begin(
      Wire,
      I2C_SPEED_FAST
    )
  ) {

    Serial.println();

    Serial.println(
      "MAX30102 not found!"
    );

    Serial.println(
      "Check:"
    );

    Serial.println(
      "SDA -> GPIO21"
    );

    Serial.println(
      "SCL -> GPIO22"
    );

    Serial.println(
      "VIN -> 3.3V"
    );

    Serial.println(
      "GND -> GND"
    );


    while (1) {

      delay(10);

    }

  }


  Serial.println(
    "MAX30102 found!"
  );


  Serial.println();

  Serial.println(
    "Place sensor on skin with steady,"
  );

  Serial.println(
    "gentle contact."
  );


  // ==========================================================
  // MAX30102 CONFIGURATION
  // ==========================================================

  byte ledBrightness =
    0x1F;


  byte sampleAverage =
    4;


  byte ledMode =
    2;


  int sampleRate =
    100;


  int pulseWidth =
    411;


  int adcRange =
    4096;


  particleSensor.setup(

    ledBrightness,

    sampleAverage,

    ledMode,

    sampleRate,

    pulseWidth,

    adcRange

  );


  // ==========================================================
  // FIREBASE
  // ==========================================================

  initializeFirebase();


  Serial.println();

  Serial.println(
    "System ready."
  );

  Serial.println(
    "============================================"
  );

}


// ============================================================
// LOOP
// ============================================================

void loop() {


  // ==========================================================
  // CHECK WIFI
  // ==========================================================

  checkWiFi();


  // ==========================================================
  // READ MAX30102
  // ==========================================================

  long irValue =
    particleSensor.getIR();


  // ==========================================================
  // HEART RATE
  // ==========================================================

  if (
    checkForBeat(irValue)
  ) {


    long delta =
      millis() - lastBeat;


    lastBeat =
      millis();


    beatsPerMinute =
      60.0 /
      (delta / 1000.0);


    if (
      beatsPerMinute > 20 &&
      beatsPerMinute < 255
    ) {


      rates[rateSpot++] =
        (byte)beatsPerMinute;


      rateSpot %=
        RATE_SIZE;


      beatAvg =
        0;


      for (
        byte x = 0;
        x < RATE_SIZE;
        x++
      ) {

        beatAvg +=
          rates[x];

      }


      beatAvg /=
        RATE_SIZE;

    }

  }


  // ==========================================================
  // UPDATE SENSOR METRICS
  // ==========================================================

  updateWindowMetrics(
    irValue
  );


  // ==========================================================
  // SERIAL MONITOR
  // ==========================================================

  Serial.print(
    "IR="
  );

  Serial.print(
    irValue
  );


  Serial.print(
    "  %HPFDeg="
  );


  if (
    windowFull &&
    contactDetected
  ) {

    Serial.print(
      hpfDegradationIndex,
      2
    );

  }

  else {

    Serial.print(
      "--"
    );

  }


  Serial.print(
    "  %Vasodilation="
  );


  if (
    windowFull &&
    contactDetected
  ) {

    Serial.print(
      vasodilationIndex,
      2
    );

  }

  else {

    Serial.print(
      "--"
    );

  }


  Serial.print(
    "  RefBPM="
  );

  Serial.print(
    beatAvg
  );


  Serial.print(
    "  Contact="
  );


  if (
    contactDetected
  ) {

    Serial.print(
      "YES"
    );

  }

  else {

    Serial.print(
      "NO"
    );

  }


  Serial.print(
    "  |  OA Pre-symptomatic Index: "
  );


  if (
    windowFull &&
    contactDetected
  ) {

    Serial.print(
      preSymptomaticIndex,
      1
    );

    Serial.println(
      "% (experimental proxy)"
    );

  }

  else {

    Serial.println(
      "-- (No contact detected)"
    );

  }


  // ==========================================================
  // FIREBASE UPLOAD
  // ==========================================================

  if (
    millis() - lastFirebaseUpload >=
    FIREBASE_UPLOAD_INTERVAL
  ) {

    lastFirebaseUpload =
      millis();


    uploadFirebaseData(
      irValue
    );

  }


  // ==========================================================
  // 20 ms LOOP DELAY
  // ==========================================================

  delay(20);

}