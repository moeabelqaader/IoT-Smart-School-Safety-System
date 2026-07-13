# IoT Smart School Safety System

Graduation project prototype for a professional bilingual parent app connected to ESP32 RFID readers through Firebase Realtime Database.

## Run The Flutter App

1. Install Flutter and Android Studio.
2. Open a terminal in `smart_school_safety`.
3. Run:

```powershell
flutter pub get
flutter analyze
flutter test
flutter run
```

If Android Studio asks for SDK paths, copy `android/local.properties.example` to `android/local.properties` and update the paths for your machine. Do not use another person's `local.properties`.

## App Features

- Arabic and English UI with RTL/LTR switching.
- Professional parent dashboard with student summary, route progress, live events, history, profile, settings, and safety alerts.
- Quick call buttons for the bus driver, bus supervisor, and school supervisor.
- Student photo support through `students/{studentId}/photoUrl` with a clean initials fallback.
- Demo Mode for simulating bus/gate RFID scans and safety alerts without ESP32 hardware.
- Daily report cards for morning bus, school entry, school exit, and home arrival.
- ESP32 device health cards for the bus reader and gate reader.
- Local notifications for RFID scan events and delay alerts.
- Firebase Cloud Messaging token registration for parent phones.
- Cloud Functions backend for instant scan push notifications from `/events` and safety delay alerts.
- Clean Firebase service layer under `lib/services`.
- State-machine transition helper under `lib/utils/status_transition_helper.dart`.

## Firebase Paths

The app reads and writes:

- `parents/{parentId}`
- `students/{studentId}`
- `events/{eventId}`
- `alerts/{alertId}`
- `devices/{BUS|GATE}`
- `parents/{parentId}/fcmTokens/{tokenKey}`

Legacy demo paths are still supported:

- `Parents/{parentId}`
- `Students/{uid}`
- `Logs/{logId}`

## Safety Timers

- `IN_BUS_TO_SCHOOL`: 5 minutes before bus delay alert.
- `LEFT_BUS_AT_SCHOOL`: 1 minute before bus-to-gate alert.
- `LEFT_SCHOOL`: 1 minute before school-to-bus alert.
- `IN_BUS_TO_HOME`: 5 minutes before home arrival alert.

The Flutter app keeps `SafetyMonitorService` as a demo fallback. The production-style backend implementation is in `functions/index.js`; every new scan event under `/events` sends an FCM push to the linked parent phone.

## ESP32 Readers

School gate reader:

```text
ESP32_SEPARATED_CODES\school_gate_reader\school_gate_reader.ino
```

Bus reader:

```text
ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino
```

Important for Arduino IDE: open only one sketch folder at a time. Do not put the gate and bus sketches in the same folder, because Arduino compiles all `.ino` files in that folder together.

For another ESP copy, set:

```cpp
#define DEVICE_LOCATION "BUS"
```

or:

```cpp
#define DEVICE_LOCATION "GATE"
```

The ESP writes the new schema and the legacy paths so old demo screens still have data.

## Optional Student Data

You can add these fields under `students/{studentId}` from Firebase Console:

```json
{
  "photoUrl": "https://example.com/student.jpg",
  "transport": {
    "busNumber": "Bus 12",
    "driverName": "Driver Name",
    "driverPhone": "+201000000001",
    "supervisorName": "Bus Supervisor",
    "supervisorPhone": "+201000000002"
  },
  "contacts": {
    "schoolSupervisorName": "School Supervisor",
    "schoolSupervisorPhone": "+201000000003"
  }
}
```

If these fields are missing, the app uses demo-friendly placeholder data.

## Cloud Functions

Backend timer migration notes are in:

```text
firebase_cloud_functions_ready.md
functions\index.js
functions\package.json
```

To deploy:

```powershell
firebase login
firebase use <your-project-id>
firebase deploy --only database,functions
```

## Firebase Rules

Realtime Database indexes are prepared in:

```text
firebase_database_rules.json
```

Upload them from Firebase Console before the demo so `events`, `alerts`, and legacy `Logs` queries stay fast.
