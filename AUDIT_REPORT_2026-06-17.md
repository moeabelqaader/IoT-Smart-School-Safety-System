# Smart School Safety - Full Technical Audit Report

Audit date: 2026-06-17  
Primary Flutter project: `C:\Projects\smart_school_safety`  
ESP32 firmware folder: `C:\Projects\ESP32_SEPARATED_CODES`  
Auditor role: Senior Flutter Engineer / Senior Software Auditor

## 1. Executive Summary

The project was audited end to end across the Flutter application, Firebase Realtime Database rules, Firebase Cloud Functions, local backend tooling, Android configuration, tests, and both ESP32 firmware files.

The Flutter application is currently buildable and analyzable with no Dart analyzer issues. Unit tests pass. The Android debug APK builds successfully. Firebase Cloud Functions JavaScript syntax is valid, and the integration checker confirms the expected Flutter, Firebase, school gate ESP32, and bus ESP32 paths/events/notifications.

Safe fixes were applied during the audit:

- Added missing Firebase Realtime Database rules for `/notifications`.
- Corrected the ESP32 bus duplicate-scan display message from 5 seconds to 3 seconds to match the actual cooldown.
- Verified previously applied fixes for parent/student isolation, FCM subscription disposal, status transition tests, Flutter deprecations, and ESP32 latency instrumentation.

Remaining high-risk items are security and deployment related: Firebase rules still allow broad authenticated writes to core nodes, and live Wi-Fi/Firebase secrets are stored in ESP32 source files. These were not removed automatically because doing so would break the current graduation demo unless credentials and device authentication are redesigned.

## 2. Scope Reviewed

Reviewed source areas:

| Area | Path |
|---|---|
| Flutter source | `C:\Projects\smart_school_safety\lib` |
| Flutter tests | `C:\Projects\smart_school_safety\test` |
| App dependencies | `C:\Projects\smart_school_safety\pubspec.yaml` and `pubspec.lock` |
| Firebase rules | `C:\Projects\smart_school_safety\firebase_database_rules.json` |
| Firebase Functions | `C:\Projects\smart_school_safety\functions\index.js` |
| Integration tooling | `C:\Projects\smart_school_safety\tools` |
| Android config | `C:\Projects\smart_school_safety\android` |
| Bus ESP32 firmware | `C:\Projects\ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino` |
| School gate ESP32 firmware | `C:\Projects\ESP32_SEPARATED_CODES\school_gate_reader\school_gate_reader.ino` |

## 3. Detected Issues and Fix Status

