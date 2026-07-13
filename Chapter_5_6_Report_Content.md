# Chapter 5 and Chapter 6 Report Content

Project title: IoT Smart School Safety System Using IoT and RFID

Source basis: This content was extracted from the actual project files, including the ESP32 bus unit code, ESP32 school gate unit code, Flutter application services and models, Firebase path definitions, Firebase Cloud Functions-ready code, and observed integration test outputs. No measured performance numbers are fabricated. Any value that requires physical measurement is marked accordingly.

## Chapter 5 - Implementation Details

### 5.1 Final Hardware Configuration

| Item | Final Value |
|---|---|
| Number of ESP32 boards | 2 ESP32 boards |
| Unit 1 | School Bus Unit |
| Unit 2 | School Gate Unit |
| Number of RFID readers | 2 RFID readers |
| RFID module type | MFRC522 / RC522 |
| TFT display type | ST7789 TFT display, 240 x 240 pixels |
| Green LED | GPIO 26 |
| Red LED | GPIO 27 |
| Buzzer | GPIO 25 |
| Communication method | Wi-Fi connected to Firebase Realtime Database |
| Time source | NTP using `pool.ntp.org` |
| Optional modules/features mentioned in code | TFT display, green/red LEDs, buzzer, Firebase Cloud Functions-ready code, Firebase Cloud Messaging-ready app code |

### 5.2 Final ESP32 Pin Connection Table

The final bus unit and school gate unit use the same hardware pin layout.

| Component | ESP32 Pin | Purpose |
|---|---:|---|
| RFID RC522 SS/SDA | GPIO 5 | RFID SPI chip select |
| RFID RC522 RST | GPIO 22 | RFID reset |
| TFT ST7789 CS | GPIO 15 | TFT chip select |
| TFT ST7789 DC | GPIO 2 | TFT data/command control |
| TFT ST7789 RST | GPIO 4 | TFT reset |
| TFT ST7789 MOSI/SDA | GPIO 23 | SPI MOSI data |
| TFT ST7789 MISO | GPIO 19 | SPI MISO, defined in code |
| TFT ST7789 SCLK/SCL | GPIO 18 | SPI clock |
| Green LED | GPIO 26 | Success indication |
| Red LED | GPIO 27 | Error or warning indication |
| Buzzer signal | GPIO 25 | Audio feedback |
| TFT VCC/BL | 3.3V | Display power and backlight |
| TFT/RFID GND | GND | Common ground |

### 5.3 Firebase Realtime Database Structure

| Firebase Node | Data Stored |
|---|---|
| `/students/{uid}` | Student profile and live status, including `uid`, `name`, `studentName`, `parentId`, `currentStatus`, `lastEvent`, `lastLocation`, `lastDate`, `lastTime`, `lastTimestamp`, `statusBeforeAlert`, contact information, transport information, `deviceName`, and `busId` when written by the bus unit. |
| `/events/{eventId}` | Full movement and alert history, including `studentId`, `studentName`, `uid`, `location`, `event`, `status`, `date`, `time`, `timestamp`, `deviceName`, optional `busId`, and optional `alertType`. |
| `/notifications/{notificationId}` | Notification records for the parent application, including student data, event, status, location, English and Arabic messages, date, time, timestamp, read state, notification type, and optional alert type. |
| `/alerts/{alertId}` | Safety delay alerts, including `studentId`, `studentName`, `alertType`, `message_en`, `message_ar`, `timestamp`, `isRead`, and `dedupeKey`. |
| `/buses/{busId}` | Bus information and live counter, including `busId`, label, capacity, `studentsOnBoard`, `lastActivity`, `lastSeen`, and status. |
| `/school/{schoolId}` | School gate state and attendance counters, including status, total students, `presentCount`, `lastActivity`, and `sessionDate`. |
| `/devices/{BUS/GATE}` | Device health data, including location, label, last seen timestamp, online flag, and status. |
| `/parents/{parentId}` | Parent account data, including name, email, phone, linked student IDs, and Firebase Cloud Messaging tokens. |
| `/unknown_scans` | Records for unrecognized RFID cards, including UID, location, event name, date, time, timestamp, and device name. |
| `/invalid_scans` | Records for invalid state transitions, including UID, student name, location, current status, reason, timestamp, and device name. |
| `/Students`, `/Logs`, `/Parents` | Legacy compatibility nodes still supported by the application/code. |
| `/device_health` | Not used as the final node name. The final code uses `/devices`. |

### 5.4 Student Movement Statuses

| Status | Meaning |
|---|---|
| `AT_HOME` | The student is at home before starting the morning route. |
| `IN_BUS_TO_SCHOOL` | The student boarded the school bus in the morning. |
| `LEFT_BUS_AT_SCHOOL` | The student left the bus at school and should enter the school gate. |
| `AT_SCHOOL` | The student entered the school gate. |
| `LEFT_SCHOOL` | The student left school after the school day. |
| `IN_BUS_TO_HOME` | The student boarded the bus to return home. |
| `ARRIVED_HOME` | The student arrived home safely. |
| `ALERT_DELAY` | A safety delay was detected. |
| `UNKNOWN` | Unknown or unsupported state. |

