#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>
#include "secret.h"

const int trigPin = 7;   // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21;  // Echo Pin of Ultrasonic Sensor
long initialDistance = -1;  // Variable to store the first measured distance


// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD

#include "camera_pins.h"

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(trigPin, OUTPUT);  // Ultrasonic sensor setup
  digitalWrite(trigPin, LOW);

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
  config.frame_size = FRAMESIZE_XGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  // for larger pre-allocated frame buffer.
  if(psramFound()){
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_HVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, -1); // lower the saturation
  
  WiFiCredentials selectedCredentials = selectWiFiCredentials();
  connectToWiFi(selectedCredentials.ssid, selectedCredentials.password);

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

WiFiCredentials selectWiFiCredentials() {
  Serial.println("Select WiFi Network:");
  Serial.println("1: IoT");
  Serial.println("2: Proximus-Home-850113");
  Serial.println("3: ARC");
  Serial.println("4: Enter new WiFi credentials");

  while (!Serial.available()) {
    delay(100);
  }

  int choice = Serial.parseInt();
  Serial.read(); // consume newline

  if (choice >= 1 && choice <= 3) {
    return wifiOptions[choice - 1];
  } else if (choice == 4) {
    return enterNewWiFiCredentials();
  } else {
    Serial.println("Invalid choice, defaulting to IoT");
    return wifiOptions[0];
  }
}

WiFiCredentials enterNewWiFiCredentials() {
  static char newSSID[32];
  static char newPassword[64];

  Serial.println("Enter SSID:");
  readSerialInput(newSSID, sizeof(newSSID));

  Serial.println("Enter Password:");
  readSerialInput(newPassword, sizeof(newPassword));

  return {newSSID, newPassword};
}

void readSerialInput(char* buffer, int bufferSize) {
  int index = 0;
  while (index < bufferSize - 1) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') break;
      buffer[index++] = c;
    }
  }
  buffer[index] = '\0';
}

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void captureAndSendImage() {
  camera_fb_t * fb = NULL;
  // Take a picture
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Encode the image data as Base64
  String imageBase64 = base64::encode(fb->buf, fb->len);
  imageBase64 = urlEncodeBase64(imageBase64); // Encode special characters

  // Send the captured image to Google Apps Script
  HTTPClient http;
  http.begin(scriptURL); // Specify the destination URL
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Construct the POST body with the image data
  String postData = "imageData=" + imageBase64;
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST request sent, response code: %d\n", httpResponseCode);
    // If the response code is 200, read the response body
    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Response from Google Apps Script:");
      Serial.println(response);
    }
  } else {
    Serial.printf("[HTTP] POST request failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  // Free the camera frame buffer
  esp_camera_fb_return(fb);
  http.end();
}

String urlEncodeBase64(String base64) {
  base64.replace("+", "%2B");
  base64.replace("/", "%2F");
  return base64;
}

void loop() {
  long duration, inches, cm;  // Variables to make the calculation for measured distance.

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  pinMode(echoPin, INPUT);
  duration = pulseIn(echoPin, HIGH);
  inches = microsecondsToInches(duration);
  cm = microsecondsToCentimeters(duration);

  // Check if this is the first measurement
  if (initialDistance == -1) {
    initialDistance = cm;  // Store the first measured distance
  } else {
    // Compare the current measurement to the initial measurement
    if (cm == initialDistance) {
      captureAndSendImage();
      delay(1000);
    }
  }

  Serial.print(inches);
  Serial.print(" in, ");
  Serial.print(cm);
  Serial.print(" cm");
  Serial.println();
  delay(100);  // Delay 100ms before next reading.
}

long microsecondsToInches(long microseconds) {
  return microseconds / 74 / 2;
}

long microsecondsToCentimeters(long microseconds) {
  return microseconds / 29 / 2;
}
