/*
 * ============================================================
 *  Smart School Safety - SCHOOL GATE ESP32
 *  ST7789 240x240 + RFID RC522 + Firebase Realtime Database
 * ============================================================
 *  Flow:
 *   RFID Scan -> Student Identification -> Gate Event Detection
 *   -> Time & Date -> Firebase Update -> Event Log
 *   -> Notification Record -> Duplicate Protection -> TFT Screen
 * ============================================================
 */

#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Firebase_ESP_Client.h>
#include "time.h"

// ================= DEVICE INFO =================
#define DEVICE_LOCATION "GATE"
#define DEVICE_NAME "School Gate Unit"
#define SCHOOL_ID "school_main"
#define TOTAL_STUDENTS 25

// ================= WIFI =================
#define WIFI_SSID "NET"
#define WIFI_PASSWORD "Moe@1000000#"

// ================= FIREBASE =================
#define DATABASE_URL "https://iot-smart-school-safety-system-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "R5nloa3wK37IEbqbTzbPfmjy82w07BUA4CDTylRm"

// ================= TFT PINS =================
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18

// ================= RFID PINS =================
#define RFID_SS 5
#define RFID_RST 22

// If your RC522 reset pin is already wired to GPIO 0, change RFID_RST above to 0.
// GPIO 22 is safer because GPIO 0 is a boot strap pin on ESP32.

// ================= INDICATORS =================
// Green LED: GPIO 26 -> long leg, short leg -> 220 ohm -> GND
// Red LED:   GPIO 27 -> long leg, short leg -> 220 ohm -> GND
// Buzzer:    S/IO -> GPIO 25, + -> 3.3V, - -> GND
#define BUZZER_PIN 25
#define GREEN_LED_PIN 26
#define RED_LED_PIN 27

// ================= SCREEN SIZE =================
#define W 240
#define H 240

// ================= COLORS =================
#define C_BG 0x0008
#define C_BG2 0x0210
#define C_BG3 0x0430
#define C_ACCENT 0x051F
#define C_GREEN 0x07E0
#define C_GREEN_DK 0x03A0
#define C_BLUE 0x001F
#define C_BLUE_MED 0x035F
#define C_BLUE_LIGHT 0x3D7F
#define C_RED 0xF800
#define C_RED_DK 0x8000
#define C_AMBER 0xFEA0
#define C_AMBER_DK 0xB400
#define C_WHITE 0xFFFF
#define C_GRAY 0x7BEF
#define C_GRAY_DK 0x39E7
#define C_TEAL 0x07FF
#define C_HEADER_BG 0x0008

// ================= OBJECTS =================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
MFRC522 rfid(RFID_SS, RFID_RST);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ================= TIME =================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 0;

// ================= STATE =================
int presentCount = 0;
int totalStudents = TOTAL_STUDENTS;
bool firebaseReady = false;
bool wifiConnected = false;
bool bootSessionResetDone = false;

String lastStudentName = "";
String lastStudentStatus = "";
String lastScanTime = "";
String lastScanType = "";

String lastUID = "";
unsigned long lastScanMillis = 0;
const unsigned long scanCooldown = 3000;
const unsigned long firebaseServerTimeoutMs = 3000;
const unsigned long firebaseSocketTimeoutMs = 3000;
const unsigned long firebaseSslTimeoutMs = 3000;
const unsigned long firebaseWifiReconnectMs = 10000;
const unsigned long statusCacheTtlMs = 1000;
const bool writeSecondaryFirebaseRecords = false;
const unsigned long resultScreenMs = 2500;

unsigned long screenTimer = 0;
bool showingResult = false;

String eventType = "";
String newStatus = "";
String invalidReason = "";

String cachedStatusAnas = "";
String cachedStatusSalah = "";
String cachedStatusMohamed = "";
String cachedStatusAbdElrashid = "";
unsigned long cachedStatusAnasMs = 0;
unsigned long cachedStatusSalahMs = 0;
unsigned long cachedStatusMohamedMs = 0;
unsigned long cachedStatusAbdElrashidMs = 0;

enum Screen { SCR_IDLE, SCR_ENTRY, SCR_EXIT, SCR_WARNING, SCR_ERROR };
Screen currentScreen = SCR_IDLE;

// ================= CORE DECLARATIONS =================
void connectWiFi();
void setupTime();
void setupFirebase();
void resetDailySchoolSessionIfNeeded();
void syncCountFromFirebase();
void processRFID(String uid);
String getUID();
String getStudentName(String uid);
String getStudentInfo(String uid, String field);
String getCachedStatus(String uid);
String validCachedStatus(String status, unsigned long cachedAtMs);
void setCachedStatus(String uid, String status);
String getCurrentStatus(String uid);
String getEffectiveStatus(String uid, String status);
bool isSafetyRouteStatus(String status);
void determineGateEvent(String currentStatus);
bool isDuplicateScan(String uid);

String twoDigits(int value);
String getDate();
String getTimeText();
String getDisplayDate();
String getDisplayTime();
unsigned long getUnixTimestamp();
void setServerTimestamp(FirebaseJson &json);

String getMessageEn(String studentName, String eventName);
String getMessageAr(String studentName, String eventName);
String getLocationLabel();

bool updateStudentStatus(String uid, String studentName, String dateText, String timeText, unsigned long timestamp);
bool saveEventLog(String uid, String studentName, String dateText, String timeText, unsigned long timestamp);
bool createNotification(String uid, String studentName, String dateText, String timeText, unsigned long timestamp);
void updateSchoolCounters(bool isEntry);
void updateLegacyPaths(String uid, String studentName, String dateText, String timeText, unsigned long timestamp);
void updateDeviceHealth(unsigned long timestamp);
void saveUnknownScan(String uid);
void saveInvalidScan(String uid, String studentName, String currentStatus, String reason);

void printEvent(String uid, String studentName, String currentStatus, String dateText, String timeText);
void printUnknownCard(String uid);
void printInvalidScan(String uid, String studentName, String currentStatus, String reason);
void printFirebaseError(String action);

