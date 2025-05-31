#include "esp_camera.h"
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>

// WiFi credentials
const char* ssid = "AHMED";
const char* password = "ahmed1122";

// Camera pin definitions for AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// SD Card pin
#define SD_CS_PIN         13

// Web server on port 80
WiFiServer server(80);

// Data parsing variables
String inputString = "";
bool stringComplete = false;
const char START_MARKER = '<';
const char END_MARKER = '>';

void setup() {
  Serial.begin(115200);
  
  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  Serial.println("SD Card Initialized");

  // Initialize the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start web server
  server.begin();
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());

  // Configure LED FLASH as output
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  // Reserve 256 bytes for the inputString
  inputString.reserve(256);
}

void loop() {
  // Check for serial data
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == START_MARKER) {
      inputString = "";
    } else if (inChar == END_MARKER) {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }

  // Process complete messages
  if (stringComplete) {
    processEncoderData(inputString);
    inputString = "";
    stringComplete = false;
  }

  // Handle web clients
  WiFiClient client = server.available();
  if (client) {
    handleWebClient(client);
  }
}

void processEncoderData(String data) {
  // Expected format: "ENCODER,leftCount,rightCount,timestamp"
  int firstComma = data.indexOf(',');
  if (firstComma == -1) return;
  
  String dataType = data.substring(0, firstComma);
  if (dataType != "ENCODER") return;
  
  int secondComma = data.indexOf(',', firstComma + 1);
  int thirdComma = data.indexOf(',', secondComma + 1);
  
  if (secondComma == -1 || thirdComma == -1) return;
  
  String leftStr = data.substring(firstComma + 1, secondComma);
  String rightStr = data.substring(secondComma + 1, thirdComma);
  String timeStr = data.substring(thirdComma + 1);
  
  long leftCount = leftStr.toInt();
  long rightCount = rightStr.toInt();
  unsigned long timestamp = timeStr.toInt();
  
  // Save to SD card
  saveToSD(leftCount, rightCount, timestamp);
}

void saveToSD(long leftCount, long rightCount, unsigned long timestamp) {
  File dataFile = SD.open("/encoder.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(timestamp);
    dataFile.print(",");
    dataFile.print(leftCount);
    dataFile.print(",");
    dataFile.println(rightCount);
    dataFile.close();
  } else {
    Serial.println("Error opening encoder.csv");
  }
}

void handleWebClient(WiFiClient client) {
  String request = client.readStringUntil('\r');
  client.flush();
  
  if (request.indexOf("GET / ") != -1) {
    // Serve main page
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><body>");
    client.println("<h1>ESP32-CAM Encoder Data Logger</h1>");
    client.println("<p>Encoder data is being logged to SD card</p>");
    client.println("<p><a href=\"/download\">Download Data</a></p>");
    client.println("<p><a href=\"/stream\">View Camera Stream</a></p>");
    client.println("</body></html>");
  }
  else if (request.indexOf("GET /download") != -1) {
    // Serve the CSV file
    File dataFile = SD.open("/encoder.csv");
    if (dataFile) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/csv");
      client.println("Content-Disposition: attachment; filename=encoder_data.csv");
      client.println();
      
      while (dataFile.available()) {
        client.write(dataFile.read());
      }
      dataFile.close();
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Content-Type: text/html");
      client.println();
      client.println("<html><body><h1>404 - File Not Found</h1></body></html>");
    }
  }
  else if (request.indexOf("GET /stream") != -1) {
    // Serve camera stream
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
    client.println();
    
    while (client.connected()) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        break;
      }
      
      client.println("--frame");
      client.println("Content-Type: image/jpeg");
      client.println("Content-Length: " + String(fb->len));
      client.println();
      client.write(fb->buf, fb->len);
      client.println();
      
      esp_camera_fb_return(fb);
      
      if (!client.connected()) break;
    }
  }
  
  delay(1);
  client.stop();
}