| Severity | Issue | Affected file/function | Root cause | Fix status |
|---|---|---|---|---|
| High | Missing Firebase rules for `/notifications` caused Flutter notification-record writes to fail if rules are deployed. | `firebase_database_rules.json`, node `/notifications` | Flutter and ESP write notification records, but the rules file did not define that node. | Fixed. Added `/notifications` index/read/write rules matching student ownership pattern. |
| High | Broad authenticated writes are allowed to critical Firebase nodes. | `firebase_database_rules.json`, `/students`, `/events`, `/alerts`, `/devices`, `/Students`, `/Logs` | Prototype rules use `.write: auth != null`, which lets any signed-in app user write sensitive system data. | Human review required. Not tightened automatically to avoid breaking current ESP/demo flow. |
| High | Real secrets are committed in ESP firmware. | `bus_reader.ino`, `school_gate_reader.ino` | Wi-Fi SSID/password and Firebase Database Secret are hardcoded for easy flashing. | Human review required. Rotate secrets before public sharing and move to private config. |
| High | Real scan-to-notification delay cannot be proven from code alone. | ESP firmware, `SafetyMonitorService`, Firebase Functions | The system now logs ESP timing, but no complete serial/Firebase/mobile timestamp set is available for the reported 3-minute delay. | Measurement required. See Section 8. |
| High | Parent/student access could be wrong if legacy parent data overrides modern links. | `lib\services\database_service.dart`, `_mergeParent`; `lib\models\parent_model.dart` | Legacy `Parents/{id}` data can contain older `child_id`; mixed data can show wrong linked students if not normalized/prioritized. | Fixed in current code. Modern linked students are preferred, IDs are normalized, and tests cover this. |
| Medium | FCM stream subscriptions could leak if not cancelled. | `lib\services\fcm_service.dart`, `initialize`, `dispose` | Foreground/opened-message listeners were long-lived service subscriptions. | Fixed in current code. Subscriptions are stored and cancelled in `dispose`. |
| Medium | School gate Firebase setup needed retry after a failed startup. | `school_gate_reader.ino`, `loop`, `setupFirebase` | If Firebase setup failed once, the reader could stay partially offline. | Fixed in current code. Retry runs every 10 seconds when Wi-Fi is available and Firebase is not ready. |
| Medium | ESP RFID path can be slowed by Firebase reads/writes. | `bus_reader.ino`, `school_gate_reader.ino`, `processRFID`, `getStudentName`, `getCurrentStatus` | Synchronous Firebase operations run inside scan handling. Network slowness can block response. | Mitigated. Known student names are local, status cache exists, Firebase timeouts are set, screen draws before writes, and timing logs were added. Further non-blocking refactor is future work. |
| Medium | ESP Wi-Fi reconnect blocks the main loop. | `bus_reader.ino`, `school_gate_reader.ino`, `connectWiFi`, `loop` | When Wi-Fi disconnects, `connectWiFi()` can block up to 40 x 500 ms plus display delay. | Human review required. A non-blocking Wi-Fi state machine would be safer but is a larger firmware change. |
| Medium | Backend scheduled safety alerts have up to one-minute schedule granularity. | `functions\index.js`, `exports.checkSafetyTimers` | Cloud Function schedule is `every 1 minutes`. | Accepted for prototype. App-local monitor checks every 10 seconds while app is open. |
| Medium | Local app safety monitoring only runs while app process is active. | `lib\services\safety_monitor_service.dart` | Flutter timers/listeners stop when the app is terminated or restricted by OS. | Backend Cloud Functions are present for reliable background monitoring, but must be deployed and tested. |
| Medium | Android build has future Kotlin Gradle Plugin migration warning. | `android\app\build.gradle.kts`, Flutter build output | Flutter warns that explicit KGP application may fail in future Flutter versions. | Not blocking now. Plan migration later. |
| Medium | Release build uses debug signing. | `android\app\build.gradle.kts`, `buildTypes.release` | Prototype release config uses `signingConfigs.getByName("debug")`. | Human review required before production/release distribution. |
| Low | ESP bus duplicate-scan message did not match configured cooldown. | `bus_reader.ino`, `loop` duplicate scan branch | Message said wait 5 seconds while `scanCooldown` is 3000 ms. | Fixed. Message now says wait 3 seconds. |
| Low | Android local properties are machine-specific. | `android\local.properties` | Local SDK/Flutter paths are stored in a generated local file. | Do not share as source. Keep `local.properties.example` as template. |
| Low | Dependencies have newer versions available. | `pubspec.yaml`, `pubspec.lock` | Firebase packages are not at latest available versions. | Not changed automatically because current versions build successfully. Upgrade should be tested separately. |
| Low | ESP compile could not be verified locally in this environment. | Both `.ino` files | `arduino-cli`, `arduino`, and `arduino_debug.exe` were not available. | Compile in Arduino IDE on the hardware workstation. |

## 4. Fixes Applied

### 4.1 Firebase Realtime Database Rules

File: `C:\Projects\smart_school_safety\firebase_database_rules.json`

Added a new `/notifications` rules block:

- `.indexOn`: `studentId`, `uid`, `timestamp`
- query-based read for linked student ownership
- record-level read for linked student ownership
- authenticated write, consistent with the current prototype's `/events` and `/alerts` policy

Reason: Flutter writes notification records in `DatabaseService.recordNotification()`, ESP writes `/notifications`, and Cloud Functions use `/notifications/{notificationId}` as a fallback push trigger.

### 4.2 ESP32 Bus Duplicate Message

File: `C:\Projects\ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino`

Changed:

```text
Duplicate scan
Wait 5 seconds
```

to:

