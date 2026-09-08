/*
  ESP32 DevKitV1 + MAX30102 — OA Knee-Patch Prototype (proxy sensor)
  --------------------------------------------------------------------
  Original sensor and OA-proxy calculation code preserved.

  ADDED:
    - WiFi connection
    - Firebase Email/Password authentication
    - Firebase Realtime Database connection
    - Live data upload
    - Stable segment result upload

  IMPORTANT:
    Replace the Firebase/WiFi values in the CONFIGURATION section below.
*/

#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// ======================= ADDED LIBRARIES =========================
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Firebase helper libraries
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ======================= CONFIGURATION ===========================

// -------- WiFi --------
#define WIFI_SSID       "YOUR_WIFI_NAME"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// -------- Firebase --------
// Firebase Project Settings -> General -> Web API Key
#define API_KEY         "YOUR_FIREBASE_WEB_API_KEY"

// Firebase Realtime Database URL
// Example:
// https://your-project-default-rtdb.firebaseio.com
#define DATABASE_URL    "https://YOUR_PROJECT-default-rtdb.firebaseio.com/"

// -------- Firebase Authentication --------
// Firebase Authentication -> Users
#define USER_EMAIL      "YOUR_FIREBASE_EMAIL"
#define USER_PASSWORD   "YOUR_FIREBASE_PASSWORD"

// Device name used in Firebase
#define DEVICE_ID       "ESP32_01"

// ================================================================


// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastFirebaseUpload = 0;

// Upload live values every 2 seconds
const unsigned long FIREBASE_UPLOAD_INTERVAL = 2000;


// =================================================================
// ORIGINAL CODE STARTS HERE
// =================================================================

MAX30105 particleSensor;

// ---------------- Heart-rate detection (kept as a reference channel) -------
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// ---------------- Windowed waveform analysis --------------------------------
const int WINDOW_SIZE = 50;
long irWindow[WINDOW_SIZE];
int windowPos = 0;
bool windowFull = false;

float vasodilationIndex = 0;
float PERF_INDEX_SCALE = 5.0;

float hpfDegradationIndex = 0;

// ---------------- Combined live index ----------------------------------------
float WEIGHT_HPF  = 0.5;
float WEIGHT_VASO = 0.5;

float preSymptomaticIndex = 0;

// ---------------- Contact gating --------------------------------------------
const long CONTACT_THRESHOLD = 50000;
bool contactDetected = false;
bool prevContactDetected = false;

// ---------------- Segment (jerk-to-jerk) averaging ---------------------------
const unsigned long SETTLE_MS   = 1200;
const unsigned long END_GUARD_MS = 500;
const int MAX_SEGMENT_SAMPLES = 120;

unsigned long segTimestamps[MAX_SEGMENT_SAMPLES];
float segHpf[MAX_SEGMENT_SAMPLES];
float segVaso[MAX_SEGMENT_SAMPLES];

int segCount = 0;
unsigned long contactStartTime = 0;

float segmentPreSymptomaticIndex = -1;
bool haveSegmentResult = false;


// =================================================================
// FIREBASE FUNCTIONS
// =================================================================


// -------------------- Connect to WiFi -----------------------------
void connectWiFi() {

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {

    delay(500);
    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("WiFi connection failed.");
    Serial.println("Sensor will continue working without Firebase.");
  }
}


// -------------------- Firebase setup ------------------------------
void setupFirebase() {

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  Serial.println("Firebase initialized.");
}