// ================= TFT DECLARATIONS =================
void drawBootScreen();
void drawScreen_Idle();
void drawScreen_Entry(String name, String grade);
void drawScreen_Exit(String name, String grade);
void drawScreen_Warning(String msg, String studentName);
void drawScreen_Error(String msg);
void drawBackground(uint16_t topColor);
void drawCard(int x, int y, int width, int height, uint16_t border, uint16_t fill);
void drawBadge(int x, int y, int width, String text, uint16_t bg, uint16_t fg);
void drawTopBar(String title, String subtitle, uint16_t accent);
void drawConnectionChip(int x, int y, String label, bool ok, uint16_t okColor);
void drawResultScreen(String title, String statusText, String name, String grade, uint16_t accent, bool isEntry);
void drawFooterLine(uint16_t accent);
void drawProgressBar(int x, int y, int width, int height, float pct, uint16_t col, uint16_t bg);
void drawStatBox(int x, int y, int width, int height, String label, int value, uint16_t col);
void drawStatusBar();
void drawCenteredText(String txt, int y, uint16_t col, uint8_t size);
void drawDivider(int y, uint16_t col);
void drawWrappedText(String msg, int x, int y, int lineLen, int lineHeight, uint16_t col);
void drawIcon_School(int x, int y, uint16_t col);
void drawIcon_Check(int x, int y, uint16_t col);
void drawIcon_Exit(int x, int y, uint16_t col);
void drawIcon_Warning(int x, int y, uint16_t col);
void drawIcon_Person(int x, int y, uint16_t col, bool withBag);
void buzz(int ms, int times = 1);
void setIndicators(bool greenOn, bool redOn);
void successIndicator(int ms = 180, int times = 1);
void warningIndicator();
void errorIndicator();
void bootIndicatorTest();

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  setIndicators(false, false);
  bootIndicatorTest();

  pinMode(TFT_CS, OUTPUT);
  pinMode(RFID_SS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(RFID_SS, HIGH);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);

  tft.init(W, H);
  tft.setRotation(0);
  tft.fillScreen(C_BG);

  drawBootScreen();

  rfid.PCD_Init();

  Serial.println("================================");
  Serial.println("Smart School Safety System");
  Serial.println("School Gate Unit Starting...");
  Serial.println("================================");

  connectWiFi();
  setupTime();
  setupFirebase();
  resetDailySchoolSessionIfNeeded();

  Serial.println("School Gate Unit Ready");
  Serial.println("Scan RFID Card...");
  drawScreen_Idle();
}

// ================= LOOP =================
void loop() {
  if (showingResult && millis() - screenTimer > resultScreenMs) {
    showingResult = false;
    currentScreen = SCR_IDLE;
    setIndicators(false, false);
    drawScreen_Idle();
  }

  static unsigned long clockRefresh = 0;
  if (currentScreen == SCR_IDLE && millis() - clockRefresh > 30000) {
    clockRefresh = millis();
    drawStatusBar();
  }

  static unsigned long wifiRetry = 0;
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    if (millis() - wifiRetry > firebaseWifiReconnectMs) {
      wifiRetry = millis();
      Serial.println("WiFi disconnected. Non-blocking reconnect started.");
      WiFi.disconnect(false);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    if (currentScreen == SCR_IDLE) drawStatusBar();
  } else {
    wifiConnected = true;
  }

  static unsigned long firebaseRetry = 0;
  if (wifiConnected && !firebaseReady && millis() - firebaseRetry > 10000) {
    firebaseRetry = millis();
    setupFirebase();
    resetDailySchoolSessionIfNeeded();
    if (currentScreen == SCR_IDLE) drawScreen_Idle();
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = getUID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (isDuplicateScan(uid)) {
    Serial.println("Duplicate scan ignored.");
    errorIndicator();
    return;
  }

  lastUID = uid;
  lastScanMillis = millis();
  processRFID(uid);
}

// ================= RFID LOGIC =================
void processRFID(String uid) {
  unsigned long scanStartMs = millis();
  Serial.println("RFID: " + uid);

  unsigned long lookupStartMs = millis();
  String studentName = getStudentName(uid);
  String grade = "";
  String currentStatus = getCurrentStatus(uid);
  currentStatus = getEffectiveStatus(uid, currentStatus);
  lastScanTime = getDisplayTime();
  Serial.print("Lookup/status time ms: ");
  Serial.println(millis() - lookupStartMs);

  if (studentName == "Unknown") {
    errorIndicator();
    printUnknownCard(uid);
    saveUnknownScan(uid);
    drawScreen_Error("Unknown Card\nUID: " + uid);
    showingResult = true;
    screenTimer = millis();
    currentScreen = SCR_ERROR;
    return;
  }

  if (!firebaseReady || WiFi.status() != WL_CONNECTED) {
    firebaseReady = false;
    warningIndicator();
    Serial.println("Scan blocked: Firebase or WiFi is not ready.");
    drawScreen_Warning("Firebase offline.\nCheck WiFi/FB.", studentName);
    showingResult = true;
    screenTimer = millis();
    currentScreen = SCR_WARNING;
    return;
  }

  determineGateEvent(currentStatus);

  if (eventType.length() == 0 || newStatus.length() == 0) {
    warningIndicator();
    printInvalidScan(uid, studentName, currentStatus, invalidReason);
    saveInvalidScan(uid, studentName, currentStatus, invalidReason);
    drawScreen_Warning(invalidReason, studentName);
    showingResult = true;
    screenTimer = millis();
    currentScreen = SCR_WARNING;
    return;
  }

  bool isEntry = newStatus == "AT_SCHOOL";
  String dateText = getDate();
  String timeText = getTimeText();
  unsigned long timestamp = getUnixTimestamp();

  printEvent(uid, studentName, currentStatus, dateText, timeText);

  unsigned long firebaseStartMs = millis();
  bool statusOk = updateStudentStatus(uid, studentName, dateText, timeText, timestamp);
  bool eventOk = saveEventLog(uid, studentName, dateText, timeText, timestamp);
  Serial.print("Critical Firebase writes ms: ");
  Serial.println(millis() - firebaseStartMs);

  if (!statusOk || !eventOk) {
    firebaseReady = false;
    warningIndicator();
    drawScreen_Warning("Firebase send failed.\nCheck Serial Monitor.", studentName);
    showingResult = true;
    screenTimer = millis();
    currentScreen = SCR_WARNING;
    Serial.print("Total scan handling ms: ");
    Serial.println(millis() - scanStartMs);
    return;
  }

  lastStudentName = studentName;
  lastStudentStatus = newStatus;
  lastScanType = isEntry ? "IN" : "OUT";
  setCachedStatus(uid, newStatus);

  if (isEntry) {
    drawScreen_Entry(studentName, grade);
    currentScreen = SCR_ENTRY;
    successIndicator(200, 1);
  } else {
    drawScreen_Exit(studentName, grade);
    currentScreen = SCR_EXIT;
    successIndicator(120, 2);
  }
  showingResult = true;
  screenTimer = millis();

  updateSchoolCounters(isEntry);

  if (writeSecondaryFirebaseRecords) {
    bool notificationOk = createNotification(uid, studentName, dateText, timeText, timestamp);
    if (!notificationOk) {
      Serial.println("Notification record failed, but event log will still trigger app push.");
    }

    updateLegacyPaths(uid, studentName, dateText, timeText, timestamp);
    updateDeviceHealth(timestamp);
  }

  Serial.print("Total scan handling ms: ");
  Serial.println(millis() - scanStartMs);
}

bool isDuplicateScan(String uid) {
  return uid == lastUID && millis() - lastScanMillis < scanCooldown;
}

String getUID() {
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidString += "0";
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();
  return uidString;
}

// ================= WIFI =================
void connectWiFi() {
  drawBackground(C_ACCENT);
  drawCard(18, 72, 204, 88, C_ACCENT, C_BG2);
  drawCenteredText("Connecting WiFi", 88, C_WHITE, 1);
  drawCenteredText(WIFI_SSID, 108, C_GRAY, 1);

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
    tft.fillRect(82, 130, 76, 18, C_BG2);
    String dots = "";
    for (int i = 0; i < tries % 4; i++) dots += ".";
    drawCenteredText(dots, 132, C_TEAL, 2);
  }

  Serial.println();
  wifiConnected = WiFi.status() == WL_CONNECTED;
  drawBadge(42, 174, 156, wifiConnected ? "WIFI CONNECTED" : "WIFI FAILED", wifiConnected ? C_GREEN_DK : C_RED_DK, C_WHITE);

  if (wifiConnected) {
    Serial.println("WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    buzz(150, 1);
  } else {
    Serial.println("WiFi connection failed. The unit will retry.");
  }

  delay(800);
}

// ================= TIME =================
void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Getting time");

  struct tm timeinfo;
  unsigned long startTime = millis();
  while (!getLocalTime(&timeinfo) && millis() - startTime < 15000) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  if (getLocalTime(&timeinfo)) {
    Serial.println("Time Ready");
  } else {
    Serial.println("Time not ready. Fallback timestamp will be used until NTP works.");
  }
}

