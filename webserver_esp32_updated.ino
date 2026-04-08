#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ====== WiFi Configuration - THAY ĐỔI THÔNG TIN NÀY ======
const char* ssid = "thanh tung";           // Thay bằng tên WiFi của bạn
const char* password = "88888888";          // Thay bằng mật khẩu WiFi

// Timeout configuration
const unsigned long WIFI_TIMEOUT = 30000;        // 30 giây timeout
const unsigned long WIFI_RETRY_INTERVAL = 500;   // Thử lại mỗi 500ms

WebServer server(80);

// MPU6050 Sensor
MPU6050 mpu6050(Wire);

// Data collection control
bool isCollecting = false;
bool isMPU6050Ready = false;
unsigned long lastSampleTime = 0;
unsigned long sampleInterval = 100;
unsigned long collectionStartTime = 0;
unsigned long maxDuration = 60000;

int sampleCount = 0;

// Memory management
const size_t MAX_BUFFER_SIZE = 32000;
String dataBuffer = "";
String fileName = "sensor_data.csv";

// Error handling
String lastError = "";

// WiFi status monitoring
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // Kiểm tra WiFi mỗi 10 giây

// Enhanced HTML interface
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 MPU6050 Data Logger</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
    .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h2 { color: #333; text-align: center; }
    .wifi-info { background-color: #e3f2fd; padding: 10px; border-radius: 5px; margin-bottom: 15px; font-size: 14px; }
    .wifi-info strong { color: #1976d2; }
    button { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    .btn-start { background-color: #4CAF50; color: white; }
    .btn-stop { background-color: #f44336; color: white; }
    .btn-download { background-color: #2196F3; color: white; }
    .btn-clear { background-color: #ff9800; color: white; }
    .status { padding: 15px; margin: 10px 0; border-radius: 5px; font-weight: bold; }
    .status-idle { background-color: #e7e7e7; color: #333; }
    .status-collecting { background-color: #c8e6c9; color: #2e7d32; }
    .status-error { background-color: #ffcdd2; color: #c62828; }
    input[type="number"] { width: 80px; padding: 5px; margin: 5px; }
    input[type="text"] { padding: 5px; margin: 5px; }
    label { font-weight: bold; }
    .connection-status { text-align: center; margin-bottom: 10px; font-size: 12px; }
    .connected { color: green; }
    .disconnected { color: red; }
  </style>
</head>
<body>
  <div class="container">
    <h2>ESP32 MPU6050 Data Logger</h2>
    
    <div class="wifi-info" id="wifiInfo">
      <strong>WiFi:</strong> <span id="wifiSSID">Loading...</span><br>
      <strong>IP:</strong> <span id="wifiIP">Loading...</span><br>
      <strong>Signal:</strong> <span id="wifiRSSI">Loading...</span> dBm
    </div>
    
    <div class="connection-status" id="connectionStatus">
      <span class="disconnected">Connecting...</span>
    </div>
    
    <div>
      <label>Sample Interval (ms): </label>
      <input type="number" id="interval" value="100" min="50" max="5000">
      <button onclick="setSampleInterval()" class="btn-download">Set</button>
    </div>
    
    <div>
      <label>File Name: </label>
      <input type="text" id="filename" value="sensor_data.csv" style="width: 200px;">
      <button onclick="setFilename()" class="btn-download">Set</button>
    </div>
    
    <div>
      <label>Max Duration (s): </label>
      <input type="number" id="duration" value="60" min="1" max="3600">
      <button onclick="setDuration()" class="btn-download">Set</button>
    </div>
    
    <div style="margin: 20px 0;">
      <button onclick="startCollection()" class="btn-start">Start Collection</button>
      <button onclick="stopCollection()" class="btn-stop">Stop Collection</button>
      <button onclick="downloadData()" class="btn-download">Download Data</button>
      <button onclick="clearData()" class="btn-clear">Clear Data</button>
    </div>

    <div id="statusBox" class="status status-idle">Status: Loading...</div>
  </div>

  <script>
    let statusUpdateTimer = null;
    let isConnected = false;

    async function startCollection() {
      try {
        const response = await fetch('/start');
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.text();
        showMessage(result, 'success');
        updateStatusNow();
      } catch (error) {
        showMessage('Error starting collection: ' + error.message, 'error');
        console.error('Start collection error:', error);
      }
    }

    async function stopCollection() {
      try {
        const response = await fetch('/stop');
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.text();
        showMessage(result, 'success');
        updateStatusNow();
      } catch (error) {
        showMessage('Error stopping collection: ' + error.message, 'error');
        console.error('Stop collection error:', error);
      }
    }

    async function downloadData() {
      try {
        const response = await fetch('/download');
        if (!response.ok) {
          const error = await response.text();
          throw new Error(error);
        }
        const blob = await response.blob();
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = document.getElementById('filename').value;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);
        showMessage('Data downloaded successfully', 'success');
      } catch (error) {
        showMessage('Error downloading data: ' + error.message, 'error');
        console.error('Download error:', error);
      }
    }

    async function clearData() {
      if (confirm('Are you sure you want to clear all collected data?')) {
        try {
          const response = await fetch('/clear');
          if (!response.ok) throw new Error('HTTP ' + response.status);
          const result = await response.text();
          showMessage(result, 'success');
          updateStatusNow();
        } catch (error) {
          showMessage('Error clearing data: ' + error.message, 'error');
          console.error('Clear data error:', error);
        }
      }
    }

    async function setSampleInterval() {
      const interval = document.getElementById('interval').value;
      if (interval < 50 || interval > 5000) {
        showMessage('Invalid interval. Use 50-5000ms', 'error');
        return;
      }
      try {
        const response = await fetch('/setinterval?value=' + interval);
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.text();
        showMessage(result, 'success');
      } catch (error) {
        showMessage('Error setting interval: ' + error.message, 'error');
        console.error('Set interval error:', error);
      }
    }

    async function setFilename() {
      const filename = document.getElementById('filename').value;
      if (!filename || filename.length === 0) {
        showMessage('Please enter a valid filename', 'error');
        return;
      }
      try {
        const response = await fetch('/setfilename?value=' + encodeURIComponent(filename));
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.text();
        showMessage(result, 'success');
      } catch (error) {
        showMessage('Error setting filename: ' + error.message, 'error');
        console.error('Set filename error:', error);
      }
    }

    async function setDuration() {
      const duration = document.getElementById('duration').value;
      if (duration < 1 || duration > 3600) {
        showMessage('Invalid duration. Use 1-3600 seconds', 'error');
        return;
      }
      try {
        const response = await fetch('/setduration?value=' + duration);
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.text();
        showMessage(result, 'success');
      } catch (error) {
        showMessage('Error setting duration: ' + error.message, 'error');
        console.error('Set duration error:', error);
      }
    }

    async function updateStatus() {
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 5000);
        
        const response = await fetch('/api/status', {
          signal: controller.signal,
          cache: 'no-cache'
        });
        
        clearTimeout(timeoutId);
        
        if (!response.ok) {
          throw new Error('HTTP ' + response.status);
        }
        
        const data = await response.json();
        updateUI(data);
        updateWiFiInfo(data);
        setConnectionStatus(true);
        
      } catch (error) {
        console.error('Status update error:', error);
        setConnectionStatus(false);
        
        const statusBox = document.getElementById('statusBox');
        statusBox.textContent = 'Status: Connection Lost';
        statusBox.className = 'status status-error';
      }
    }

    function updateUI(data) {
      const statusBox = document.getElementById('statusBox');
      statusBox.textContent = data.status || 'Status: Unknown';
      
      if (data.sensor === false || data.error === true) {
        statusBox.className = 'status status-error';
      } else if (data.collecting === true) {
        statusBox.className = 'status status-collecting';
      } else {
        statusBox.className = 'status status-idle';
      }
    }

    function updateWiFiInfo(data) {
      if (data.wifi) {
        document.getElementById('wifiSSID').textContent = data.wifi.ssid || 'N/A';
        document.getElementById('wifiIP').textContent = data.wifi.ip || 'N/A';
        document.getElementById('wifiRSSI').textContent = data.wifi.rssi || 'N/A';
      }
    }

    function setConnectionStatus(connected) {
      isConnected = connected;
      const statusElement = document.getElementById('connectionStatus');
      if (connected) {
        statusElement.innerHTML = '<span class="connected">Connected</span>';
      } else {
        statusElement.innerHTML = '<span class="disconnected">Disconnected</span>';
      }
    }

    function showMessage(message, type) {
      if (type === 'error') {
        console.error('Message:', message);
      } else {
        console.log('Message:', message);
      }
    }

    function updateStatusNow() {
      updateStatus();
    }

    function startStatusUpdates() {
      if (statusUpdateTimer) {
        clearInterval(statusUpdateTimer);
      }
      statusUpdateTimer = window.setInterval(updateStatus, 1000);
    }

    function stopStatusUpdates() {
      if (statusUpdateTimer) {
        clearInterval(statusUpdateTimer);
        statusUpdateTimer = null;
      }
    }

    window.addEventListener('load', function() {
      updateStatus();
      startStatusUpdates();
    });

    window.addEventListener('beforeunload', function() {
      stopStatusUpdates();
    });
  </script>
</body>
</html>
)rawliteral";

// Function prototypes
void handleRoot();
void handleStart();
void handleStop();
void handleStatus();
void handleDownload();
void handleClear();
void handleSetInterval();
void handleSetFilename();
void handleSetDuration();
void handleAPIStatus();
void collectSensorData();
bool initializeMPU6050();
bool connectToWiFi();
void checkWiFiConnection();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 MPU6050 Data Logger Starting...");
  Serial.println("=================================\n");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("⚠ SPIFFS Mount Failed");
  } else {
    Serial.println("✓ SPIFFS initialized");
  }
  
  // Initialize MPU6050
  Serial.println("\nInitializing MPU6050 sensor...");
  isMPU6050Ready = initializeMPU6050();
  
  // Connect to WiFi
  Serial.println("\nConnecting to WiFi...");
  if (connectToWiFi()) {
    Serial.println("✓ WiFi connected successfully!");
    Serial.println("\n=================================");
    Serial.println("WEBSERVER ACCESS INFORMATION");
    Serial.println("=================================");
    Serial.println("Network: " + String(ssid));
    Serial.println("IP Address: " + WiFi.localIP().toString());
    Serial.println("Signal Strength: " + String(WiFi.RSSI()) + " dBm");
    Serial.println("\nOpen browser and go to:");
    Serial.println("http://" + WiFi.localIP().toString());
    Serial.println("=================================\n");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("Please check your WiFi credentials and try again.");
    return; // Không tiếp tục nếu không kết nối được WiFi
  }
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);
  server.on("/download", handleDownload);
  server.on("/clear", handleClear);
  server.on("/setinterval", handleSetInterval);
  server.on("/setfilename", handleSetFilename);
  server.on("/setduration", handleSetDuration);
  server.on("/api/status", handleAPIStatus);
  
  // Start web server
  server.begin();
  Serial.println("✓ Web server started successfully!");
  
  // Initialize data buffer with CSV header
  dataBuffer = "Timestamp(ms),AccX(g),AccY(g),AccZ(g),GyroX(deg/s),GyroY(deg/s),GyroZ(deg/s)\n";
  
  Serial.println("\nSystem ready!");
}

void loop() {
  // Check WiFi connection periodically
  if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }
  
  server.handleClient();
  
  // Collect sensor data if enabled
  if (isCollecting && isMPU6050Ready) {
    collectSensorData();
  }
  
  delay(1);
}

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  
  unsigned long startAttemptTime = millis();
  
  // Chờ kết nối với timeout
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < WIFI_TIMEOUT) {
    delay(WIFI_RETRY_INTERVAL);
    Serial.print(".");
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    Serial.println("WiFi connection timeout!");
    Serial.println("Status code: " + String(WiFi.status()));
    return false;
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n⚠ WiFi disconnected! Attempting to reconnect...");
    
    if (connectToWiFi()) {
      Serial.println("✓ WiFi reconnected!");
      Serial.println("IP Address: " + WiFi.localIP().toString());
    } else {
      Serial.println("✗ WiFi reconnection failed!");
    }
  }
}

bool initializeMPU6050() {
  Wire.begin();
  mpu6050.begin();
  
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) {
    Serial.println("✗ MPU6050 not found!");
    lastError = "MPU6050 sensor not detected";
    return false;
  }
  
  mpu6050.calcGyroOffsets(true);
  Serial.println("✓ MPU6050 initialized successfully");
  lastError = "";
  return true;
}

