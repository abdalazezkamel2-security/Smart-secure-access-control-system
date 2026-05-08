#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>

#define RELAY_PIN 2
#define BUZZER_PIN 4
#define LED_PIN 6
#define NUM_LEDS 16 

LiquidCrystal_I2C lcd(0x27, 16, 2); 
Adafruit_NeoPixel ring(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int fadeValue = 10; // ابدأ من قيمة منخفضة
int fadeStep = 2;   // تنفس أهدأ
unsigned long lastFadeTime = 0;
bool isStandby = true;

void setup() {
  Serial.begin(115200); 
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH); 
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  
  lcd.init(); lcd.backlight();
  ring.begin();
  ring.setBrightness(70); // تحديد السطوع الكلي لتقليل سحب الأمبير
  ring.show(); 
  
  initStandbyScreen();
}

void loop() {
  if (isStandby) {
    if (millis() - lastFadeTime > 40) { // تحديث أبطأ قليلاً للاستقرار
      lastFadeTime = millis();
      fadeValue += fadeStep;
      if (fadeValue <= 10 || fadeValue >= 100) fadeStep = -fadeStep; // تنفس خفيف (أبيض هادئ)
      setRingColor(fadeValue, fadeValue, fadeValue); 
    }
  }

  if (Serial.available() > 0) {
    char cmd = Serial.read(); 
    if (cmd == '@') {
      isStandby = false;
      accessGrantedSequence();
      isStandby = true;
      initStandbyScreen();
    } 
    else if (cmd == '$') {
      isStandby = false;
      accessDeniedSequence();
      isStandby = true;
      initStandbyScreen();
    }
  }
}

void initStandbyScreen() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("System Online");
  lcd.setCursor(0, 1); lcd.print("Scan Your Card");
}

void accessGrantedSequence() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Access Granted!");
  lcd.setCursor(0, 1); lcd.print("Welcome Boss");
  
  setRingColor(0, 150, 0); // أخضر (سطوع متوسط للأمان)
  digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW);
  
  digitalWrite(RELAY_PIN, LOW); delay(2500); digitalWrite(RELAY_PIN, HIGH);
}

void accessDeniedSequence() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ACCESS DENIED!");
  lcd.setCursor(0, 1); lcd.print("Intruder Alert!!");
  
  // --- فلاش أحمر متزامن 3 مرات مع البازر ---
  for (int i = 0; i < 3; i++) {
    setRingColor(200, 0, 0); // أحمر ساطع
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300); // زيادة وقت الإنذار بناءً على طلبك
    
    setRingColor(0, 0, 0); 
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void setRingColor(int r, int g, int b) {
  for(int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(r, g, b));
  }
  ring.show(); 
}