String twoDigits(int value) {
  return value < 10 ? "0" + String(value) : String(value);
}

String getDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Unknown Date";
  return String(timeinfo.tm_year + 1900) + "-" + twoDigits(timeinfo.tm_mon + 1) + "-" + twoDigits(timeinfo.tm_mday);
}

String getTimeText() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Unknown Time";
  return twoDigits(timeinfo.tm_hour) + ":" + twoDigits(timeinfo.tm_min) + ":" + twoDigits(timeinfo.tm_sec);
}

String getDisplayDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--/--";
  return twoDigits(timeinfo.tm_mday) + "/" + twoDigits(timeinfo.tm_mon + 1) + "/" + String((timeinfo.tm_year + 1900) % 100);
}

String getDisplayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--";
  return twoDigits(timeinfo.tm_hour) + ":" + twoDigits(timeinfo.tm_min);
}

unsigned long getUnixTimestamp() {
  time_t now;
  time(&now);
  if (now < 100000) return millis() / 1000;
  return (unsigned long)now;
}

void setServerTimestamp(FirebaseJson &json) {
  json.set("serverTimestamp/.sv", "timestamp");
}

// ================= FIREBASE =================
void setupFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    firebaseReady = false;
    Serial.println("Firebase skipped because WiFi is offline.");
    return;
  }

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  config.timeout.serverResponse = firebaseServerTimeoutMs;
  config.timeout.socketConnection = firebaseSocketTimeoutMs;
  config.timeout.sslHandshake = firebaseSslTimeoutMs;
  config.timeout.wifiReconnect = firebaseWifiReconnectMs;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  String statusPath = String("/school/") + SCHOOL_ID + "/status";
  String totalPath = String("/school/") + SCHOOL_ID + "/totalStudents";

  firebaseReady = Firebase.RTDB.setString(&fbdo, statusPath, "ACTIVE");
  if (firebaseReady) {
    Firebase.RTDB.setInt(&fbdo, totalPath, TOTAL_STUDENTS);
    Serial.println("Firebase Ready");
  } else {
    printFirebaseError("Firebase init status");
  }
}

void resetDailySchoolSessionIfNeeded() {
  if (bootSessionResetDone) return;

  presentCount = 0;
  lastStudentName = "";
  lastStudentStatus = "";
  lastScanTime = "";
  lastScanType = "";

  if (!writeSecondaryFirebaseRecords || !firebaseReady) {
    bootSessionResetDone = true;
    return;
  }

  String today = getDate();
  String basePath = String("/school/") + SCHOOL_ID;
  String datePath = basePath + "/sessionDate";
  String countPath = basePath + "/presentCount";
  String lastPath = basePath + "/lastActivity";

  bool dateOk = Firebase.RTDB.setString(&fbdo, datePath, today);
  bool countOk = Firebase.RTDB.setInt(&fbdo, countPath, 0);
  bool lastOk = Firebase.RTDB.setString(&fbdo, lastPath, "Ready");

  if (dateOk && countOk && lastOk) {
    bootSessionResetDone = true;
    Serial.println("Clean gate session started. Attendance counter reset to 0.");
  } else {
    printFirebaseError("reset gate session");
  }
}

