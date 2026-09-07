/*
  ============================================================
   OSTEOTRACK - ESP32 + MAX30102 + Firebase
  ============================================================

   Hardware:
   - ESP32 DevKit
   - MAX30102

   Firebase:
   sensorData/ESP32_01/
      heartRate
      spo2
      hpfDegradation
      hypervascularization
      presymptomaticIndex
      timestamp

   IMPORTANT:
   This is an experimental physiological-monitoring prototype.
   MAX30102 does NOT diagnose osteoarthritis.

   HPF degradation, hypervascularization and pre-symptomatic
   index are placeholders until validated formulas are supplied.
  ============================================================
*/

#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include <MAX30105.h>
#include "spo2_algorithm.h"

#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// ============================================================
// 1. WIFI SETTINGS
// ============================================================

#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"


// ============================================================
// 2. FIREBASE SETTINGS
// ============================================================

#define API_KEY         "YOUR_FIREBASE_API_KEY"

#define DATABASE_URL    "YOUR_FIREBASE_DATABASE_URL"

#define FIREBASE_USER_EMAIL     "YOUR_FIREBASE_EMAIL"
#define FIREBASE_USER_PASSWORD  "YOUR_FIREBASE_PASSWORD"


// ============================================================
// 3. DEVICE
// ============================================================

#define DEVICE_ID "ESP32_01"


// ============================================================
// 4. I2C PINS
// ============================================================

#define SDA_PIN 21
#define SCL_PIN 22


// ============================================================
// 5. SENSOR SETTINGS
// ============================================================

#define BUFFER_SIZE 100
#define SAMPLE_RATE_HZ 100

#define FINGER_THRESHOLD 50000


// ============================================================
// 6. EXPERIMENTAL DESATURATION SETTINGS
// ============================================================

#define DESAT_DROP_PERCENT 3
#define DESAT_MIN_DURATION_S 10
#define ODI_ALERT_THRESHOLD 5


// ============================================================
// 7. FIREBASE UPDATE TIMER
// ============================================================

const unsigned long FIREBASE_UPDATE_INTERVAL = 5000;

unsigned long lastFirebaseUpdate = 0;


// ============================================================
// 8. INDIA TIME
// ============================================================

const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;


// ============================================================
// 9. OBJECTS
// ============================================================

MAX30105 particleSensor;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ============================================================
// 10. SENSOR VARIABLES
// ============================================================

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2 = 0;
int8_t validSPO2 = 0;

int32_t heartRate = 0;
int8_t validHeartRate = 0;


// ============================================================
// 11. DESATURATION VARIABLES
// ============================================================

float baselineSpO2 = -1;

unsigned long desatStartTime = 0;

bool inDesatEvent = false;

unsigned int desatEventCount = 0;

unsigned long windowStartTime = 0;