### 5.5 Main Event Types

| Event Type | Trigger |
|---|---|
| `Boarded Bus` | A bus scan occurs when the student is at home or at the start of the morning route. |
| `Left Bus` | A bus scan occurs while the student is `IN_BUS_TO_SCHOOL`. |
| `Entered School` | A gate scan occurs after `LEFT_BUS_AT_SCHOOL`. |
| `Left School` | A gate scan occurs while the student is `AT_SCHOOL`. |
| `Boarded Bus To Home` | A bus scan occurs after `LEFT_SCHOOL`. |
| `Arrived Home` | A bus scan occurs while the student is `IN_BUS_TO_HOME`. |
| `Safety Alert` | A safety timer is exceeded. |
| `Unknown Card` | The RFID UID is not recognized by the system. |
| Invalid scan records | Unsupported transitions are saved in `/invalid_scans`. |

## Chapter 6 - Testing and Evaluation

### 6.1 Testing Scenarios

| Test Case | Expected Result | Actual Result | Status |
|---|---|---|---|
| Valid RFID card scan | UID is recognized and student is identified | Observed with Anas card in Firebase integration test | Passed |
| Unknown RFID card scan | Error screen, red LED/buzzer, and record in `/unknown_scans` | Needs real test measurement | Pending |
| Duplicate scan protection | Same card is ignored within cooldown period | Implemented in code; needs real test measurement | Pending |
| Bus boarding event | `Boarded Bus` event and `IN_BUS_TO_SCHOOL` status | Observed in Firebase integration test | Passed |
| Bus exit at school | `Left Bus` event and `LEFT_BUS_AT_SCHOOL` status | Observed in Firebase integration test | Passed |
| School entry event | `Entered School` event and `AT_SCHOOL` status | Observed in Firebase integration test | Passed |
| School exit event | `Left School` event and `LEFT_SCHOOL` status | Observed in Firebase integration test | Passed |
| Return bus boarding event | `Boarded Bus To Home` event and `IN_BUS_TO_HOME` status | Observed in Firebase integration test | Passed |
| Home arrival event | `Arrived Home` event and `ARRIVED_HOME` status | Observed in Firebase integration test | Passed |
| Firebase student status update | `/students/{uid}` is updated | Observed in Firebase integration test | Passed |
| Event log creation | New record is added to `/events` | Observed in Firebase integration test | Passed |
| Notification record creation | New record is added to `/notifications` | Observed in Firebase integration test | Passed |
| Safety delay alert | Record is added to `/alerts`, `/notifications`, and `/events` | Observed for bus-to-gate delay | Passed |
| TFT display output | Success/error screen is displayed | Implemented in code; needs photo/video evidence | Needs real test measurement |
| Green LED and buzzer success indication | Green LED and buzzer activate after success | Implemented in code; needs real test measurement | Pending |
| Red LED error indication | Red LED and buzzer activate on error/warning | Implemented in code; needs real test measurement | Pending |
| Wi-Fi reconnection if implemented | Firebase reconnect and retry behavior operate after connection issues | Firebase reconnect enabled and Firebase retry every 10 seconds if not ready; needs real network test | Pending |

### 6.2 Performance-Related Configuration Values From Code

| Metric | Value from Code | Notes |
|---|---:|---|
| Duplicate scan cooldown | 3000 ms | Used by both ESP32 units. |
| Bus-to-school alert timer | 5 minutes | Triggered by `IN_BUS_TO_SCHOOL`. |
| Bus-to-gate alert timer | 1 minute | Triggered by `LEFT_BUS_AT_SCHOOL`. |
| School-to-bus alert timer | 1 minute | Triggered by `LEFT_SCHOOL`. |
| Bus-to-home alert timer | 5 minutes | Triggered by `IN_BUS_TO_HOME`. |
| Old safety session maximum age | 6 hours | Prevents stale old route alerts. |
| Local backend monitor interval | 10 seconds | Used by `tools/safety_monitor_backend.dart`. |
| Flutter safety audit interval | 10 seconds | Used by `SafetyMonitorService`. |
| Cloud Functions scheduler | Every 1 minute | If Firebase Cloud Functions are deployed. |
| Cloud Functions timeout | 120 seconds | Defined in `functions/index.js`. |
| Wi-Fi initial connection attempts | 40 tries x 500 ms | Approximately 20 seconds maximum. |
| NTP time wait | 15 seconds maximum | Checks every 500 ms. |
| Firebase retry if not ready | 10 seconds | ESP bus code retry loop. |
| Result screen display duration | 4000 ms | TFT returns to idle after 4 seconds. |
| Idle clock/status refresh | 30000 ms | Used by ESP TFT screens. |
| Flutter splash delay | 180 ms | Reduced app startup splash delay. |
| Bus success buzzer for entry | 180 ms x 1 | Bus unit. |
| Bus success buzzer for exit/home arrival | 100 ms x 2 | Bus unit. |
| Gate success buzzer for entry | 200 ms x 1 | School gate unit. |
| Gate success buzzer for exit | 120 ms x 2 | School gate unit. |
| Warning buzzer | 150 ms x 2 | Both ESP32 units. |
| Error buzzer | 100 ms x 3 | Both ESP32 units. |
| Multi-beep gap | 80 ms | Both ESP32 units. |
| Boot splash/display delay | 1500 ms bus, 1600 ms gate | TFT boot screen delay. |

