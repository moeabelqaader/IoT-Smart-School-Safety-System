# IoT Smart School Safety System - Final Delivery Notes

## المسار النهائي المعتمد

اشتغل وشغل وابني المشروع من المسار ده فقط:

```text
C:\Projects\smart_school_safety
```

أكواد ESP32 النهائية هنا:

```text
C:\Projects\ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino
C:\Projects\ESP32_SEPARATED_CODES\school_gate_reader\school_gate_reader.ino
```

ملف APK النهائي:

```text
C:\Projects\smart_school_safety\build\app\outputs\flutter-apk\app-debug.apk
```

## Local Safety Backend Monitor للديمو

لو Firebase Cloud Functions مش منشورة، شغل الأمر ده من اللاب أثناء الديمو عشان التايمرات تفضل شغالة من السيرفر المحلي وتكتب Alerts/Notifications في Firebase:

```powershell
cd C:\Projects\smart_school_safety
dart run tools\safety_monitor_backend.dart
```

للتأكد مرة واحدة فقط:

```powershell
dart run tools\safety_monitor_backend.dart --once
```

الأداة دي بتفحص `/students` كل 10 ثواني، ولو الطالب عدى وقت الأمان المطلوب، بتكتب في:

```text
/alerts
/notifications
/events
/students/{studentId}
```

مهم: لا تشغل Flutter من مسار عربي مثل:

```text
M:\تخرج\file\smart_school_safety
```

لأن Java/Gradle على Windows لا يقرأ المسار العربي بشكل صحيح وقد يظهر خطأ:

```text
Could not find or load main class org.gradle.wrapper.GradleWrapperMain
```

## أوامر تشغيل التطبيق

افتح PowerShell:

```powershell
cd C:\Projects\smart_school_safety
flutter pub get
flutter run
```

أو ثبت APK يدويًا:

```powershell
adb install -r "C:\Projects\smart_school_safety\build\app\outputs\flutter-apk\app-debug.apk"
```

## آخر تعديلات التطبيق

- كل كروت `System features` أصبحت تفتح صفحات فعلية واضحة بدل تغيير التاب بصمت.
- كل حساب ولي أمر يرى الطالب المرتبط به فقط.
- تم تنظيف ربط Firebase:
  - `4TtQiiCQMsgAiizmcbUagSgI4I82` -> `71E2344C`
  - `KVuO5k05MGa8aSTaXjkS29MAhC83` -> `210E160E`
- تم تقوية Safety Alerts:
  - التطبيق يعمل Safety Audit كل 10 ثواني.
  - لو الطالب تأخر بعد حالة خطر، ينشئ Alert في Firebase ويظهر Local Notification فورًا.
  - التنبيه لا يعتمد فقط على Stream أو Timer واحد.
- تم إضافة Firebase indexes للمسارات:
  - `/events`
  - `/notifications`
  - `/alerts`
  - `/students`

## آخر تعديلات ESP32

### Bus Reader

المسار:

```text
C:\Projects\ESP32_SEPARATED_CODES\bus_reader\bus_reader.ino
```

التعديلات:

- الشاشة والبازر يستجيبوا فور قراءة الكارت بدل انتظار كل عمليات Firebase.
- Cooldown أصبح 3 ثواني بدل 5 ثواني.
- يسجل:
  - `Boarded Bus`
  - `Left Bus`
  - `Boarded Bus To Home`
  - `Arrived Home`
- يكتب في:
  - `/students`
  - `/events`
  - `/notifications`
  - `/devices`
  - `/buses`

### School Gate Reader

المسار:

```text
C:\Projects\ESP32_SEPARATED_CODES\school_gate_reader\school_gate_reader.ino
```

التعديلات:

- بداية اليوم أصبحت نظيفة:
  - `presentCount = 0`
  - `lastActivity = No scans today`
  - `sessionDate = تاريخ اليوم`
- الشاشة لا تعرض آخر طالب قديم بعد تشغيل جديد في يوم جديد.
- Cooldown أصبح 3 ثواني.
- لا يسمح بدخول المدرسة مباشرة من الباص قبل `Left Bus`.
- يسجل:
  - `Entered School`
  - `Left School`

## تسلسل الاختبار النهائي

ابدأ من Firebase وخلي الطالب:

```text
currentStatus = AT_HOME
```

ثم اختبر:

```text
1. Scan على جهاز الباص
   Expected: Boarded Bus / IN_BUS_TO_SCHOOL

2. Scan تاني على جهاز الباص بعد 3 ثواني
   Expected: Left Bus / LEFT_BUS_AT_SCHOOL

3. لا تعمل Scan على المدرسة لمدة دقيقة
   Expected: Safety Alert / ALERT_DELAY

4. لإلغاء سيناريو التأخير في تجربة أخرى:
   Scan على المدرسة قبل الدقيقة
   Expected: Entered School / AT_SCHOOL

5. Scan على المدرسة بعد اليوم الدراسي
   Expected: Left School / LEFT_SCHOOL

6. Scan على الباص
   Expected: Boarded Bus To Home / IN_BUS_TO_HOME

7. Scan على الباص عند البيت
   Expected: Arrived Home / ARRIVED_HOME
```

## ملاحظات مهمة

- لو التطبيق مفتوح أو في الخلفية، Safety Alerts المحلية تعمل.
- لو التطبيق مغلق تمامًا وتريد إشعار حقيقي من السيرفر، يلزم Firebase Cloud Functions + FCM server push.
- APK لا يعمل على iPhone. iOS يحتاج Mac + Xcode + Firebase iOS config.
- تحذيرات Kotlin أثناء build ليست مشكلة طالما ظهر:

```text
Built build\app\outputs\flutter-apk\app-debug.apk
```

## الملفات التي ترسلها لصاحبك

أرسل فولدر:

```text
C:\Projects\smart_school_safety
C:\Projects\ESP32_SEPARATED_CODES
```

أو للاختبار السريع أرسل APK:

```text
C:\Projects\smart_school_safety\build\app\outputs\flutter-apk\app-debug.apk
```
