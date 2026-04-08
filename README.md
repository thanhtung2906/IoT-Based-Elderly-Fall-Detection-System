# IoT-Based-Elderly-Fall-Detection-System

## 📌 Project Overview
The **IoT-Based Elderly Fall Detection System** is an end-to-end wearable solution designed to monitor the movements of elderly individuals and detect fall events in real-time. 

By combining edge hardware (ESP32 & MPU6050) with machine learning (Random Forest & Logistic Regression) and an automated event-handling pipeline (Node-RED), this project provides a reliable, low-latency alert system to notify guardians immediately when a fall occurs.

## 🛠 Tech Stack & Tools
* **Hardware:** ESP32 Microcontroller, MPU6050 (Accelerometer & Gyroscope).
* **Embedded Software:** C++ / Arduino IDE.
* **Connectivity:** MQTT Protocol (Local Mosquitto Broker), HTTP (Web Server for data collection), TCP/IP.
* **Machine Learning:** Python, Scikit-learn (Random Forest, Logistic Regression).
* **Data Flow & Automation:** Node-RED (for event routing, Firebase storage, and SMTP email alerts).

## ⚙️ System Architecture

The project operates in two main phases:

### 1. Data Collection & Model Training
* The MPU6050 collects raw acceleration and gyroscope data from volunteers.
* An ESP32 hosts an HTTP Web Server (STA mode), providing a user interface to control data logging (set sampling frequency, start/stop, and download datasets).
* The collected data is used to train and optimize the action classification models (Random Forest and Logistic Regression).

### 2. Real-Time Deployment & Inference
* **Edge Node:** The wearable ESP32 acts as an MQTT Publisher, continuously streaming real-time motion data.
* **Message Broker:** A local Mosquitto server routes the sensor data streams.
* **Inference Engine:** A Python script subscribes to the MQTT topics, feeds the live data into the trained ML model, and predicts the user's action.
* **Event Handling:** Node-RED receives the prediction outputs. It logs the data to Firebase for future analysis and routes the events. If a "Fall" is detected, it instantly triggers an SMTP protocol alert to the guardian's email.

## 🔌 Hardware Setup (Wiring)
Connect the MPU6050 sensor to the ESP32 using the standard I2C interface:
* **VCC:** 3.3V
* **GND:** GND
* **SCL:** D22 
* **SDA:** D21 

## 🚀 How to Run

Follow these steps to deploy the real-time detection system locally:

### Step 1: Start the MQTT Broker
Ensure you have Mosquitto installed on your local machine. Start the service. It runs on the default port `1883` and accepts anonymous connections (no username/password required).

### Step 2: Configure and Flash the ESP32
1. Open the `.ino` project file in Arduino IDE.
2. Update the WiFi credentials in the source code to match your local network:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. Update the MQTT Broker IP address to your computer's local IP.
4. Compile and flash the code to the ESP32.

### Step 3: Run the Machine Learning Inference
Ensure you have Python installed. Since there is no `requirements.txt` yet, manually install the required standard libraries:
```bash
pip install paho-mqtt scikit-learn pandas numpy
```
Run the prediction script to start listening to MQTT data and making real-time classifications:
```bash
python inference.py
```

### Step 4: Set up Node-RED Pipeline
1. Open your local Node-RED dashboard (usually `http://localhost:1880`).
2. Manually create a flow with the following nodes:
   * **MQTT In node:** Subscribe to the topic where `inference.py` publishes the final predictions.
   * **Switch node:** Route the flow based on the payload (e.g., if payload == "Fall").
   * **Firebase out node:** Configure your Firebase credentials to log the events.
   * **Email out node:** Configure SMTP settings to send alert emails to the guardian.
3. Deploy the flow.