const unsigned long ONE_HOUR_MS = 3600000UL;


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("          OSTEOTRACK");
  Serial.println(" ESP32 + MAX30102 + Firebase");
  Serial.println("================================");
  Serial.println();


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);


  // ----------------------------------------------------------
  // MAX30102
  // ----------------------------------------------------------

  Serial.println("Initializing MAX30102...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("ERROR: MAX30102 not found!");
    Serial.println("Check SDA, SCL, VIN and GND wiring.");

    while (true)
    {
      delay(1000);
    }
  }


  // ----------------------------------------------------------
  // SENSOR CONFIGURATION
  // ----------------------------------------------------------

  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;

  int sampleRate = SAMPLE_RATE_HZ;

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


  Serial.println("MAX30102 initialized.");


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  Serial.println();
  Serial.print("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  int wifiAttempts = 0;


  while (
    WiFi.status() != WL_CONNECTED &&
    wifiAttempts < 30
  )
  {
    delay(500);

    Serial.print(".");

    wifiAttempts++;
  }


  Serial.println();


  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP: ");

    Serial.println(
      WiFi.localIP()
    );
  }
  else
  {
    Serial.println("Wi-Fi connection failed.");

    Serial.println(
      "Check Wi-Fi name/password."
    );
  }


  // ----------------------------------------------------------
  // TIME
  // ----------------------------------------------------------

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov"
  );

  Serial.println(
    "Time synchronization started."
  );


  // ----------------------------------------------------------
  // FIREBASE
  // ----------------------------------------------------------

  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;


  auth.user.email =
    FIREBASE_USER_EMAIL;

  auth.user.password =
    FIREBASE_USER_PASSWORD;


  config.token_status_callback =
    tokenStatusCallback;


  Firebase.begin(
    &config,
    &auth
  );


  Firebase.reconnectWiFi(true);


  Serial.println(
    "Firebase initialized."
  );


  // ----------------------------------------------------------
  // TIMER
  // ----------------------------------------------------------

  windowStartTime = millis();


  Serial.println();
  Serial.println("Setup complete.");

  Serial.println(
    "Place your finger on MAX30102."
  );

  Serial.println();

  Serial.println(
    "time,IR,heartRate,validHR,SpO2,validSpO2,event"
  );
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{

  // ----------------------------------------------------------
  // CHECK WIFI
  // ----------------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(
      "Wi-Fi disconnected. Reconnecting..."
    );

    WiFi.reconnect();

    delay(1000);

    return;
  }


  // ----------------------------------------------------------
  // COLLECT SENSOR DATA
  // ----------------------------------------------------------

  for (int i = 0; i < BUFFER_SIZE; i++)
  {

    while (!particleSensor.available())
    {
      particleSensor.check();
    }


    redBuffer[i] =
      particleSensor.getRed();


    irBuffer[i] =
      particleSensor.getIR();


    particleSensor.nextSample();
  }


  // ----------------------------------------------------------
  // FINGER DETECTION
  // ----------------------------------------------------------

  bool fingerPresent =
    (
      irBuffer[BUFFER_SIZE - 1]
      > FINGER_THRESHOLD
    );


  if (!fingerPresent)
  {
    Serial.println(
      "No finger detected - place finger on sensor."
    );

    inDesatEvent = false;

    return;
  }


  // ----------------------------------------------------------
  // CALCULATE HEART RATE + SPO2
  // ----------------------------------------------------------

  maxim_heart_rate_and_oxygen_saturation(

    irBuffer,

    BUFFER_SIZE,

    redBuffer,

    &spo2,

    &validSPO2,

    &heartRate,

    &validHeartRate
  );


  // ----------------------------------------------------------
  // PROCESS READING
  // ----------------------------------------------------------

  processReading();


  // ----------------------------------------------------------
  // RESET HOURLY WINDOW
  // ----------------------------------------------------------

  resetHourlyWindowIfNeeded();
}


// ============================================================
// PROCESS SENSOR READING
// ============================================================

void processReading()
{

  unsigned long nowS =
    millis() / 1000;


  String eventTag = "";


  // ----------------------------------------------------------
  // SPO2 PROCESSING
  // ----------------------------------------------------------

  if (
    validSPO2 &&
    spo2 > 0 &&
    spo2 <= 100
  )
  {

    // --------------------------------------------------------
    // Establish baseline
    // --------------------------------------------------------

    if (baselineSpO2 < 0)
    {
      baselineSpO2 = spo2;
    }

    else if (!inDesatEvent)
    {
      baselineSpO2 =
        (0.95 * baselineSpO2) +
        (0.05 * spo2);
    }


    // --------------------------------------------------------
    // Calculate SpO2 drop
    // --------------------------------------------------------

    float drop =
      baselineSpO2 - spo2;


    if (
      drop >= DESAT_DROP_PERCENT
    )
    {

      if (!inDesatEvent)
      {
        inDesatEvent = true;

        desatStartTime =
          millis();
      }

      else
      {
        unsigned long duration =
          (
            millis() -
            desatStartTime
          ) / 1000;


        if (
          duration >=
          DESAT_MIN_DURATION_S
        )
        {
          eventTag =
            "DESAT_EVENT";
        }
      }
    }

    else
    {

      if (inDesatEvent)
      {

        unsigned long duration =
          (
            millis() -
            desatStartTime
          ) / 1000;


        if (
          duration >=
          DESAT_MIN_DURATION_S
        )
        {
          desatEventCount++;

          eventTag =
            "DESAT_RESOLVED";
        }
      }


      inDesatEvent = false;
    }
  }


  // ----------------------------------------------------------
  // SERIAL MONITOR
  // ----------------------------------------------------------

  Serial.print(nowS);

  Serial.print(",");

  Serial.print(
    irBuffer[BUFFER_SIZE - 1]
  );

  Serial.print(",");

  Serial.print(heartRate);

  Serial.print(",");

  Serial.print(validHeartRate);

  Serial.print(",");

  Serial.print(spo2);

  Serial.print(",");

  Serial.print(validSPO2);

  Serial.print(",");

  Serial.println(eventTag);


  // ----------------------------------------------------------
  // SEND TO FIREBASE EVERY 5 SECONDS
  // ----------------------------------------------------------

  if (
    millis() -
    lastFirebaseUpdate >=
    FIREBASE_UPDATE_INTERVAL
  )
  {

    lastFirebaseUpdate =
      millis();


    sendDataToFirebase();
  }


  // ----------------------------------------------------------
  // EXPERIMENTAL ODI
  // ----------------------------------------------------------

  float elapsedHours =
    (
      millis() -
      windowStartTime
    ) /
    (float)ONE_HOUR_MS;


  if (elapsedHours > 0)
  {

    float odi =
      desatEventCount /
      elapsedHours;


    if (
      odi >=
      ODI_ALERT_THRESHOLD
    )
    {

      Serial.print(
        "ALERT: Estimated ODI = "
      );

      Serial.print(
        odi,
        1
      );

      Serial.println(
        " events/hr - screening only."
      );
    }
  }
}