void syncCountFromFirebase() {
  if (!writeSecondaryFirebaseRecords || !firebaseReady) return;
  String basePath = String("/school/") + SCHOOL_ID;
  String datePath = basePath + "/sessionDate";
  String countPath = basePath + "/presentCount";
  String today = getDate();

  if (Firebase.RTDB.getString(&fbdo, datePath) && fbdo.stringData() != today) {
    presentCount = 0;
    Firebase.RTDB.setString(&fbdo, datePath, today);
    Firebase.RTDB.setInt(&fbdo, countPath, 0);
    return;
  }

  if (Firebase.RTDB.getInt(&fbdo, countPath)) {
    presentCount = fbdo.intData();
    if (presentCount < 0) presentCount = 0;
    if (presentCount > totalStudents) presentCount = totalStudents;
  }
}

String getStudentName(String uid) {
  if (uid == "C0FA565F") return "Anas";
  if (uid == "1132C30E") return "Salah";
  if (uid == "1132C3E0") return "Salah";
  if (uid == "71E2344C") return "Mohamed";
  if (uid == "210E160E") return "Abd-Elrashid";

  String path = String("/students/") + uid + "/name";
  if (firebaseReady && Firebase.RTDB.getString(&fbdo, path)) {
    String name = fbdo.stringData();
    if (name.length() > 0) return name;
  }

  return "Unknown";
}

String getCachedStatus(String uid) {
  if (uid == "C0FA565F") return validCachedStatus(cachedStatusAnas, cachedStatusAnasMs);
  if (uid == "1132C30E" || uid == "1132C3E0") return validCachedStatus(cachedStatusSalah, cachedStatusSalahMs);
  if (uid == "71E2344C") return validCachedStatus(cachedStatusMohamed, cachedStatusMohamedMs);
  if (uid == "210E160E") return validCachedStatus(cachedStatusAbdElrashid, cachedStatusAbdElrashidMs);
  return "";
}

String validCachedStatus(String status, unsigned long cachedAtMs) {
  if (status.length() == 0) return "";
  if (millis() - cachedAtMs > statusCacheTtlMs) return "";
  return status;
}

void setCachedStatus(String uid, String status) {
  unsigned long nowMs = millis();
  if (uid == "C0FA565F") {
    cachedStatusAnas = status;
    cachedStatusAnasMs = nowMs;
  }
  if (uid == "1132C30E" || uid == "1132C3E0") {
    cachedStatusSalah = status;
    cachedStatusSalahMs = nowMs;
  }
  if (uid == "71E2344C") {
    cachedStatusMohamed = status;
    cachedStatusMohamedMs = nowMs;
  }
  if (uid == "210E160E") {
    cachedStatusAbdElrashid = status;
    cachedStatusAbdElrashidMs = nowMs;
  }
}

String getStudentInfo(String uid, String field) {
  if (!firebaseReady) return "";
  String path = String("/students/") + uid + "/" + field;
  if (Firebase.RTDB.getString(&fbdo, path)) return fbdo.stringData();
  return "";
}

String getCurrentStatus(String uid) {
  String path = String("/students/") + uid + "/currentStatus";
  if (firebaseReady && Firebase.RTDB.getString(&fbdo, path)) {
    String status = fbdo.stringData();
    if (status.length() > 0) {
      setCachedStatus(uid, status);
      return status;
    }
  }

  String legacyPath = String("/Students/") + uid + "/status";
  if (firebaseReady && Firebase.RTDB.getString(&fbdo, legacyPath)) {
    String legacy = fbdo.stringData();
    if (legacy == "IN") {
      setCachedStatus(uid, "AT_SCHOOL");
      return "AT_SCHOOL";
    }
    if (legacy == "OUT") {
      setCachedStatus(uid, "LEFT_SCHOOL");
      return "LEFT_SCHOOL";
    }
  }

  String cachedStatus = getCachedStatus(uid);
  if (cachedStatus.length() > 0) return cachedStatus;

  return "UNKNOWN";
}

String getEffectiveStatus(String uid, String status) {
  if (isSafetyRouteStatus(status)) {
    String lastDatePath = String("/students/") + uid + "/lastDate";
    if (firebaseReady && Firebase.RTDB.getString(&fbdo, lastDatePath)) {
      String lastDate = fbdo.stringData();
      if (lastDate.length() > 0 && lastDate != getDate()) {
        setCachedStatus(uid, "AT_HOME");
        return "AT_HOME";
      }
    }
  }

  if (status != "ALERT_DELAY") return status;

  String path = String("/students/") + uid + "/statusBeforeAlert";
  if (firebaseReady && Firebase.RTDB.getString(&fbdo, path)) {
    String beforeAlert = fbdo.stringData();
    if (beforeAlert.length() > 0) {
      setCachedStatus(uid, beforeAlert);
      return beforeAlert;
    }
  }

  return "UNKNOWN";
}

bool isSafetyRouteStatus(String status) {
  return status == "IN_BUS_TO_SCHOOL" ||
         status == "LEFT_BUS_AT_SCHOOL" ||
         status == "LEFT_SCHOOL" ||
         status == "IN_BUS_TO_HOME" ||
         status == "ALERT_DELAY";
}

void determineGateEvent(String currentStatus) {
  eventType = "";
  newStatus = "";
  invalidReason = "";

  if (currentStatus == "AT_SCHOOL") {
    eventType = "Left School";
    newStatus = "LEFT_SCHOOL";
    return;
  }

  if (currentStatus == "LEFT_BUS_AT_SCHOOL") {
    eventType = "Entered School";
    newStatus = "AT_SCHOOL";
    return;
  }

  if (currentStatus == "IN_BUS_TO_SCHOOL") {
    invalidReason = "Scan bus exit first. Student is still marked in bus.";
    return;
  }

  if (currentStatus == "AT_HOME" || currentStatus == "ARRIVED_HOME" || currentStatus == "UNKNOWN") {
    invalidReason = "Morning route must start with bus scan.";
    return;
  }

  if (currentStatus == "LEFT_SCHOOL") {
    invalidReason = "Already left school. Waiting for bus scan.";
    return;
  }

  if (currentStatus == "IN_BUS_TO_HOME") {
    invalidReason = "Already in bus to home.";
    return;
  }

  invalidReason = "Unsupported gate transition from " + currentStatus;
}

