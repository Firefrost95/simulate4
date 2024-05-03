#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>

const int trigPin = 7;   // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21;  // Echo Pin of Ultrasonic Sensor
long initialDistance = -1;  // Variable to store the first measured distance
// Define your Google Apps Script endpoint URL
const char* scriptURL = "https://script.google.com/macros/s/AKfycbzTW6LPc59BaGms6Vi9uFcm0uL2CY_IG5duj4nf_6-2vEoGD9DG4F3BF4DIPVzV8ea9/exec";

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

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "IoT";
const char* password = "KdGIoT70!";

//const char* ssid = "Proximus-Home-850113";
//const char* password = "44a4dxh37e3wpfab";

//const char* ssid = "ARC";
//const char* password = "1nt3rn3t4rC!";
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
  config.frame_size = FRAMESIZE_SVGA;
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
  s->set_vflip(s, 1); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, -1); // lower the saturation
  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
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
  Serial.println(imageBase64);


  // Send the captured image to Google Apps Script
  HTTPClient http;
  http.begin(scriptURL); // Specify the destination URL
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Construct the POST body with the image data
  String postData = "imageData=" + imageBase64;
  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST request sent, response code: %d\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP] POST request failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  // Free the camera frame buffer
  esp_camera_fb_return(fb);
  http.end();
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
      // Send local IP address to the Google Apps Script web app
      captureAndSendImage();
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