### 6.3 Values That Must Be Measured Manually

| Measurement | Simple Measurement Method |
|---|---|
| RFID reading response time | Record a video showing the card tap and TFT/buzzer response, then calculate the time difference. |
| Firebase upload delay | Compare ESP Serial Monitor timestamp with the Firebase event timestamp or screen-record the Firebase console. |
| Mobile application update delay | Record the phone screen during a scan and note the time until the status changes. |
| Notification delivery delay | Record the scan moment and the notification popup time on the mobile device. |
| TFT display update time | Record the RFID scan and TFT success/error screen, then measure the time difference. |
| Overall scan-to-notification time | Use one video showing RFID scan and phone notification, or compare ESP Serial time with notification time. |

### 6.4 Recommended Screenshots and Images for the Final Report

- Full hardware setup with both ESP32 units.
- School Bus Unit close-up.
- School Gate Unit close-up.
- RFID card scan moment.
- TFT success screen: Boarded Bus or Entered School.
- TFT error screen: Unknown Card or Invalid Transition.
- Green LED success indication.
- Red LED error indication.
- Firebase Realtime Database root structure.
- Firebase `/students/{uid}` record.
- Firebase `/events` event record.
- Firebase `/notifications` notification record.
- Firebase `/alerts` safety alert record.
- Flutter login screen.
- Flutter dashboard.
- Flutter student status screen.
- Flutter live events/notifications screen.
- Flutter history screen.
- Flutter safety alerts screen.
- Real mobile notification popup.

### 6.5 System Evaluation Criteria

| Criterion | Evaluation Method |
|---|---|
| RFID detection accuracy | Scan known and unknown cards. |
| Firebase integration | Verify records in `/students`, `/events`, and `/notifications`. |
| Safety alert logic | Trigger delay scenarios and verify `/alerts`. |
| Mobile app usability | Test dashboard, history, alerts, and language switch. |
| Hardware feedback | Observe TFT display, LEDs, and buzzer. |
| Real-time behavior | Measure scan-to-update and scan-to-notification delays. |

### 6.6 Achievement of Project Objectives

| Objective | Achievement |
|---|---|
| Track student movement using RFID | Achieved |
| Monitor bus and school gate scans | Achieved |
| Store data in Firebase | Achieved |
| Display status in parent app | Achieved |
| Generate notification records | Achieved |
| Generate safety delay alerts | Achieved |
| Support Arabic and English | Achieved |
| Provide professional parent UI | Achieved |
| Full cloud push when app is terminated | Prepared, but requires deployed Firebase Cloud Functions and production FCM setup |

### 6.7 Main System Results

| Result | Evidence |
|---|---|
| Student can be identified by RFID UID | Observed in integration testing |
| Bus events update Firebase | Observed |
| Gate events update Firebase | Observed |
| Event logs are created | Observed |
| Notifications are created | Observed |
| Safety alert for bus-to-gate delay works | Observed |
| TFT, LED, and buzzer outputs | Implemented; attach real test photos/videos |

### 6.8 Performance Evaluation Summary

| Metric | Result |
|---|---|
| RFID response time | Placeholder: measure manually |
| Firebase upload delay | Placeholder: measure manually |
| Mobile app update delay | Placeholder: measure manually |
| Notification delivery delay | Placeholder: measure manually |
| TFT display update time | Placeholder: measure manually |
| Duplicate scan cooldown | 3 seconds from code |
| Safety timer accuracy | 1 minute and 5 minutes from code; validate manually |

### 6.9 Advantages of the Proposed System

| Advantage |
|---|
| Real-time student safety tracking using IoT and RFID. |
| Parent-friendly mobile monitoring interface. |
| Separate bus and school gate verification points. |
| Automatic event history and notification records. |
| Safety delay detection for risky transitions. |
| Bilingual Arabic/English support. |
| Visual and audio feedback on ESP32 units. |
| Firebase structure suitable for future expansion. |

### 6.10 System Limitations

| Limitation |
|---|
| Internet connection is required for Firebase updates. |
| Local notification behavior depends on mobile OS permissions. |
| Full server push requires deployed Firebase Cloud Functions and Firebase Cloud Messaging. |
| RFID range depends on RC522 hardware and card position. |
| Real performance must be measured physically, not only from code. |
| Stale Firebase data must be reset before clean demonstrations. |

### 6.11 Future Enhancements

| Enhancement |
|---|
| Deploy production Firebase Cloud Functions for server-side alerts. |
| Add an admin dashboard for school supervisors. |
| Add GPS tracking for buses. |
| Add a bus driver mobile interface. |
| Add attendance report export as PDF or Excel. |
| Add stronger authentication and Firebase security rules. |
| Add offline queueing on ESP32 when Wi-Fi is unavailable. |
| Add enclosure and PCB design for the final hardware prototype. |