void updateSchoolCounters(bool isEntry) {
  if (isEntry) {
    presentCount++;
    if (presentCount > totalStudents) presentCount = totalStudents;
  } else {
    presentCount--;
    if (presentCount < 0) presentCount = 0;
  }

  if (!writeSecondaryFirebaseRecords || !firebaseReady) return;
  String basePath = String("/school/") + SCHOOL_ID;
  String countPath = basePath + "/presentCount";
  String lastPath = basePath + "/lastActivity";
  String datePath = basePath + "/sessionDate";
  Firebase.RTDB.setString(&fbdo, datePath, getDate());
  Firebase.RTDB.setInt(&fbdo, countPath, presentCount);
  Firebase.RTDB.setString(&fbdo, lastPath, getTimeText());
}

bool updateStudentStatus(String uid, String studentName, String dateText, String timeText, unsigned long timestamp) {
  if (!firebaseReady) return false;

  FirebaseJson json;
  json.set("uid", uid);
  json.set("name", studentName);
  json.set("studentName", studentName);
  json.set("currentStatus", newStatus);
  json.set("lastEvent", eventType);
  json.set("lastLocation", DEVICE_LOCATION);
  json.set("lastDate", dateText);
  json.set("lastTime", timeText);
  json.set("lastTimestamp", timestamp);
  setServerTimestamp(json);
  json.set("statusBeforeAlert", "");
  json.set("deviceName", DEVICE_NAME);

  String path = String("/students/") + uid;
  if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
    Serial.println("Student status updated.");
    return true;
  }

  printFirebaseError("update student");
  return false;
}

bool saveEventLog(String uid, String studentName, String dateText, String timeText, unsigned long timestamp) {
  if (!firebaseReady) return false;

  FirebaseJson json;
  json.set("studentId", uid);
  json.set("studentName", studentName);
  json.set("uid", uid);
  json.set("location", DEVICE_LOCATION);
  json.set("event", eventType);
  json.set("status", newStatus);
  json.set("date", dateText);
  json.set("time", timeText);
  json.set("timestamp", timestamp);
  setServerTimestamp(json);
  json.set("deviceName", DEVICE_NAME);

  String path = "/events";
  if (Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    Serial.println("Event saved.");
    return true;
  }

  printFirebaseError("save event");
  return false;
}

bool createNotification(String uid, String studentName, String dateText, String timeText, unsigned long timestamp) {
  if (!firebaseReady) return false;

  FirebaseJson json;
  json.set("uid", uid);
  json.set("studentId", uid);
  json.set("studentName", studentName);
  json.set("event", eventType);
  json.set("status", newStatus);
  json.set("location", DEVICE_LOCATION);
  json.set("message_en", getMessageEn(studentName, eventType));
  json.set("message_ar", getMessageAr(studentName, eventType));
  json.set("date", dateText);
  json.set("time", timeText);
  json.set("timestamp", timestamp);
  setServerTimestamp(json);
  json.set("isRead", false);
  json.set("type", "normal");
  json.set("deviceName", DEVICE_NAME);

  String path = "/notifications";
  if (Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    Serial.println("Notification created.");
    return true;
  }

  printFirebaseError("create notification");
  return false;
}

void updateLegacyPaths(String uid, String studentName, String dateText, String timeText, unsigned long timestamp) {
  if (!firebaseReady) return;

  String legacyStudentPath = String("/Students/") + uid;
  Firebase.RTDB.setString(&fbdo, legacyStudentPath + "/name", studentName);
  Firebase.RTDB.setString(&fbdo, legacyStudentPath + "/status", newStatus == "AT_SCHOOL" ? "IN" : "OUT");
  Firebase.RTDB.setString(&fbdo, legacyStudentPath + "/last_time", dateText + " " + timeText);
  Firebase.RTDB.setString(&fbdo, legacyStudentPath + "/location", getLocationLabel());

  FirebaseJson logJson;
  logJson.set("uid", uid);
  logJson.set("name", studentName);
  logJson.set("event", eventType);
  logJson.set("location", getLocationLabel());
  logJson.set("timestamp", timestamp);
  setServerTimestamp(logJson);

  String logPath = "/Logs";
  Firebase.RTDB.pushJSON(&fbdo, logPath, &logJson);
}

void updateDeviceHealth(unsigned long timestamp) {
  if (!firebaseReady) return;

  FirebaseJson json;
  json.set("location", DEVICE_LOCATION);
  json.set("label", DEVICE_NAME);
  json.set("lastSeen", timestamp);
  setServerTimestamp(json);
  json.set("online", true);
  json.set("status", "ONLINE");

  String path = String("/devices/") + DEVICE_LOCATION;
  if (!Firebase.RTDB.updateNode(&fbdo, path, &json)) {
    printFirebaseError("update device health");
  }
}

void saveUnknownScan(String uid) {
  if (!firebaseReady) return;

  FirebaseJson json;
  json.set("uid", uid);
  json.set("location", DEVICE_LOCATION);
  json.set("event", "Unknown Card");
  json.set("date", getDate());
  json.set("time", getTimeText());
  json.set("timestamp", getUnixTimestamp());
  setServerTimestamp(json);
  json.set("deviceName", DEVICE_NAME);

  String path = "/unknown_scans";
  if (!Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    printFirebaseError("save unknown scan");
  }
}

void saveInvalidScan(String uid, String studentName, String currentStatus, String reason) {
  if (!firebaseReady) return;

  FirebaseJson json;
  json.set("uid", uid);
  json.set("studentName", studentName);
  json.set("location", DEVICE_LOCATION);
  json.set("currentStatus", currentStatus);
  json.set("reason", reason);
  json.set("timestamp", getUnixTimestamp());
  setServerTimestamp(json);
  json.set("deviceName", DEVICE_NAME);

  String path = "/invalid_scans";
  if (!Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    printFirebaseError("save invalid scan");
  }
}