```text
Duplicate scan
Wait 3 seconds
```

Reason: `scanCooldown` is configured as `3000` ms.

## 5. Verification Results

Commands were run from `C:\Projects\smart_school_safety`.

| Command | Result |
|---|---|
| `dart format lib test` | Passed. 52 files checked, 0 changed. |
| `flutter analyze` | Passed. No issues found. |
| `flutter test` | Passed. 11 tests passed. |
| `node --check functions\index.js` | Passed using bundled Node runtime. |
| `node tools\integration_check.js` | Passed. Verified Flutter, Firebase Functions, school ESP32, and bus ESP32 paths/events/notifications. |
| `node -e "JSON.parse(...firebase_database_rules.json...)"` | Passed. Firebase rules file is valid JSON. |
| `flutter build apk --debug` | Passed. APK built successfully. |

APK output:

```text
C:\Projects\smart_school_safety\build\app\outputs\flutter-apk\app-debug.apk
Size: 175,577,067 bytes
Built: 2026-06-17 20:42:42
```

Build warning:

```text
Flutter warns that the Android app and firebase_database plugin apply Kotlin Gradle Plugin.
This is not a current build failure, but it may require migration in a future Flutter release.
```

ESP firmware compile verification:

```text
arduino-cli: not found
arduino: not found
arduino_debug.exe: not found
```

Result: ESP compile was not verified in this environment. Use Arduino IDE on the hardware workstation.

## 6. Data Flow Audit

### Normal scan flow

1. ESP32 reads RFID UID.
2. ESP32 resolves student name locally for known UIDs, then Firebase fallback for unknown configured students.
3. ESP32 reads current status from cache/Firebase.
4. ESP32 validates transition.
5. ESP32 updates TFT and LED/buzzer.
6. ESP32 writes:
   - `/students/{uid}`
   - `/events`
   - `/notifications`
   - `/devices/{BUS|GATE}`
   - `/buses/bus_01` or `/school/school_main`
   - legacy compatibility paths under `/Students` and `/Logs`
7. Flutter listens to linked student, events, and alerts.
8. Flutter shows local notifications while app is open.
9. Firebase Functions can send FCM push notifications from `/events`, `/notifications`, and `/alerts` if deployed.

### Safety timer flow

| Status | Timer | Expected next status | Alert type |
|---|---:|---|---|
| `IN_BUS_TO_SCHOOL` | 5 minutes | `LEFT_BUS_AT_SCHOOL` | `BUS_TO_SCHOOL_DELAY` |
| `LEFT_BUS_AT_SCHOOL` | 1 minute | `AT_SCHOOL` | `BUS_TO_GATE_DELAY` |
| `LEFT_SCHOOL` | 1 minute | `IN_BUS_TO_HOME` or `ARRIVED_HOME` | `SCHOOL_TO_BUS_DELAY` |
| `IN_BUS_TO_HOME` | 5 minutes | `ARRIVED_HOME` | `BUS_TO_HOME_DELAY` |

Flutter local monitor interval:

```text
Timer.periodic(Duration(seconds: 10))
```

Cloud Functions backend schedule:

```text
every 1 minutes
```

## 7. Security Review

| Finding | Severity | Notes |
|---|---|---|
| Firebase Database Secret in ESP files | High | Rotate before public submission. Do not publish the source with live secret. |
| Wi-Fi password in ESP files | High | Move to a private local config before sharing widely. |
| Broad authenticated writes in rules | High | Safe for prototype only. Production should restrict writes to admin/backend/device identities. |
| Debug signing for release | Medium | Fine for prototype APK, not acceptable for production release. |
| Firebase client config in `google-services.json` | Low/Expected | Normal for Firebase apps, but still avoid publishing unnecessary project artifacts. |

## 8. Performance and Delay Findings

The reported 3-minute delay cannot be proven from the current saved logs. The ESP firmware now contains timing logs:

```text
Lookup/status time ms:
Critical Firebase writes ms:
Total scan handling ms:
```

Use Serial Monitor during real scans to capture these values.

Current code configuration:

| Metric | Value | File |
|---|---:|---|
| Duplicate scan cooldown | 3000 ms | `bus_reader.ino`, `school_gate_reader.ino` |
| Firebase server response timeout | 5000 ms | both ESP files |
| Firebase socket timeout | 5000 ms | both ESP files |
| Firebase SSL handshake timeout | 5000 ms | both ESP files |
| Firebase Wi-Fi reconnect timeout | 10000 ms | both ESP files |
| ESP status cache TTL | 15 minutes | both ESP files |
| ESP Wi-Fi connect attempts | 40 attempts x 500 ms | both ESP files |
| ESP NTP wait | up to 15000 ms | both ESP files |
| TFT result screen duration | 4000 ms | both ESP files |
| TFT idle clock refresh | 30000 ms | both ESP files |
| Flutter splash delay | 180 ms | `lib\screens\splash_screen.dart` |
| Flutter safety audit loop | 10 seconds | `lib\services\safety_monitor_service.dart` |
| Cloud Functions safety schedule | every 1 minute | `functions\index.js` |

Manual measurement still required:

| Metric | Why code is insufficient | How to measure |
|---|---|---|
| RFID read response time | Depends on physical card distance, RC522 hardware, and ESP loop timing. | Use Serial Monitor and stopwatch/video from card tap to `RFID:` log. |
| Firebase upload delay | Depends on Wi-Fi and Firebase network latency. | Compare `RFID:` log time to `Critical Firebase writes ms`. |
| Mobile UI update delay | Depends on Firebase listener delivery and phone network. | Record screen while scanning and compare Firebase event time to UI update. |
| Notification delivery delay | Depends on local notification permission, app foreground/background, and FCM. | Compare ESP `Event saved` time to phone notification timestamp. |
| Overall scan-to-notification time | Requires synchronized ESP and phone timestamps. | Film Serial Monitor and phone notification together or log both timestamps. |

## 9. Human Review Required

1. Decide whether to keep prototype Firebase rules or harden them before final deployment.
2. Rotate Firebase Database Secret and Wi-Fi credentials before sending code outside the team.
3. Deploy Firebase Cloud Functions if background/closed-app alerts must be reliable.
4. Confirm Android notification permission on the test phone.
5. Compile both ESP files in Arduino IDE with the exact installed board core and libraries.
6. Perform real timing measurements using Serial Monitor and phone screen recording.
7. Consider replacing blocking ESP Wi-Fi reconnect with non-blocking reconnect logic after the demo is stable.

## 10. Testing Commands

Recommended local validation:

```powershell
cd C:\Projects\smart_school_safety
flutter pub get
dart format lib test
flutter analyze
flutter test
flutter build apk --debug
```

Firebase Functions validation:

```powershell
cd C:\Projects\smart_school_safety
node --check functions\index.js
node tools\integration_check.js
```

Firebase rules JSON validation:

```powershell
cd C:\Projects\smart_school_safety
node -e "JSON.parse(require('fs').readFileSync('firebase_database_rules.json','utf8')); console.log('valid')"
```

ESP validation:

```text
Open Arduino IDE.
Open C:\Projects\ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino.
Select ESP32 Dev Module.
Verify/Compile.
Repeat for C:\Projects\ESP32_SEPARATED_CODES\school_gate_reader\school_gate_reader.ino.
Open Serial Monitor at 115200 baud and capture timing logs during real scans.
```

## 11. Final Recommendations

1. Use `C:\Projects\smart_school_safety` as the single final Flutter project path.
2. Use `C:\Projects\ESP32_SEPARATED_CODES` as the single final ESP32 firmware path.
3. Before testing delay again, flash both ESP32 boards with the latest `.ino` files from this folder.
4. Install the latest APK from `C:\Projects\smart_school_safety\build\app\outputs\flutter-apk\app-debug.apk`.
5. During the next hardware test, capture Serial Monitor output from both ESP32 boards.
6. If delay remains above a few seconds, first inspect the printed values for `Lookup/status time ms` and `Critical Firebase writes ms`.
7. For final report/demo reliability, deploy Firebase Functions and keep the Flutter app open only as the UI, not as the only safety-monitor backend.

