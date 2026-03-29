#include <Wire.h>
#include <BH1750.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>


// ====== ตั้งค่า Pin ======
#define DHTPIN 14
#define DHTTYPE DHT11
#define TRIG_PIN 12
#define ECHO_PIN 13
#define BUZZER_PIN 25
#define LED_DISTANCE_PIN 26
#define LED_LIGHT_PIN 19
const int SOIL_PIN = 34;
#define RELAY_PUMP 27
#define RELAY_FAN  33

// ====== Threshold ใหม่ ======
int LIGHT_OPEN = 800;
int LIGHT_CLOSE = 300;

int SOIL_ON = 35;
int SOIL_OFF = 55;

// ====== State ======
bool shadeOpen = false;
bool pumpRunning = false;

// ====== เวลา ======
unsigned long pumpStart = 0;

// ====== ตั้งค่าเกณฑ์ (Threshold) ======
const int SOIL_DRY = 4100; 
const int SOIL_WET = 3100; 

// ====== ตัวแปรสำหรับการจัดการเวลา (millis) ======
unsigned long previousMillis = 0;   // เก็บเวลาล่าสุดที่พิมพ์จอ
const long interval = 5000;        // ตั้งรอบการพิมพ์จอ 5 วินาที (5000ms)

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

LiquidCrystal_I2C lcd(0x27, 20, 4);

#define IN1 2
#define IN2 4

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void motorForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void motorBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
}

void openShade() {
  motorForward();
  delay(2000);
  motorStop();
}

void closeShade() {
  motorBackward();
  delay(2000);
  motorStop();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(23,18); // SDA , SCL
  dht.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_DISTANCE_PIN, OUTPUT);
  pinMode(LED_LIGHT_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

// ปิดรีเลย์ตอนเริ่ม
  digitalWrite(RELAY_PUMP, HIGH);
  digitalWrite(RELAY_FAN, HIGH);


  Serial.println(F("--- ระบบโรงเรือนอัจฉริยะ (โหมด Real-time 5s) เริ่มทำงาน ---"));

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Smart Greenhouse");
  lcd.setCursor(0,1);
  lcd.print("System Starting");
  delay(5000);
  lcd.clear();
}

void loop() {
  // 1. อ่านค่าเซนเซอร์ตลอดเวลา
  unsigned long currentMillis = millis();

  // Ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;

  bool waterOK = (distance < 10);

  // DHT11, Light, Soil
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("DHT error");
    return;
  }
  float lux = lightMeter.readLightLevel();
  int soilRaw = analogRead(SOIL_PIN);
  int soilPercent = map(soilRaw, SOIL_DRY, SOIL_WET, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  // 2. ระบบ Logic (ทำงานทันที ไม่ต้องรอ 5 วินาที)
  
  // แสง -> LED 19
  // ===== คุมสแลน =====
  if (lux > LIGHT_OPEN && !shadeOpen) {
    openShade();
    shadeOpen = true;
  }

  if (lux < LIGHT_CLOSE && shadeOpen) {
    closeShade();
    shadeOpen = false;
  }

  // ระยะทาง -> LED 26
  if (distance > 30) digitalWrite(LED_DISTANCE_PIN, HIGH);
  else digitalWrite(LED_DISTANCE_PIN, LOW);

  // อุณหภูมิ -> fan
  if (t > 30) {
    digitalWrite(RELAY_FAN, LOW);
  } 
  else if (t < 28) {
    digitalWrite(RELAY_FAN, HIGH);
  }

  // 3. ส่วนของการพิมพ์ลง Serial Monitor (ทำงานทุก 5 วินาที)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // อัปเดตเวลาล่าสุด

    Serial.println(F("\n====== [ ข้อมูลเซนเซอร์รอบ 5 วินาที ] ======"));
    Serial.printf("อากาศ: %.1f°C | %.1f%% RH\n", t, h);
    Serial.printf("แสง: %.1f lx | ดิน: %d (%d%%)\n", lux, soilRaw, soilPercent);
    Serial.printf("ระยะทาง: %.2f cm\n", distance);
    
    // พิมพ์สถานะ LED/Buzzer เพื่อเช็ค
    Serial.print("สถานะอุปกรณ์: ");
    Serial.print(lux <= 200 ? "LED19:ON " : "LED19:OFF ");
    Serial.print(distance > 30 ? "LED26:ON " : "LED26:OFF ");
    Serial.println(t > 32.0 ? "BUZZER:WARNING!" : "BUZZER:NORMAL");

    // ===== LCD Display =====
    lcd.clear();

    lcd.setCursor(0,0);
    lcd.print("Temp:");
    lcd.print(t,1);
    lcd.print("C ");

    lcd.print("Hum:");
    lcd.print(h,0);
    lcd.print("%");

    lcd.setCursor(0,1);
    lcd.print("Light:");
    lcd.print(lux,0);
    lcd.print(" lx");

    lcd.setCursor(0,2);
    lcd.print("Soil:");
    lcd.print(soilRaw);      // ค่า raw จริง
    lcd.print(" (");
    lcd.print(soilPercent);  // ค่า %
    lcd.print("%)");

    lcd.setCursor(0,3);
    lcd.print("W:");
    lcd.print(distance,1);
    lcd.print("cm ");

    lcd.setCursor(10,3);
    lcd.print("Fan:");
    lcd.print(digitalRead(RELAY_FAN)==LOW ? "ON" : "OFF");
  }

  //test relay
  if (waterOK) {

    if (soilPercent < SOIL_ON && !pumpRunning) {
      digitalWrite(RELAY_PUMP, LOW);
      pumpStart = millis();
      pumpRunning = true;
    }

    if (pumpRunning && millis() - pumpStart > 5000) {
      digitalWrite(RELAY_PUMP, HIGH);
      pumpRunning = false;
    }

  } else {
    digitalWrite(RELAY_PUMP, HIGH);
    pumpRunning = false;
  }
} 