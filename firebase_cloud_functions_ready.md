# Firebase Cloud Functions + FCM

The backend safety monitor is now implemented under `functions/index.js`.

It does two jobs:

1. Sends FCM push notifications immediately when the ESP32 or demo app creates a scan record under `/events`.
2. Runs every minute and checks the safety windows:
   - `IN_BUS_TO_SCHOOL`: 5 minutes.
   - `LEFT_BUS_AT_SCHOOL`: 1 minute.
   - `LEFT_SCHOOL`: 1 minute.
   - `IN_BUS_TO_HOME`: 5 minutes.

When a timer expires, the function:

- Re-reads the latest student status.
- Prevents duplicate alerts using `dedupeKey`.
- Creates `/alerts/{alertId}`.
- Adds a `Safety Alert` event under `/events`.
- Sets the student status to `ALERT_DELAY`.
- Sends a high-priority FCM notification to the parent phone tokens saved under:

```text
parents/{parentId}/fcmTokens/{tokenKey}
```

Deploy steps:

```powershell
cd smart_school_safety
firebase login
firebase use <your-project-id>
firebase deploy --only database,functions
```

The Flutter app still keeps `SafetyMonitorService` as a local fallback for the graduation demo. The real push path is Firebase Cloud Functions + FCM.

Normal `/notifications` records are still supported as a fallback, but `/events` is the main push source to avoid missing scans.