// -------------------- Upload live data ----------------------------
void uploadLiveData() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!Firebase.ready()) {
    return;
  }

  String basePath = "/devices/" + String(DEVICE_ID) + "/live";

  // IR value
  long irValue = particleSensor.getIR();

  if (!Firebase.RTDB.setInt(&fbdo,
                            basePath + "/irValue",
                            irValue)) {

    Serial.print("Firebase IR error: ");
    Serial.println(fbdo.errorReason());
  }

  // Contact
  if (!Firebase.RTDB.setBool(&fbdo,
                             basePath + "/contact",
                             contactDetected)) {

    Serial.print("Firebase contact error: ");
    Serial.println(fbdo.errorReason());
  }

  // HPF degradation
  if (windowFull && contactDetected) {

    Firebase.RTDB.setFloat(
      &fbdo,
      basePath + "/hpfDegradation",
      hpfDegradationIndex
    );

    // Vasodilation
    Firebase.RTDB.setFloat(
      &fbdo,
      basePath + "/vasodilation",
      vasodilationIndex
    );

    // Live OA index
    Firebase.RTDB.setFloat(
      &fbdo,
      basePath + "/liveIndex",
      preSymptomaticIndex
    );
  }

  // BPM
  Firebase.RTDB.setInt(
    &fbdo,
    basePath + "/bpm",
    beatAvg
  );

  // Stable segment index
  if (haveSegmentResult) {

    Firebase.RTDB.setFloat(
      &fbdo,
      basePath + "/stableSegmentIndex",
      segmentPreSymptomaticIndex
    );
  }

  // Timestamp
  Firebase.RTDB.setInt(
    &fbdo,
    basePath + "/lastUpdate",
    millis()
  );

  Serial.println("Firebase live data uploaded.");
}


// ---------------- Upload completed measurement ------------------
void uploadCompletedMeasurement() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Measurement not uploaded.");
    return;
  }

  if (!Firebase.ready()) {
    Serial.println("Firebase not ready. Measurement not uploaded.");
    return;
  }

  String path =
    "/devices/" + String(DEVICE_ID) + "/measurement";

  // Stable combined index
  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/stableIndex",
    segmentPreSymptomaticIndex
  );

  // Current HPF
  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/hpfDegradation",
    hpfDegradationIndex
  );

  // Current vasodilation
  Firebase.RTDB.setFloat(
    &fbdo,
    path + "/vasodilation",
    vasodilationIndex
  );

  // BPM
  Firebase.RTDB.setInt(
    &fbdo,
    path + "/bpm",
    beatAvg
  );

  // Number of samples used
  Firebase.RTDB.setInt(
    &fbdo,
    path + "/samples",
    segCount
  );

  // Timestamp
  Firebase.RTDB.setInt(
    &fbdo,
    path + "/timestamp",
    millis()
  );

  Serial.println("----------------------------------");
  Serial.println("COMPLETED MEASUREMENT UPLOADED");
  Serial.print("Stable Index: ");
  Serial.println(segmentPreSymptomaticIndex);
  Serial.println("----------------------------------");
}


// =================================================================
// ORIGINAL FUNCTION — ONLY FIREBASE CALL ADDED AFTER FINALIZATION
// =================================================================

void finalizeSegment(unsigned long lossTime) {

  unsigned long cutoff =
    (lossTime > END_GUARD_MS)
    ? (lossTime - END_GUARD_MS)
    : 0;

  float hpfSum = 0;
  float vasoSum = 0;

  int usedCount = 0;

  for (int i = 0; i < segCount; i++) {

    if (segTimestamps[i] <= cutoff) {

      hpfSum += segHpf[i];
      vasoSum += segVaso[i];

      usedCount++;
    }
  }

  if (usedCount > 0) {

    float hpfAvg  = hpfSum / usedCount;
    float vasoAvg = vasoSum / usedCount;

    segmentPreSymptomaticIndex =
      constrain(
        WEIGHT_HPF * hpfAvg +
        WEIGHT_VASO * vasoAvg,
        0,
        100
      );

    haveSegmentResult = true;

    // ================= FIREBASE ADDITION =================
    uploadCompletedMeasurement();
    // =====================================================
  }

  // If usedCount == 0, previous result remains unchanged.
}


