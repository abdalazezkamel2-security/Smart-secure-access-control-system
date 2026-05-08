#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>

// --- إيقاف الرسترة المفاجئة ---
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- بيانات الشبكة والبوت ---
const char* ssid = "hamada.gana"; 
const char* password = "shery@hamda"; 
String BOTtoken = "8410005003:AAE6Y3qgZ9w4_kH6b34OPYv17t0fCuHdHq8"; 
String CHAT_ID = "1628414543"; 

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// --- إعدادات دبابيس الكاميرا ---
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

// --- إعدادات دبابيس الـ RFID ---
#define SCK_PIN   14
#define MISO_PIN  13
#define MOSI_PIN  12
#define SS_PIN    15
#define RST_PIN   -1 

MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- كارت المدير (Master Card) ---
String masterCard = "C4 BE C9 06"; 

// --- دالة إرسال الصورة لتليجرام (كاملة بتفاصيل السيريال الأصلية) ---
String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  camera_fb_t * fb = NULL;
  
  Serial.println("[TELEGRAM] Clearing old frame from RAM...");
  fb = esp_camera_fb_get();  
  if (fb) esp_camera_fb_return(fb); 
  delay(150); 
  
  Serial.println("[TELEGRAM] Taking new high-speed security photo...");
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("[ERROR] Camera capture failed! Check camera flex cable.");
    return "Camera capture failed";
  }
  Serial.println("[TELEGRAM] Photo captured successfully. Size: " + String(fb->len) + " bytes");
  
  Serial.println("[TELEGRAM] Connecting to Telegram API Server..."); 
  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("[TELEGRAM] Connection established. Uploading payload...");
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";
    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
    
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
    
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) { clientTCP.write(fbBuf, 1024); fbBuf += 1024; }
      else if (fbLen%1024>0) { size_t remainder = fbLen%1024; clientTCP.write(fbBuf, remainder); }
    }  
    clientTCP.print(tail);
    esp_camera_fb_return(fb); 
    
    Serial.println("[TELEGRAM] Payload sent. Waiting for API response (Max 1.5s)...");
    int waitTime = 1500; 
    long startTimer = millis();
    boolean state = false;
    while ((startTimer + waitTime) > millis()){
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') getAll += String(c);        
        if (getAll.substring(getAll.length()-2, getAll.length()) == "\r\n") state = true;
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
    Serial.println("[TELEGRAM] Done! Photo delivered.");
  } else {
    esp_camera_fb_return(fb);
    Serial.println("[ERROR] Failed to connect to Telegram API.");
  }
  return getBody;
}

void triggerAlarm() {
  Serial.println("[ALARM] UNAUTHORIZED ACCESS DETECTED! Triggering sequence...");
  Serial.println("[ALARM] Sending alert text to Telegram...");
  bot.sendMessage(CHAT_ID, "⚠️ تنبيه أمني: محاولة دخول بكارت غير مصرح به!", "");
  
  // إرسال إشارة الإنذار للنانو (قبل بدء عملية الرفع الثقيلة)
  Serial.write('$'); 
  
  sendPhotoTelegram();
  Serial.println("[ALARM] Sequence completed. System returning to standby.");
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  Serial.println("\n\n[INIT] ---------------------------------");
  Serial.println("[INIT] Booting ESP32-CAM Security System (MASTER)");

  Serial.println("[WIFI] Connecting to SSID: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  while (WiFi.status() != WL_CONNECTED) { 
    delay(1000); 
    Serial.print("."); 
  }
  Serial.println("\n[WIFI] Connected! IP Address: " + WiFi.localIP().toString());

  Serial.println("[CAM] Initializing Camera Module...");
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA; 
  config.jpeg_quality = 12; 
  config.fb_count = 1; 
  config.grab_mode = CAMERA_GRAB_LATEST; 

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed with error 0x%x\n", err);
    return;
  }
  Serial.println("[CAM] Camera initialized successfully.");
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);       
  s->set_hmirror(s, 1);     
  
  Serial.println("[RFID] Initializing SPI Bus and MFRC522 Reader...");
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();
  
  // --- رفع حساسية الأنتينا (التعديل الهندسي) ---
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); 
  Serial.println("[RFID] Antenna Gain set to MAX for better sensitivity.");
  
  Serial.println("[RFID] Reader initialized.");
  
  Serial.println("[INIT] Sending Startup Message to Telegram...");
  bot.sendMessage(CHAT_ID, "✅ النظام الأمني جاهز. (System Online)", "");
  
  Serial.println("[INIT] Boot complete. Entering Standby Mode.");
  Serial.println("----------------------------------------");
}

void loop() {
  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("\n[EVENT] --------------------------------");
  Serial.println("[RFID] New Card Detected! Reading UID...");

  String scannedCard = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    scannedCard += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    scannedCard += String(mfrc522.uid.uidByte[i], HEX);
  }
  scannedCard.trim();
  scannedCard.toUpperCase();

  Serial.println("[AUTH] Scanned UID: [" + scannedCard + "]");
  Serial.println("[AUTH] Comparing with Master Card...");

  if (scannedCard == masterCard) {
    Serial.println("[AUTH] Match found! Access Granted.");
    
    // إرسال الإشارة للنانو فوراً (لفتح الريلاي وتغيير الليدات)
    Serial.write('@');
    
    Serial.println("[TELEGRAM] Sending Access Granted notification...");
    bot.sendMessage(CHAT_ID, "✅ تم الدخول بنجاح (كارت المدير).", "");
    
    delay(2000); 

    // --- الترقيع البرمجي (Software Patch) للهروب من التهنيجة ---
    Serial.println("[SYSTEM] Applying SPI Software Patch...");
    SPI.end(); 
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN); 
    mfrc522.PCD_Init(); 
    mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); // إعادة تفعيل الحساسية القصوى بعد الرسترة
    Serial.println("[RECOVERY] RFID reset successful. Ready for next card.");

  } else {
    Serial.println("[AUTH] NO MATCH! Access Denied.");
    // دالة الإنذار ترسل الحرف '$' للنانو وتبدأ التصوير
    triggerAlarm();
  }

  mfrc522.PICC_HaltA();
  Serial.println("[RFID] Halting card to prevent multi-reads.");
  Serial.println("----------------------------------------");
  delay(500); 
}