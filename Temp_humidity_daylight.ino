#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <time.h>

// WiFi credentials
const char* ssid = "Hi";  
const char* password = "123456789";  

// Firestore endpoint
const char* firestoreUrl = "https://firestore.googleapis.com/v1/projects/esp32-fc4e5/databases/(default)/documents/sensorData";

// NTP Server settings for timestamp
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 8 * 3600; // GMT+8 for Philippines
const int daylightOffset_sec = 0;

// DHT Sensor settings
#define DHTPIN 4      // GPIO where DHT11 is connected
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LDR Sensor settings
#define LDRPIN 35     // GPIO where LDR is connected (ADC pin)
#define THRESHOLD 1000 // Threshold to determine day/night (adjust as needed)

void setup() {
  Serial.begin(115200);
  
  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Configure time for Firestore timestamps
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\nFailed to connect! Restarting...");
    ESP.restart();
  }
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize LDR pin
  pinMode(LDRPIN, INPUT);
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

void loop() {
  checkWiFi(); // Ensure WiFi remains connected
  
  // Read DHT sensor values
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Read LDR sensor
  int ldrValue = analogRead(LDRPIN);
  String isDaylight = (ldrValue < THRESHOLD) ? "0" : "1"; // "0" for daylight, "1" for night
  
  Serial.print("LDR Raw Value: ");
  Serial.println(ldrValue);
  Serial.print("Daylight Status: ");
  Serial.println(isDaylight);
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor! Retrying...");
    delay(1000);
    return;
  }

  // Append "°C" for temperature and "%" for humidity in Serial Monitor output
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  if (WiFi.status() == WL_CONNECTED) {
    // Get current time for Firestore
    struct tm timeinfo;
    char timestamp[30];
    
    if (getLocalTime(&timeinfo)) {
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
      Serial.print("Current time: ");
      Serial.println(timestamp);
    } else {
      Serial.println("Failed to obtain time");
      strcpy(timestamp, "Unknown");
    }
    
    // Send data to Firebase Firestore
    sendToFirestore(temperature, humidity, isDaylight, timestamp);
  } else {
    Serial.println("WiFi Disconnected, trying to reconnect...");
    WiFi.begin(ssid, password);
  }
  
  delay(5000); // Wait 5 seconds before sending again
}

void sendToFirestore(float temperature, float humidity, String daylight, const char* timestamp) {
  HTTPClient http;
  http.begin("https://firestore.googleapis.com/v1/projects/esp32-fc4e5/databases/(default)/documents/sensorData"); 

  http.addHeader("Content-Type", "application/json");

  // Correct JSON payload without adding units
  String jsonData = "{"
                    "\"fields\": {"
                    "\"temperature\": {\"doubleValue\": " + String(temperature) + "},"  
                    "\"humidity\": {\"doubleValue\": " + String(humidity) + "},"  
                    "\"daylight\": {\"stringValue\": \"" + daylight + "\"},"  // String is fine for daylight
                    "\"timestamp\": {\"stringValue\": \"" + String(timestamp) + "\"}"
                    "}"
                    "}";

  int httpResponseCode = http.POST(jsonData);

  Serial.print("Firestore HTTP Response code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Firestore Response: " + response);
  } else {
    Serial.println("Firestore Error: " + http.errorToString(httpResponseCode));
  }

  http.end();
}
