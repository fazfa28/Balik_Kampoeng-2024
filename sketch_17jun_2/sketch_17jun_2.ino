#include "esp_camera.h"
#include "Arduino.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include "driver/rtc_io.h"
#include "time.h"


int count = 1;

const int led = 14;

const char *ssid = "trapcam_modem";
const char *password = "trapcam123";

String serverName = "192.168.100.193";
String serverPath = "/upload.php";
const int serverPort = 80;

WiFiClient client;

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define PIR_PIN 13

camera_fb_t *capturedImage = NULL;
unsigned long lastMotionDetectedTime = 0;
const unsigned long motionDebounceDelay = 10000; // 5 seconds debounce time

bool initialFramesDiscarded = true;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

struct tm currenttime;

void initTime() {
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&currenttime)) {
    Serial.print("Failed to obtain time. going to deep sleep");

    esp_deep_sleep_start();
    delay(500);
  }
}

void convertBGRtoRGB(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i += 3) {
        uint8_t temp = buf[i];
        buf[i] = buf[i + 2];
        buf[i + 2] = temp;
    }
}


void setup() {
  pinMode(4, OUTPUT);

  // Configure wake up source
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1); // Wake up when PIR sensor detect motion

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    esp_sleep_enable_timer_wakeup(10000000);
    esp_deep_sleep_start();
    delay(500);
  }


  // sensor_t *s = esp_camera_sensor_get();
  // if (s != NULL) {
  //   s->set_exposure_ctrl(s, 0); // enable exposure control
  //   // s->set_brightness(s, 2); // -2 to 2
  //   // // s->set_control(s, 1); // -2 to 2
  //   // s->set_gain_ctrl(s, 1); 
  //   // s->set_whitebal(s, 2);
  //   // s->set_awb_gain(s, 1);
  //   s->set_aec2(s, 0);
  //   s->set_ae_level(s, 0);
  //   s->set_aec_value(s, 50);
  // }

 // sensor_t *s = esp_camera_sensor_get();
  //if (s != NULL) {
    //s->set_exposure_ctrl(s, 0);
    //s->set_aec2(s, 0);
    //s->set_ae_level(s, 2);
    //s->set_aec_value(s, 50);}


 // input pin for PIR Sensor
  pinMode(PIR_PIN, INPUT);


  //blink built-in led when wake up
  // Allow camera to stabilize
  
  // Discard initial frames
  for (int i = 0; i < 2; i++) {
    camera_fb_t *tempFrame = esp_camera_fb_get();
    if (tempFrame) {
      esp_camera_fb_return(tempFrame);
      Serial.println("Discarding initial frame");
      delay(500);
    }
  }
  initialFramesDiscarded = true;

  //Capture photo  
  CapturePhoto();

  digitalWrite(4, HIGH);
  delay(200);
  digitalWrite(4,LOW);
  delay(200);

  WiFi.mode(WIFI_STA);
  int wifi_timer = millis();
  int wifi_restart = millis();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifi_timer> 10000) {
      if (millis() - wifi_restart > 20000){
        esp_deep_sleep_start();
        delay(4000);
      }
      Serial.println("Reconnect the wifi");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      wifi_timer = millis();
    }
    Serial.print(".");
    delay(500);
  }

  Serial.println("Connected.");
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  //Configure time
  initTime();

  //Check if its night or day
  if (currenttime.tm_hour < 18 && currenttime.tm_hour > 5) {
    Serial.print("It's ");
    Serial.print(currenttime.tm_hour);
    Serial.print(":");
    Serial.println(currenttime.tm_min);
    Serial.println("Active");

    //Send photo
     SendPhoto();
  }

  else {
    int duration_hrs = (5 + 24 - currenttime.tm_hour)%12;
    int duration_mins = (60 - currenttime.tm_min)%60;
    // int duration_hrs = 0;
    // int duration_mins = 3;
    //Disabling the previous external wakeup source 
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    delay(5000);
    
    //Set up the night sleep duration
    esp_sleep_enable_timer_wakeup(duration_hrs * 3600000000 + duration_mins * 60000000);
    Serial.print("It's ");
    Serial.print(currenttime.tm_hour);
    Serial.print(":");
    Serial.print(currenttime.tm_min);
    Serial.print(" , going to deep sleep for ");
    Serial.print(duration_hrs);
    Serial.print(" hours and ");
    Serial.print(duration_mins);
    Serial.println(" minutes.");
    esp_deep_sleep_start();
  }

  Serial.println("Going to deep sleep");
  esp_deep_sleep_start();
  delay(2000);
}


void CapturePhoto() {
  if (capturedImage) {
    esp_camera_fb_return(capturedImage);
    capturedImage = NULL;
  }

  delay(3000);

  capturedImage = esp_camera_fb_get();
  if (!capturedImage) {
    Serial.println("Camera capture failed");
    return;
  } else {
    Serial.println("Photo captured");
  }

  Serial.println("check checking running or not");

  // if (initialFramesDiscarded) {
  //   Serial.println("checking if its 2 first pictures");
  //   // delay(3000);
  //   for (int i = 0; i < 2; i++) {
  //     if (capturedImage) {
  //       esp_camera_fb_return(capturedImage);
  //       Serial.println("Discarding first 2 pictures taken");
  //     }
  //     capturedImage = esp_camera_fb_get();
  //   }
  //   initialFramesDiscarded = false;

  // } 
  
}

void SendPhoto() {

  Serial.println("Connecting to server: " + serverName);

  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam-2.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = capturedImage->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    uint8_t *fbBuf = capturedImage->buf;
    size_t fbLen = capturedImage->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);
    esp_camera_fb_return(capturedImage);
    capturedImage = NULL;

    int timeoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;
    String getAll;
    String getBody;

    while ((startTimer + timeoutTimer) > millis()) {
      Serial.print(".");
      delay(100);
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length() == 0) { state = true; }
          getAll = "";
        } else if (c != '\r') { getAll += String(c); }
        if (state == true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length() > 0) { break; }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);

    if (getBody.indexOf("The file") != -1) {
      Serial.println("Upload successful!");
    } else {
      Serial.println("Upload failed.");
    }
  } else {
    Serial.println("Connection to " + serverName + " failed.");
  }
    // esp_light_sleep_start();
    delay(2000);
}

void loop() {

  // Serial.println("Motion detected!");
  // delay(1000);
  // CapturePhoto();
  // Serial.println("light sleep start");
  // SendPhoto();

  // // esp_light_sleep_start();
  // delay(10000);
  
}