String getLocationLabel() {
  return "School Gate";
}

String getMessageEn(String studentName, String eventName) {
  if (eventName == "Entered School") return studentName + " entered school.";
  if (eventName == "Left School") return studentName + " left school.";
  return studentName + " " + eventName;
}

String getMessageAr(String studentName, String eventName) {
  if (eventName == "Entered School") return studentName + " دخل المدرسة.";
  if (eventName == "Left School") return studentName + " خرج من المدرسة.";
  return studentName + " " + eventName;
}

void printFirebaseError(String action) {
  Serial.print("Firebase failed - ");
  Serial.print(action);
  Serial.print(": ");
  Serial.println(fbdo.errorReason());
}

// ================= SERIAL PRINTS =================
void printEvent(String uid, String studentName, String currentStatus, String dateText, String timeText) {
  Serial.println("------------------------------");
  Serial.print("UID: ");
  Serial.println(uid);
  Serial.print("Student Name: ");
  Serial.println(studentName);
  Serial.print("Location: ");
  Serial.println(DEVICE_LOCATION);
  Serial.print("Previous Status: ");
  Serial.println(currentStatus);
  Serial.print("Event: ");
  Serial.println(eventType);
  Serial.print("New Status: ");
  Serial.println(newStatus);
  Serial.print("Date: ");
  Serial.println(dateText);
  Serial.print("Time: ");
  Serial.println(timeText);
  Serial.println("------------------------------");
}

void printUnknownCard(String uid) {
  Serial.println("------------------------------");
  Serial.println("Unknown Card Detected");
  Serial.print("UID: ");
  Serial.println(uid);
  Serial.println("------------------------------");
}

void printInvalidScan(String uid, String studentName, String currentStatus, String reason) {
  Serial.println("------------------------------");
  Serial.println("Invalid transition ignored");
  Serial.print("UID: ");
  Serial.println(uid);
  Serial.print("Student Name: ");
  Serial.println(studentName);
  Serial.print("Current Status: ");
  Serial.println(currentStatus);
  Serial.print("Reason: ");
  Serial.println(reason);
  Serial.println("------------------------------");
}

// ================= TFT SCREENS =================
void drawBootScreen() {
  drawBackground(C_ACCENT);

  tft.setTextColor(C_TEAL);
  tft.setTextSize(1);
  tft.setCursor(74, 18);
  tft.print("IoT SAFETY");

  drawCard(48, 38, 144, 72, C_ACCENT, C_BG2);
  drawIcon_School(100, 52, C_TEAL);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(24, 124);
  tft.print("SMART SCHOOL");
  tft.setCursor(48, 148);
  tft.print("GATE UNIT");

  drawBadge(48, 180, 144, "RFID + FIREBASE", C_BG3, C_TEAL);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(78, 214);
  tft.print("v3.1  ");
  tft.print(SCHOOL_ID);

  delay(1600);
}

void drawScreen_Idle() {
  drawBackground(C_BG3);
  drawTopBar("SMART GATE", getDisplayDate() + "  " + getDisplayTime(), C_TEAL);

  int absentCount = max(0, totalStudents - presentCount);
  float pct = totalStudents > 0 ? (float)presentCount / totalStudents : 0;

  drawCard(10, 48, 220, 42, C_ACCENT, C_BG2);
  drawIcon_School(18, 52, C_TEAL);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(66, 56);
  tft.print("Ready for RFID scan");
  tft.setTextColor(C_GRAY);
  tft.setCursor(66, 70);
  tft.print("Place card near reader");
  drawBadge(168, 61, 52, "LIVE", C_ACCENT, C_WHITE);

  drawStatBox(10, 100, 104, 58, "PRESENT", presentCount, C_GREEN);
  drawStatBox(126, 100, 104, 58, "ABSENT", absentCount, C_RED);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(12, 168);
  tft.print("Attendance");
  tft.setTextColor(C_WHITE);
  tft.setCursor(84, 168);
  tft.print(presentCount);
  tft.print("/");
  tft.print(totalStudents);
  tft.setTextColor(C_TEAL);
  tft.setCursor(184, 168);
  tft.print((int)(pct * 100));
  tft.print("%");

  drawProgressBar(12, 180, W - 24, 8, pct, C_GREEN, C_GRAY_DK);

  drawCard(10, 196, 220, 26, C_GRAY_DK, C_BG2);

  if (lastStudentName != "") {
    uint16_t scanCol = lastScanType == "IN" ? C_GREEN : C_BLUE_LIGHT;
    tft.fillRoundRect(18, 203, 26, 12, 4, scanCol);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(23, 205);
    tft.print(lastScanType);

    tft.setTextColor(C_WHITE);
    tft.setCursor(52, 202);
    String name = lastStudentName;
    if (name.length() > 15) name = name.substring(0, 15);
    tft.print(name);

    tft.setTextColor(C_TEAL);
    tft.setCursor(176, 202);
    tft.print(lastScanTime);
  } else {
    tft.setTextColor(C_GRAY);
    tft.setCursor(18, 205);
    tft.print("Last scan: none today");
  }

  drawStatusBar();
}

void drawScreen_Entry(String name, String grade) {
  drawResultScreen("ENTRY OK", "AT SCHOOL", name, grade, C_GREEN, true);
}

void drawScreen_Exit(String name, String grade) {
  drawResultScreen("EXIT OK", "LEFT SCHOOL", name, grade, C_BLUE_LIGHT, false);
}

void drawScreen_Warning(String msg, String studentName) {
  drawBackground(C_AMBER_DK);
  tft.fillRect(0, 0, W, 86, C_AMBER_DK);

  drawIcon_Warning(92, 8, C_AMBER);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(48, 66);
  tft.print("CHECK SCAN");

  drawCard(10, 96, 220, 86, C_AMBER, C_BG2);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(20, 106);
  tft.print("Student:");

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 120);
  String dn = studentName;
  if (dn.length() > 13) dn = dn.substring(0, 13);
  tft.print(dn);

  drawWrappedText(msg, 20, 148, 31, 11, C_WHITE);

  drawBadge(24, 194, 192, "TRANSITION NOT SAVED", C_AMBER_DK, C_WHITE);

  drawFooterLine(C_AMBER);
}