// =================================================================
// ORIGINAL WINDOW FUNCTION
// =================================================================

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

    contactDetected =
      (irDC >= CONTACT_THRESHOLD);


    // ---- "1st jerk": contact just started ----
    if (contactDetected && !prevContactDetected) {

      contactStartTime = millis();

      segCount = 0;
    }


    if (contactDetected) {

      float perfusionIndexPct =
        (irAC / irDC) * 100.0;

      vasodilationIndex =
        constrain(
          perfusionIndexPct * PERF_INDEX_SCALE,
          0,
          100
        );


      int sampleGap =
        maxIdx - minIdx;

      if (sampleGap < 0)
        sampleGap += WINDOW_SIZE;


      float riseTimeFraction =
        sampleGap / (float)WINDOW_SIZE;


      hpfDegradationIndex =
        constrain(
          (1.0 - riseTimeFraction) * 100.0,
          0,
          100
        );


      preSymptomaticIndex =
        constrain(
          WEIGHT_HPF * hpfDegradationIndex +
          WEIGHT_VASO * vasodilationIndex,
          0,
          100
        );


      // Store segment data after settling period
      unsigned long now = millis();

      if (
        now - contactStartTime >= SETTLE_MS &&
        segCount < MAX_SEGMENT_SAMPLES
      ) {

        segTimestamps[segCount] = now;

        segHpf[segCount] =
          hpfDegradationIndex;

        segVaso[segCount] =
          vasodilationIndex;

        segCount++;
      }
    }


    // ---- "2nd jerk": contact ended ----
    if (!contactDetected && prevContactDetected) {

      finalizeSegment(millis());
    }


    prevContactDetected =
      contactDetected;
  }
}


// =================================================================
// SETUP
// =================================================================

void setup() {

  Serial.begin(115200);

  delay(200);

  Serial.println();

  Serial.println(
    "ESP32 + MAX30102 -- OA Knee-Patch Prototype"
  );

  Serial.println(
    "OA Pre-symptomatic Index is an EXPERIMENTAL PROXY, not a diagnosis."
  );


  // ---------------- WiFi ----------------

  connectWiFi();


  // ---------------- Firebase ------------

  if (WiFi.status() == WL_CONNECTED) {

    setupFirebase();
  }


  // ---------------- MAX30102 ------------

  Wire.begin(21, 22);


  if (!particleSensor.begin(
        Wire,
        I2C_SPEED_FAST
      )) {

    Serial.println(
      "MAX30102 not found. Check wiring."
    );

    while (1) {
      delay(10);
    }
  }


  Serial.println(
    "Sensor found. Place on skin with steady, gentle contact."
  );


  byte ledBrightness = 0x1F;

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
}


// =================================================================
// LOOP
// =================================================================

void loop() {

  long irValue =
    particleSensor.getIR();


  // ---------------- Heart rate ----------------

  if (checkForBeat(irValue)) {

    long delta =
      millis() - lastBeat;

    lastBeat =
      millis();


    beatsPerMinute =
      60.0 / (delta / 1000.0);


    if (
      beatsPerMinute > 20 &&
      beatsPerMinute < 255
    ) {

      rates[rateSpot++] =
        (byte)beatsPerMinute;

      rateSpot %= RATE_SIZE;


      beatAvg = 0;

      for (
        byte x = 0;
        x < RATE_SIZE;
        x++
      ) {

        beatAvg += rates[x];
      }

      beatAvg /= RATE_SIZE;
    }
  }


  // ---------------- Sensor analysis ----------------

  updateWindowMetrics(irValue);


  // ---------------- Serial output ----------------

  Serial.print("IR=");
  Serial.print(irValue);


  Serial.print("  %HPFDeg=");

  Serial.print(
    (windowFull && contactDetected)
      ? String(hpfDegradationIndex, 2)
      : String("--")
  );


  Serial.print("  %Vasodilation=");

  Serial.print(
    (windowFull && contactDetected)
      ? String(vasodilationIndex, 2)
      : String("--")
  );


  Serial.print("  RefBPM=");
  Serial.print(beatAvg);


  Serial.print("  |  Live Index: ");


  if (
    windowFull &&
    contactDetected
  ) {

    Serial.print(
      preSymptomaticIndex,
      1
    );

    Serial.print("%");

  } else {

    Serial.print(
      "-- (No contact detected)"
    );
  }


  Serial.print(
    "  |  Stable Segment Index: "
  );


  if (haveSegmentResult) {

    Serial.print(
      segmentPreSymptomaticIndex,
      1
    );

    Serial.print(
      "% (avg of last completed measurement, experimental proxy)"
    );

  } else {

    Serial.print(
      "-- (no completed measurement yet)"
    );
  }


  Serial.println();


  // ---------------- Firebase live upload ----------------

  if (
    millis() - lastFirebaseUpload >=
    FIREBASE_UPLOAD_INTERVAL
  ) {

    lastFirebaseUpload =
      millis();

    uploadLiveData();
  }


  delay(20);
}