// ============================================================
// SEND DATA TO FIREBASE
// ============================================================

void sendDataToFirebase()
{

  // ----------------------------------------------------------
  // CHECK FIREBASE
  // ----------------------------------------------------------

  if (!Firebase.ready())
  {
    Serial.println(
      "Firebase not ready."
    );

    return;
  }


  // ----------------------------------------------------------
  // CHECK HEART RATE
  // ----------------------------------------------------------

  if (
    !validHeartRate ||
    heartRate <= 0
  )
  {
    Serial.println(
      "Heart rate invalid - not sending."
    );

    return;
  }


  // ----------------------------------------------------------
  // CHECK SPO2
  // ----------------------------------------------------------

  if (
    !validSPO2 ||
    spo2 <= 0 ||
    spo2 > 100
  )
  {
    Serial.println(
      "SpO2 invalid - not sending."
    );

    return;
  }


  // ----------------------------------------------------------
  // FIREBASE PATH
  // ----------------------------------------------------------

  String path =
    "/sensorData/" +
    String(DEVICE_ID);


  // ----------------------------------------------------------
  // HEART RATE
  // ----------------------------------------------------------

  if (
    Firebase.RTDB.setInt(
      &fbdo,
      path + "/heartRate",
      heartRate
    )
  )
  {
    Serial.print(
      "Heart Rate sent: "
    );

    Serial.println(
      heartRate
    );
  }
  else
  {
    Serial.print(
      "Heart Rate Firebase error: "
    );

    Serial.println(
      fbdo.errorReason()
    );
  }


  // ----------------------------------------------------------
  // SPO2
  // ----------------------------------------------------------

  if (
    Firebase.RTDB.setInt(
      &fbdo,
      path + "/spo2",
      spo2
    )
  )
  {
    Serial.print(
      "SpO2 sent: "
    );

    Serial.println(
      spo2
    );
  }
  else
  {
    Serial.print(
      "SpO2 Firebase error: "
    );

    Serial.println(
      fbdo.errorReason()
    );
  }


  // ----------------------------------------------------------
  // EXPERIMENTAL OA FIELDS
  // ----------------------------------------------------------
  //
  // These are intentionally -1.
  // We do NOT invent medical formulas.
  //

  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/hpfDegradation",
    -1.0
  );


  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/hypervascularization",
    -1.0
  );


  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/presymptomaticIndex",
    -1.0
  );


  // ----------------------------------------------------------
  // TIMESTAMP
  // ----------------------------------------------------------

  struct tm timeinfo;


  if (
    getLocalTime(
      &timeinfo
    )
  )
  {

    char timestamp[30];


    strftime(
      timestamp,
      sizeof(timestamp),
      "%Y-%m-%dT%H:%M:%S",
      &timeinfo
    );


    if (
      Firebase.RTDB.setString(
        &fbdo,
        path + "/timestamp",
        timestamp
      )
    )
    {
      Serial.print(
        "Timestamp sent: "
      );

      Serial.println(
        timestamp
      );
    }
    else
    {
      Serial.print(
        "Timestamp Firebase error: "
      );

      Serial.println(
        fbdo.errorReason()
      );
    }
  }


  Serial.println(
    "Firebase update complete."
  );

  Serial.println();
}


// ============================================================
// RESET HOURLY WINDOW
// ============================================================

void resetHourlyWindowIfNeeded()
{

  if (
    millis() -
    windowStartTime >=
    ONE_HOUR_MS
  )
  {

    Serial.print(
      "Hourly summary - desaturation events: "
    );


    Serial.println(
      desatEventCount
    );


    desatEventCount = 0;

    windowStartTime =
      millis();
  }
}