void drawScreen_Error(String msg) {
  drawBackground(C_RED_DK);
  tft.fillRect(0, 0, W, 84, C_RED_DK);

  tft.fillCircle(120, 35, 26, C_WHITE);
  tft.drawLine(108, 24, 132, 48, C_RED_DK);
  tft.drawLine(132, 24, 108, 48, C_RED_DK);
  tft.drawLine(109, 24, 133, 48, C_RED_DK);
  tft.drawLine(133, 24, 109, 48, C_RED_DK);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(72, 64);
  tft.print("ERROR");

  drawCard(10, 98, 220, 92, C_RED, C_BG2);
  drawWrappedText(msg, 20, 112, 31, 13, C_WHITE);

  drawBadge(24, 200, 192, "SCAN IGNORED", C_RED_DK, C_WHITE);
  drawFooterLine(C_RED);
}

// ================= DRAWING HELPERS =================
void drawBackground(uint16_t topColor) {
  for (int y = 0; y < H; y++) {
    uint8_t blue = map(y, 0, H, 36, 4);
    uint8_t green = map(y, 0, H, 18, 2);
    tft.drawFastHLine(0, y, W, tft.color565(0, green, blue));
  }
  tft.fillRect(0, 0, W, 4, topColor);
}

void drawCard(int x, int y, int width, int height, uint16_t border, uint16_t fill) {
  tft.fillRoundRect(x + 2, y + 3, width, height, 8, C_BG);
  tft.fillRoundRect(x, y, width, height, 8, fill);
  tft.drawRoundRect(x, y, width, height, 8, border);
}

void drawBadge(int x, int y, int width, String text, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(x, y, width, 18, 9, bg);
  tft.drawRoundRect(x, y, width, 18, 9, fg & 0x7BEF);

  tft.setTextColor(fg);
  tft.setTextSize(1);
  int textWidth = text.length() * 6;
  int textX = x + (width - textWidth) / 2;
  if (textX < x + 4) textX = x + 4;
  tft.setCursor(textX, y + 6);
  tft.print(text);
}

void drawTopBar(String title, String subtitle, uint16_t accent) {
  tft.fillRect(0, 0, W, 42, C_HEADER_BG);
  tft.drawFastHLine(0, 42, W, accent);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(12, 8);
  tft.print(title);

  tft.setTextColor(C_GRAY);
  tft.setCursor(12, 23);
  tft.print(subtitle);

  drawConnectionChip(166, 7, "WF", wifiConnected, C_GREEN);
  drawConnectionChip(166, 23, "FB", firebaseReady, C_TEAL);
}

void drawConnectionChip(int x, int y, String label, bool ok, uint16_t okColor) {
  uint16_t color = ok ? okColor : C_RED;
  tft.fillRoundRect(x, y, 54, 13, 6, C_BG2);
  tft.fillCircle(x + 8, y + 6, 3, color);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(x + 16, y + 3);
  tft.print(label);
}

void drawResultScreen(String title, String statusText, String name, String grade, uint16_t accent, bool isEntry) {
  drawBackground(accent);

  uint16_t headerColor = isEntry ? C_GREEN_DK : C_BLUE_MED;
  tft.fillRect(0, 0, W, 88, headerColor);
  tft.drawFastHLine(0, 88, W, accent);

  if (isEntry) {
    drawIcon_Check(96, 10, C_WHITE);
  } else {
    drawIcon_Exit(96, 10, C_WHITE);
  }

  drawCenteredText(title, 66, C_WHITE, 2);

  drawCard(10, 100, 220, 72, accent, C_BG2);
  drawIcon_Person(22, 120, accent, isEntry);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(62, 112);
  tft.print("Student");

  String displayName = name;
  if (displayName.length() > 12) displayName = displayName.substring(0, 12);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(62, 128);
  tft.print(displayName);

  tft.setTextColor(C_TEAL);
  tft.setTextSize(1);
  tft.setCursor(62, 154);
  if (grade != "") {
    tft.print("Grade ");
    tft.print(grade);
  } else {
    tft.print(DEVICE_NAME);
  }

  drawBadge(18, 184, 204, statusText, isEntry ? C_GREEN_DK : C_BLUE_MED, C_WHITE);
  drawFooterLine(accent);
}

void drawFooterLine(uint16_t accent) {
  tft.fillRect(0, H - 22, W, 22, C_HEADER_BG);
  tft.drawFastHLine(0, H - 23, W, accent);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(8, H - 15);
  tft.print("Present ");

  tft.setTextColor(accent);
  tft.print(presentCount);

  tft.setTextColor(C_GRAY);
  tft.print("/");
  tft.print(totalStudents);

  String nowText = lastScanTime != "" ? lastScanTime : getDisplayTime();
  tft.setTextColor(C_TEAL);
  int tw = nowText.length() * 6;
  tft.setCursor(W - tw - 8, H - 15);
  tft.print(nowText);
}

void drawProgressBar(int x, int y, int width, int height, float pct, uint16_t col, uint16_t bg) {
  if (pct < 0) pct = 0;
  if (pct > 1) pct = 1;

  tft.fillRoundRect(x, y, width, height, height / 2, bg);
  int fillW = (int)(width * pct);
  if (fillW > 2) tft.fillRoundRect(x, y, fillW, height, height / 2, col);
  if (fillW > 6) tft.drawFastHLine(x + 2, y + 2, fillW - 4, C_WHITE);
}

