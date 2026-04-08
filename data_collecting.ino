#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <MPU6050_tockn.h>


// ===========================
const char* ssid = "Enter your Wifi name";
const char* password = "Enter your Password";


const char* mqtt_server = "Enter your mqtt broker server";  

WiFiClient espClient;
PubSubClient client(espClient);
MPU6050 mpu6050(Wire);

// ===========================
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


// ===========================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("✅ connected to MQTT broker");
    } else {
      Serial.print("❌ failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retry in 5s");
      delay(5000);
    }
  }
}

// ===========================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Khởi động cảm biến MPU6050
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);

  // Kết nối WiFi
  setup_wifi();

  // Cấu hình MQTT broker
  client.setServer(mqtt_server, 1883);
}

// ===========================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Cập nhật dữ liệu cảm biến
  mpu6050.update();

  // Lấy dữ liệu cảm biến
  float ax = mpu6050.getAccX();
  float ay = mpu6050.getAccY();
  float az = mpu6050.getAccZ();
  float gx = mpu6050.getGyroX();
  float gy = mpu6050.getGyroY();
  float gz = mpu6050.getGyroZ();

  // In ra Serial để kiểm tra
  Serial.print("ACC: ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.print(az);
  Serial.print(" | GYRO: ");
  Serial.print(gx); Serial.print(", ");
  Serial.print(gy); Serial.print(", ");
  Serial.println(gz);

  // Tạo JSON payload
  char payload[150];
  snprintf(payload, sizeof(payload),
           "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
           "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
           ax, ay, az, gx, gy, gz);

  // Gửi dữ liệu lên MQTT
  client.publish("mpu6050/data", payload);

  Serial.print("Published to MQTT: ");
  Serial.println(payload);

  delay(50); // ~20Hz (50ms)
}
