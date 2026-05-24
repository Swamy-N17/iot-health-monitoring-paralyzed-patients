# 🏥 IoT-Enabled Health Monitoring and Smart Assistance System for Paralyzed Patients

A real-time IoT-based healthcare system designed to monitor 
vital signs of paralyzed patients and send emergency alerts 
using ESP32, Firebase, and FlutterFlow.

## 🔴 Live App
👉 [Open CareTrack App](https://health-app-iag6pn.flutterflow.app/)

## 🚀 Features
- Real-time monitoring of patient vitals:
  - ❤️ Heart Rate
  - 🌡️ Body Temperature  
  - 💉 SpO2 (Oxygen Saturation)
  - 🖐️ Flex Sensors (4 fingers)
- Emergency assistance alert system
- Firebase Realtime Database integration
- Mobile app with login and patient dashboard
- Instant condition detection (Temp, HR, SpO2 alerts)

## 🛠️ Tech Stack
- **Hardware:** ESP32 Dev Module
- **Programming:** C (Arduino IDE)
- **Database:** Firebase Realtime Database
- **Mobile App:** FlutterFlow
- **Cloud:** Google Firebase

## 📊 Firebase Data Structure

```json
{
  "Body": {
    "Condition": "",
    "HeartRate": "",
    "Sp02": "",
    "Temperature": ""
  },
  "FlexSensors": {
    "Flex1": "",
    "Flex2": "",
    "Flex3": "",
    "Flex4": ""
  }
}
```

## 📌 Status
✅ Hardware Complete
✅ Firebase Integration Complete  
✅ Mobile App Complete