void drawStatBox(int x, int y, int width, int height, String label, int value, uint16_t col) {
  drawCard(x, y, width, height, col, C_BG2);

  tft.setTextColor(col);
  tft.setTextSize(1);
  int lw = label.length() * 6;
  tft.setCursor(x + (width - lw) / 2, y + 8);
  tft.print(label);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  String vs = String(value);
  int vw = vs.length() * 12;
  tft.setCursor(x + (width - vw) / 2, y + 25);
  tft.print(vs);

  float pct = totalStudents > 0 ? (float)value / totalStudents * 100 : 0;
  tft.setTextColor(col);
  tft.setTextSize(1);
  String ps = String((int)pct) + "%";
  int pw = ps.length() * 6;
  tft.setCursor(x + (width - pw) / 2, y + height - 14);
  tft.print(ps);
}

void drawStatusBar() {
  tft.fillRect(0, H - 18, W, 18, C_HEADER_BG);
  tft.drawFastHLine(0, H - 19, W, C_GRAY_DK);

  tft.setTextColor(wifiConnected ? C_GREEN : C_RED);
  tft.setTextSize(1);
  tft.setCursor(4, H - 12);
  tft.print(wifiConnected ? "WiFi" : "NoWF");

  tft.setTextColor(C_GRAY_DK);
  tft.setCursor(36, H - 12);
  tft.print("|");

  tft.setTextColor(firebaseReady ? C_TEAL : C_AMBER);
  tft.setCursor(44, H - 12);
  tft.print(firebaseReady ? "FB:OK" : "FB:--");

  tft.setTextColor(C_GRAY_DK);
  tft.setCursor(86, H - 12);
  tft.print("|");

  tft.setTextColor(C_GREEN);
  tft.setCursor(94, H - 12);
  tft.print(presentCount);
  tft.setTextColor(C_GRAY);
  tft.print(" present");

  String nowText = getDisplayTime();
  tft.setTextColor(C_TEAL);
  int tw = nowText.length() * 6;
  tft.setCursor(W - tw - 4, H - 12);
  tft.print(nowText);
}

void drawCenteredText(String txt, int y, uint16_t col, uint8_t size) {
  tft.setTextColor(col);
  tft.setTextSize(size);
  int textWidth = txt.length() * 6 * size;
  int x = (W - textWidth) / 2;
  if (x < 2) x = 2;
  tft.setCursor(x, y);
  tft.print(txt);
}

void drawDivider(int y, uint16_t col) {
  tft.drawFastHLine(0, y, W, col);
  tft.drawFastHLine(0, y + 1, W, col & 0x7BEF);
}

void drawWrappedText(String msg, int x, int y, int lineLen, int lineHeight, uint16_t col) {
  tft.setTextColor(col);
  tft.setTextSize(1);

  int lineStart = 0;
  int currentY = y;
  while (lineStart < msg.length() && currentY < H - 22) {
    int newlineIndex = msg.indexOf('\n', lineStart);
    int nextBreak = lineStart + lineLen;
    if (newlineIndex >= 0 && newlineIndex < nextBreak) nextBreak = newlineIndex;
    if (nextBreak > msg.length()) nextBreak = msg.length();

    String line = msg.substring(lineStart, nextBreak);
    tft.setCursor(x, currentY);
    tft.print(line);

    currentY += lineHeight;
    lineStart = nextBreak;
    if (lineStart < msg.length() && msg.charAt(lineStart) == '\n') lineStart++;
  }
}

void drawIcon_School(int x, int y, uint16_t col) {
  tft.fillTriangle(x + 20, y, x, y + 16, x + 40, y + 16, col);
  tft.fillRect(x + 4, y + 16, 32, 28, col);
  tft.fillRect(x + 14, y + 30, 12, 14, C_BG);
  tft.fillRect(x + 6, y + 20, 8, 6, C_BG);
  tft.fillRect(x + 26, y + 20, 8, 6, C_BG);
}

void drawIcon_Check(int x, int y, uint16_t col) {
  tft.fillCircle(x + 23, y + 23, 24, col);
  tft.fillCircle(x + 23, y + 23, 19, C_GREEN_DK);
  tft.drawLine(x + 11, y + 23, x + 20, y + 32, col);
  tft.drawLine(x + 12, y + 23, x + 21, y + 32, col);
  tft.drawLine(x + 20, y + 32, x + 36, y + 14, col);
  tft.drawLine(x + 21, y + 32, x + 37, y + 14, col);
}

void drawIcon_Exit(int x, int y, uint16_t col) {
  tft.fillCircle(x + 23, y + 23, 24, col);
  tft.fillCircle(x + 23, y + 23, 19, C_BLUE_MED);
  tft.fillRect(x + 10, y + 19, 20, 8, col);
  tft.fillTriangle(x + 28, y + 14, x + 28, y + 32, x + 42, y + 23, col);
}

void drawIcon_Warning(int x, int y, uint16_t col) {
  tft.fillTriangle(x + 23, y + 2, x + 2, y + 44, x + 44, y + 44, col);
  tft.fillTriangle(x + 23, y + 8, x + 7, y + 40, x + 39, y + 40, C_BG);
  tft.fillRect(x + 20, y + 16, 6, 14, col);
  tft.fillRect(x + 20, y + 33, 6, 5, col);
}

void drawIcon_Person(int x, int y, uint16_t col, bool withBag) {
  tft.fillCircle(x + 10, y + 6, 7, col);
  tft.fillRoundRect(x + 2, y + 15, 16, 16, 4, col);
  if (withBag) {
    tft.fillRoundRect(x + 16, y + 14, 8, 10, 2, C_TEAL);
    tft.drawFastHLine(x + 17, y + 13, 6, C_TEAL);
  }
}

void buzz(int ms, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
    if (times > 1) delay(80);
  }
}

void setIndicators(bool greenOn, bool redOn) {
  digitalWrite(GREEN_LED_PIN, greenOn ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, redOn ? HIGH : LOW);
}

void successIndicator(int ms, int times) {
  setIndicators(true, false);
  buzz(ms, times);
}

void warningIndicator() {
  setIndicators(false, true);
  buzz(150, 2);
}

void errorIndicator() {
  setIndicators(false, true);
  buzz(100, 3);
}

void bootIndicatorTest() {
  setIndicators(true, false);
  buzz(70, 1);
  delay(80);
  setIndicators(false, true);
  buzz(70, 1);
  delay(80);
  setIndicators(false, false);
}