void collectSensorData() {
  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();
    
    if (dataBuffer.length() > MAX_BUFFER_SIZE) {
      isCollecting = false;
      lastError = "Buffer full - collection stopped";
      Serial.println("⚠ Buffer full! Collection stopped automatically.");
      return;
    }
    
    mpu6050.update();
    if (isCollecting && (millis() - collectionStartTime >= maxDuration)) {
      isCollecting = false;
      lastError = "Max duration reached - collection stopped";
      Serial.println("⏱ Collection stopped: max duration reached");
    }

    String dataLine = String(millis()) + "," +
                     String(mpu6050.getAccX(), 3) + "," +
                     String(mpu6050.getAccY(), 3) + "," +
                     String(mpu6050.getAccZ(), 3) + "," +
                     String(mpu6050.getGyroX(), 3) + "," +
                     String(mpu6050.getGyroY(), 3) + "," +
                     String(mpu6050.getGyroZ(), 3) + "\n";
    
    dataBuffer += dataLine;
    sampleCount++;
    
    if (sampleCount % 10 == 0) {
      Serial.println("📊 Samples collected: " + String(sampleCount));
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStart() {
  if (!isMPU6050Ready) {
    server.send(400, "text/plain", "Error: MPU6050 not ready");
    return;
  }
  
  isCollecting = true;
  collectionStartTime = millis();
  sampleCount = 0;
  lastError = "";
  
  dataBuffer = "Timestamp(ms),AccX(g),AccY(g),AccZ(g),GyroX(deg/s),GyroY(deg/s),GyroZ(deg/s)\n";
  
  Serial.println("▶ Data collection started");
  server.send(200, "text/plain", "Data collection started successfully");
}

void handleStop() {
  isCollecting = false;
  Serial.println("⏸ Data collection stopped. Total samples: " + String(sampleCount));
  server.send(200, "text/plain", "Data collection stopped. Samples: " + String(sampleCount));
}

void handleStatus() {
  String status = "Status: ";
  if (!isMPU6050Ready) {
    status += "Sensor Error - " + lastError;
  } else if (isCollecting) {
    status += "Collecting (Samples: " + String(sampleCount) + ")";
  } else {
    status += "Idle (Ready)";
  }
  
  server.send(200, "text/plain", status);
}

void handleDownload() {
  if (dataBuffer.length() <= 100) {
    server.send(400, "text/plain", "No data to download");
    return;
  }
  
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
  server.sendHeader("Content-Length", String(dataBuffer.length()));
  server.send(200, "text/csv", dataBuffer);
  
  Serial.println("⬇ Data downloaded. File size: " + String(dataBuffer.length()) + " bytes");
}

void handleClear() {
  dataBuffer = "Timestamp(ms),AccX(g),AccY(g),AccZ(g),GyroX(deg/s),GyroY(deg/s),GyroZ(deg/s)\n";
  sampleCount = 0;
  lastError = "";
  server.send(200, "text/plain", "Data buffer cleared");
  Serial.println("🗑 Data buffer cleared");
}

void handleSetInterval() {
  if (server.hasArg("value")) {
    int newInterval = server.arg("value").toInt();
    if (newInterval >= 50 && newInterval <= 5000) {
      sampleInterval = newInterval;
      server.send(200, "text/plain", "Sample interval set to " + String(newInterval) + "ms");
      Serial.println("⏱ Sample interval changed to: " + String(newInterval) + "ms");
    } else {
      server.send(400, "text/plain", "Invalid interval. Use 50-5000ms");
    }
  } else {
    server.send(400, "text/plain", "Missing interval value");
  }
}

void handleSetFilename() {
  if (server.hasArg("value")) {
    String newFilename = server.arg("value");
    if (newFilename.length() > 0 && newFilename.length() < 100) {
      fileName = newFilename;
      if (!fileName.endsWith(".csv")) {
        fileName += ".csv";
      }
      server.send(200, "text/plain", "Filename set to " + fileName);
      Serial.println("📝 Filename changed to: " + fileName);
    } else {
      server.send(400, "text/plain", "Invalid filename");
    }
  } else {
    server.send(400, "text/plain", "Missing filename value");
  }
}

void handleSetDuration() {
  if (server.hasArg("value")) {
    int newDuration = server.arg("value").toInt();
    if (newDuration >= 1 && newDuration <= 3600) {
      maxDuration = (unsigned long)newDuration * 1000;
      server.send(200, "text/plain", "Max duration set to " + String(newDuration) + "s");
      Serial.println("⏱ Max duration changed to: " + String(newDuration) + "s");
    } else {
      server.send(400, "text/plain", "Invalid duration. Use 1-3600 seconds");
    }
  } else {
    server.send(400, "text/plain", "Missing duration value");
  }
}

void handleAPIStatus() {
  StaticJsonDocument<512> doc;
  
  doc["collecting"] = isCollecting;
  doc["samples"] = sampleCount;
  doc["sensor"] = isMPU6050Ready;
  doc["memory"] = (dataBuffer.length() * 100) / MAX_BUFFER_SIZE;
  doc["error"] = !lastError.isEmpty();
  
  // Thêm thông tin WiFi
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = String(ssid);
  wifi["ip"] = WiFi.localIP().toString();
  wifi["rssi"] = WiFi.RSSI();
  wifi["connected"] = (WiFi.status() == WL_CONNECTED);
  
  if (isCollecting) {
    doc["status"] = "Status: Collecting data... (" + String(sampleCount) + " samples)";
    doc["duration"] = (millis() - collectionStartTime) / 1000;
  } else if (!isMPU6050Ready) {
    doc["status"] = "Status: Sensor Error - " + lastError;
    doc["duration"] = 0;
  } else {
    doc["status"] = "Status: Ready";
    doc["duration"] = 0;
  }
  
  String response;
  serializeJson(doc, response);
  
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "application/json", response);
}
