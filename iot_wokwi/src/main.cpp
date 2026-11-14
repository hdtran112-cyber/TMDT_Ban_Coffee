#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ==============================================================================
// 1. CẤU HÌNH THÔNG TIN HỆ THỐNG THEO ĐẶC TẢ KỸ THUẬT (FULL RIG)
// ==============================================================================
const char* GROUP_NAME       = "12_DH-THMT_Nhom14";
const char* DEFAULT_BATCH    = "B20261027-02";

// Ngưỡng nhiệt độ cảnh báo
const float WARN_TEMP        = 195.0; // Bắt đầu cảnh báo đèn vàng
const float ALERT_MAX_TEMP   = 205.0; // Quá nhiệt nguy hiểm -> Bật còi buzzer & đèn đỏ

// Cấu hình mạng WiFi (Wokwi ảo)
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// Cấu hình Django Backend API (host.wokwi.internal hoặc 10.0.2.2)
String djangoApiUrl     = "http://host.wokwi.internal:8000/api/iot/readings/";
String fallbackApiUrl   = "http://10.0.2.2:8000/api/iot/readings/";
String deviceKey        = "my-test-secret-key-123";
String currentBatchCode = DEFAULT_BATCH;

// ==============================================================================
// 2. ĐỊNH NGHĨA CHÂN PHẦN CỨNG (PINOUT THEO ĐẶC TẢ FULL RIG)
// ==============================================================================
#define PIN_DHT          4   // DHT22: Cảm biến nhiệt - ẩm môi trường
#define PIN_POT_GAS      34  // Potentiometer: Chiết áp chỉnh Lửa / Gas (0-100%)
#define PIN_POT_AIR      35  // Potentiometer: Chiết áp chỉnh Gió / Airflow (0-100%)
#define PIN_SERVO        18  // Servo: Động cơ van gió / Quạt hút (0 - 180 độ)
#define PIN_BTN_DUMP     19  // Pushbutton: Xả mẻ / Làm nguội khẩn cấp (INPUT_PULLUP)
#define PIN_LED_GREEN    12  // LED Xanh: Normal / Roasting (<= 195°C)
#define PIN_LED_YELLOW   14  // LED Vàng: Cảnh báo sắp First Crack (> 195°C)
#define PIN_LED_RED      27  // LED Đỏ: Báo động quá nhiệt (> 205°C)
#define PIN_BUZZER       26  // Còi Buzzer báo động quá nhiệt
#define I2C_SDA          21  // LCD 2004 SDA
#define I2C_SCL          22  // LCD 2004 SCL

// Khởi tạo các ngoại vi
LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(PIN_DHT, DHT22);
Servo exhaustServo;
WebServer server(80);

// ==============================================================================
// 3. ĐƯỜNG CONG TỰ ĐỘNG CHUẨN (AUTO PROFILE) & BIẾN TRẠNG THÁI
// ==============================================================================
int elapsedSeconds       = 0;
float currentTemp        = 130.0;
float baseTemp           = 130.0;
float gasEffect          = 0.0;
float airCooling         = 0.0;

float gasPercent         = 65.0;
float airPercent         = 25.0;
int servoAngle           = 45;

float envTemp            = 28.0;
float envHumidity        = 60.0;

bool isAutoMode          = true;  // MẶC ĐỊNH KHỞI ĐỘNG CHẾ ĐỘ TỰ ĐỘNG CHẠY (AUTO)
bool manualInterference  = false; // Phát hiện người dùng tự ý chỉnh nhiệt độ lên cao
bool isAlert             = false; // Quá nhiệt (> 205°C)
bool isWarning           = false; // Giai đoạn cảnh báo (195°C - 205°C)
bool emergencyDumpActive = false; // Xả nhiệt khẩn cấp
bool manualOverride      = false; // Can thiệp từ giao diện Web
float overrideGas        = 50.0;
float overrideAir        = 30.0;
bool lastButtonState     = HIGH;
String roastingPhase     = "DRYING";

int lastHttpCode         = 0;
String lastApiStatus     = "Chưa gửi";

unsigned long lastTickTime    = 0;
unsigned long lastApiSendTime = 0;
const unsigned long API_INTERVAL = 3000; // Gửi dữ liệu mỗi 3 giây

// Đường cong rang tự động chuẩn công nghiệp (S-Curve Profile 112°C -> 202.5°C)
// Đảm bảo ở chế độ TỰ ĐỘNG luôn luôn an toàn (< 205°C)
struct AutoPoint {
  int second;
  float temp;
  float gas;
  float air;
};

const AutoPoint autoProfile[] = {
  {0,   130.0, 68.0, 20.0}, // Nạp hạt (Charge Temp)
  {30,  112.0, 72.0, 22.0}, // Turning Point
  {60,  128.0, 75.0, 25.0}, // Sấy hạt (Drying)
  {120, 155.0, 68.0, 32.0}, // Vàng hóa (Yellowing)
  {180, 172.0, 62.0, 40.0}, // Phản ứng Maillard
  {240, 188.0, 52.0, 48.0}, // Tiếp cận First Crack
  {300, 197.5, 45.0, 52.0}, // Nổ đợt 1 (First Crack - Đèn Vàng)
  {360, 202.0, 38.0, 60.0}  // Phát triển (Development - Đạt tối đa 202°C < 205°C)
};
const int PROFILE_SIZE = sizeof(autoProfile) / sizeof(autoProfile[0]);

float getAutoBaseTemp(int sec) {
  if (sec <= autoProfile[0].second) return autoProfile[0].temp;
  int maxSec = autoProfile[PROFILE_SIZE - 1].second;
  if (sec >= maxSec) return autoProfile[PROFILE_SIZE - 1].temp;

  for (int i = 0; i < PROFILE_SIZE - 1; i++) {
    if (sec >= autoProfile[i].second && sec <= autoProfile[i+1].second) {
      float ratio = (float)(sec - autoProfile[i].second) / (autoProfile[i+1].second - autoProfile[i].second);
      return autoProfile[i].temp + ratio * (autoProfile[i+1].temp - autoProfile[i].temp);
    }
  }
  return 202.0;
}

float getAutoGas(int sec) {
  if (sec <= autoProfile[0].second) return autoProfile[0].gas;
  int maxSec = autoProfile[PROFILE_SIZE - 1].second;
  if (sec >= maxSec) return autoProfile[PROFILE_SIZE - 1].gas;
  for (int i = 0; i < PROFILE_SIZE - 1; i++) {
    if (sec >= autoProfile[i].second && sec <= autoProfile[i+1].second) {
      float ratio = (float)(sec - autoProfile[i].second) / (autoProfile[i+1].second - autoProfile[i].second);
      return autoProfile[i].gas + ratio * (autoProfile[i+1].gas - autoProfile[i].gas);
    }
  }
  return 38.0;
}

float getAutoAir(int sec) {
  if (sec <= autoProfile[0].second) return autoProfile[0].air;
  int maxSec = autoProfile[PROFILE_SIZE - 1].second;
  if (sec >= maxSec) return autoProfile[PROFILE_SIZE - 1].air;
  for (int i = 0; i < PROFILE_SIZE - 1; i++) {
    if (sec >= autoProfile[i].second && sec <= autoProfile[i+1].second) {
      float ratio = (float)(sec - autoProfile[i].second) / (autoProfile[i+1].second - autoProfile[i].second);
      return autoProfile[i].air + ratio * (autoProfile[i+1].air - autoProfile[i].air);
    }
  }
  return 60.0;
}

// Xác định 4 pha rang theo đặc tả
String determinePhase(float temp) {
  if (temp < 138.0) return "DRYING";
  if (temp < 172.0) return "YELLOWING";
  if (temp <= 196.0) return "FIRST CRACK";
  return "DEVELOPMENT";
}

// ==============================================================================
// 4. ĐỌC CẢM BIẾN, TÍNH TOÁN CÂN BẰNG NHIỆT & ĐIỀU KHIỂN CHẤP HÀNH
// ==============================================================================
void updatePhysicsAndControls() {
  // 1. Đọc nút bấm xả mẻ khẩn cấp (Active LOW với INPUT_PULLUP)
  bool btnState = digitalRead(PIN_BTN_DUMP);
  if (btnState == LOW && lastButtonState == HIGH) {
    emergencyDumpActive = !emergencyDumpActive; // Đảo trạng thái khẩn cấp
    if (emergencyDumpActive) {
      manualOverride = false;
      manualInterference = false;
    }
    delay(50); // Chống dội phím
  }
  lastButtonState = btnState;

  // 2. Đọc chiết áp Lửa (Gas) và Gió (Airflow)
  int rawGas = analogRead(PIN_POT_GAS);
  int rawAir = analogRead(PIN_POT_AIR);
  float measuredGas = ((float)rawGas / 4095.0) * 100.0;
  float measuredAir = ((float)rawAir / 4095.0) * 100.0;

  // Kiểm tra thao tác vặn biến trở Gas:
  // Nếu người dùng xoay núm Gas lên cao (> 58%) -> Phát hiện TỰ Ý CHỈNH NHIỆT ĐỘ LÊN CAO!
  static int prevGasRaw = -1;
  if (prevGasRaw >= 0 && abs(rawGas - prevGasRaw) > 60) {
    // Có sự thay đổi vị trí núm vặn thực tế -> huỷ override web để ưu tiên núm xoay
    manualOverride = false;
  }
  prevGasRaw = rawGas;

  float autoGasVal = getAutoGas(elapsedSeconds);
  float autoAirVal = getAutoAir(elapsedSeconds);

  // Xử lý các chế độ can thiệp:
  if (manualOverride) {
    // Người dùng điều khiển qua Web Dashboard (Nút bấm hoặc Slider trên Web)
    gasPercent = overrideGas;
    airPercent = overrideAir;
    manualInterference = (overrideGas >= 60.0);
    isAutoMode = !manualInterference;
  }
  else if (measuredGas >= 58.0) {
    // Người dùng vặn núm Gas trên mạch Wokwi vượt 58% -> Tự ý tăng nhiệt độ
    manualInterference = true;
    isAutoMode = false;
    gasPercent = measuredGas;
    airPercent = measuredAir;
  }
  else if (manualInterference && measuredGas <= 45.0) {
    // Người dùng vặn núm Gas về lại mức an toàn -> Khôi phục chế độ Tự Động
    manualInterference = false;
    isAutoMode = true;
  }

  if (isAutoMode && !manualOverride && !manualInterference) {
    // CHẾ ĐỘ TỰ ĐỘNG CHẠY HOÀN TOÀN THEO PROFILE MẺ RANG CHUẨN
    gasPercent = autoGasVal;
    airPercent = autoAirVal;
  }

  // Chế độ xả nhiệt khẩn cấp
  if (emergencyDumpActive) {
    gasPercent = 0.0;
    airPercent = 100.0;
    manualInterference = false;
  }

  // 3. Điều khiển Servo góc van gió (0 - 180 độ)
  servoAngle = map((int)airPercent, 0, 100, 0, 180);
  if (servoAngle < 0) servoAngle = 0;
  if (servoAngle > 180) servoAngle = 180;
  exhaustServo.write(servoAngle);

  // 4. Đọc cảm biến DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    envHumidity = h;
    envTemp = t;
  }

  // 5. Tính toán Nhiệt độ hạt (Bean Temp):
  // - Khi TỰ ĐỘNG: nhiệt độ bám sát đường cong autoBaseTemp chuẩn (tối đa 202°C < 205°C, LUÔN AN TOÀN)
  // - Khi TỰ Ý TĂNG NHIỆT ĐỘ: nhiệt độ BẮT BUỘC vượt 205°C (207.5°C - 225°C) -> LẬP TỨC BÁO LỖI QUÁ NHIỆT!
  baseTemp = getAutoBaseTemp(elapsedSeconds);

  if (emergencyDumpActive) {
    currentTemp = currentTemp - 2.5; // Hạ nhiệt nhanh chóng khi xả mẻ
    if (currentTemp < 95.0) currentTemp = 95.0;
    gasEffect = 0.0;
    airCooling = 25.0;
  }
  else if (manualInterference || gasPercent >= 60.0) {
    // TỰ Ý CHỈNH NHIỆT LÊN CAO:
    // Đảm bảo vượt ngưỡng 205.0°C để kích hoạt báo động quá nhiệt (Alert)
    float extra = (gasPercent - 60.0);
    if (extra < 0) extra = 0;
    currentTemp = 207.5 + (extra / 40.0) * 18.0; // Khoảng từ 207.5°C đến 225.5°C
    gasEffect = currentTemp - baseTemp;
    airCooling = (airPercent / 100.0) * 15.0;
    currentTemp -= airCooling * 0.15; // Gió làm mát nhẹ nhưng vẫn giữ quá nhiệt
  }
  else {
    // CHẾ ĐỘ TỰ ĐỘNG CHUẨN
    gasEffect = 0.0;
    airCooling = 0.0;
    currentTemp = baseTemp + ((float)(random(-2, 3)) * 0.1); // Dao động nhẹ ±0.2°C
    if (currentTemp > 202.5) currentTemp = 202.5; // Giới hạn an toàn tuyệt đối
  }

  if (currentTemp < 90.0) currentTemp = 90.0;
  if (currentTemp > 240.0) currentTemp = 240.0;

  roastingPhase = determinePhase(currentTemp);

  // 6. Xử lý trạng thái cảnh báo & Điều khiển LED / Buzzer
  if (currentTemp > ALERT_MAX_TEMP) {
    // LỖI QUÁ NHIỆT NGUY HIỂM (> 205°C) -> LED Đỏ sáng + Còi Buzzer hú báo động
    isAlert = true;
    isWarning = false;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_BUZZER, 1200, 250); // Còi kêu tít tít báo động sự cố
  } else if (currentTemp > WARN_TEMP) {
    // Cảnh báo giai đoạn First Crack (195°C - 205°C) -> LED Vàng sáng
    isAlert = false;
    isWarning = true;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
  } else {
    // Trạng thái bình thường an toàn (<= 195°C) -> LED Xanh sáng
    isAlert = false;
    isWarning = false;
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
  }
}

// ==============================================================================
// 5. HIỂN THỊ MÀN HÌNH LCD 20x4 CHUẨN ĐẶC TẢ
// ==============================================================================
void updateLCD() {
  // Dòng 0: Mã mẻ & Trạng thái: [AUTO], [MANU], [ALRT] hoặc [DUMP]
  lcd.setCursor(0, 0);
  char line0[21];
  const char* stTag = "[AUTO]";
  if (isAlert) stTag = "[ALRT]";
  else if (emergencyDumpActive) stTag = "[DUMP]";
  else if (manualInterference)  stTag = "[MANU]";

  snprintf(line0, sizeof(line0), "B:%-10s %s", currentBatchCode.c_str(), stTag);
  lcd.print(line0);

  // Dòng 1: BT:198.5C | Env:28C
  lcd.setCursor(0, 1);
  char line1[21];
  snprintf(line1, sizeof(line1), "BT:%5.1fC|Env:%2.0fC %2.0f%%", currentTemp, envTemp, envHumidity);
  lcd.print(line1);

  // Dòng 2: Gas: 75%  | Air: 40%
  lcd.setCursor(0, 2);
  char line2[21];
  snprintf(line2, sizeof(line2), "Gas:%3.0f%% | Air:%3.0f%%", gasPercent, airPercent);
  lcd.print(line2);

  // Dòng 3: Phase: DRYING / YELLOWING / FIRST CRACK / DEVELOPMENT
  lcd.setCursor(0, 3);
  char line3[21];
  snprintf(line3, sizeof(line3), "Phase: %-13s", roastingPhase.c_str());
  lcd.print(line3);
}

// ==============================================================================
// 6. GỬI DỮ LIỆU HTTP POST VỀ BACKEND DJANGO REST FRAMEWORK (CHUẨN 100%)
// ==============================================================================
void sendReadingToBackend() {
  if (WiFi.status() != WL_CONNECTED) {
    lastApiStatus = "Mất kết nối WiFi";
    return;
  }

  server.handleClient();

  // Payload chuẩn hợp đồng: {"batch_code": "B20261027-02", "elapsed_s": 90, "temperature_c": 198.5}
  String payload = "{\"batch_code\":\"" + currentBatchCode + "\",\"elapsed_s\":" + String(elapsedSeconds) + ",\"temperature_c\":" + String(currentTemp, 1) + "}";

  HTTPClient http;
  http.begin(djangoApiUrl);
  http.setConnectTimeout(300);
  http.setTimeout(300);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  int httpCode = http.POST(payload);
  if (httpCode <= 0) {
    // Fallback sang IP 10.0.2.2 nếu host.wokwi.internal không phản hồi
    http.end();
    http.begin(fallbackApiUrl);
    http.setConnectTimeout(300);
    http.setTimeout(300);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", deviceKey);
    httpCode = http.POST(payload);
  }
  lastHttpCode = httpCode;

  if (httpCode > 0) {
    lastApiStatus = String(httpCode) + " " + (httpCode == 201 ? "Created" : "OK");
  } else {
    lastApiStatus = "Lỗi HTTP: " + http.errorToString(httpCode);
  }

  Serial.printf("[IoT] Giây: %ds | BT: %.1f°C | Gas: %.0f%% | Air: %.0f%% (Servo %d°) | Phase: %s | Mode: %s | Trang thai: %s | API: %s\n",
    elapsedSeconds, currentTemp, gasPercent, airPercent, servoAngle, roastingPhase.c_str(),
    isAutoMode ? "TỰ ĐỘNG" : "CAN THIỆP THỦ CÔNG",
    isAlert ? "QUÁ NHIỆT / LỖI!" : (isWarning ? "CẢNH BÁO" : "BÌNH THƯỜNG"),
    lastApiStatus.c_str());

  http.end();
  server.handleClient();
}

// ==============================================================================
// 7. GIAO DIỆN WEB DASHBOARD LOCALHOST (:8088 QUA WOKWI NET.FORWARD)
// ==============================================================================
const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>☕ Trạm Giám Sát & Điều Khiển Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14</title>
  <style>
    :root {
      --primary: #5c3a21;
      --bg: #0f141c;
      --card-bg: #1a2230;
      --card-border: #2a374d;
      --text: #e1e7ef;
      --text-muted: #8c9ba5;
      --danger: #ef4444;
      --warning: #f59e0b;
      --success: #10b981;
      --accent: #38bdf8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; }
    .container { max-width: 960px; margin: 0 auto; }
    .header {
      background: linear-gradient(135deg, #2b1704 0%, #4a2810 50%, #1f2937 100%);
      padding: 22px 26px;
      border-radius: 16px;
      margin-bottom: 20px;
      border: 1px solid #4a3424;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 10px 25px rgba(0,0,0,0.4);
    }
    .header h1 { font-size: 22px; font-weight: 700; color: #fde68a; }
    .header p { font-size: 14px; color: #d1d5db; margin-top: 4px; }
    .badge-group { background: #374151; color: #60a5fa; padding: 6px 16px; border-radius: 20px; font-weight: 700; font-size: 14px; }
    
    .status-banner {
      padding: 16px 22px;
      border-radius: 12px;
      margin-bottom: 20px;
      font-weight: 700;
      font-size: 15px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      transition: all 0.3s ease;
    }
    .banner-auto { background: rgba(16,185,129,0.15); color: #34d399; border: 1px solid #10b981; }
    .banner-warn { background: rgba(245,158,11,0.2); color: #fbbf24; border: 1px solid #f59e0b; }
    .banner-alert { background: rgba(239,68,68,0.25); color: #f87171; border: 1px solid #ef4444; animation: pulse 1s infinite; }
    @keyframes pulse { 0% { opacity: 0.7; } 50% { opacity: 1; } 100% { opacity: 0.7; } }

    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin-bottom: 20px; }
    .card {
      background: var(--card-bg);
      border-radius: 14px;
      padding: 22px;
      border: 1px solid var(--card-border);
      box-shadow: 0 4px 15px rgba(0,0,0,0.2);
    }
    .card-title {
      font-size: 15px;
      font-weight: 700;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-bottom: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid var(--card-border);
      padding-bottom: 10px;
    }
    .metric-hero { font-size: 42px; font-weight: 800; color: #f59e0b; margin: 8px 0; }
    .metric-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px dashed #222d3d; font-size: 14px; }
    .metric-row:last-child { border-bottom: none; }
    .badge { padding: 4px 10px; border-radius: 12px; font-weight: 700; font-size: 12px; }
    .badge-ok { background: rgba(16,185,129,0.2); color: var(--success); border: 1px solid var(--success); }
    .badge-warn { background: rgba(245,158,11,0.2); color: var(--warning); border: 1px solid var(--warning); }
    .badge-alert { background: rgba(239,68,68,0.2); color: var(--danger); border: 1px solid var(--danger); animation: pulse 1s infinite; }
    
    .progress-bar-bg { background: #111827; height: 10px; border-radius: 5px; overflow: hidden; margin-top: 6px; }
    .progress-bar { height: 100%; border-radius: 5px; transition: width 0.3s ease; }
    .progress-gas { background: linear-gradient(90deg, #f59e0b, #ef4444); }
    .progress-air { background: linear-gradient(90deg, #38bdf8, #3b82f6); }
    
    .leds { display: flex; gap: 14px; margin-top: 10px; align-items: center; }
    .led-pill { display: flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 600; padding: 6px 12px; border-radius: 20px; background: #111827; }
    .led-circle { width: 12px; height: 12px; border-radius: 50%; background: #444; transition: 0.3s; }
    .led-green-on { background: #10b981; box-shadow: 0 0 10px #10b981; }
    .led-yellow-on { background: #f59e0b; box-shadow: 0 0 10px #f59e0b; }
    .led-red-on { background: #ef4444; box-shadow: 0 0 14px #ef4444; animation: pulse 0.8s infinite; }
    
    .btn-action { width: 100%; padding: 12px; border: none; border-radius: 8px; font-weight: 700; cursor: pointer; transition: 0.2s; font-size: 13px; }
    .btn-action:hover { filter: brightness(1.2); transform: translateY(-1px); }
    
    .slider-box { margin-top: 14px; padding: 10px; background: #111827; border-radius: 8px; }
    .slider-label { display: flex; justify-content: space-between; font-size: 13px; font-weight: 600; margin-bottom: 6px; }
    input[type=range] { width: 100%; accent-color: #ef4444; cursor: pointer; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div>
        <h1>☕ Trạm Giám Sát & Điều Khiển Lò Rang Cà Phê IoT</h1>
        <p>Hệ thống TMDT_Ban_Coffee &bull; Module B: Roasting Telemetry & IoT Control</p>
      </div>
      <div>
        <span class="badge-group">Nhóm: 12_ĐH-THMT_Nhom14</span>
      </div>
    </div>

    <!-- Live Status Banner -->
    <div id="modeBanner" class="status-banner banner-auto">
      <span id="bannerText">🤖 HỆ THỐNG ĐANG TỰ ĐỘNG CHẠY THEO ĐƯỜNG CONG RANG TIÊU CHUẨN</span>
      <span id="bannerSub" style="font-size: 13px; opacity: 0.9;">AN TOÀN</span>
    </div>

    <div class="grid">
      <!-- Card 1: Bean Temperature & Phase -->
      <div class="card">
        <div class="card-title">
          <span>🌡️ Nhiệt Độ Hạt (Bean Temp)</span>
          <span id="badgeStatus" class="badge badge-ok">[OK] AN TOÀN</span>
        </div>
        <div class="metric-hero" id="tempBean">-- °C</div>
        <div class="metric-row">
          <span>Giai đoạn rang (Phase):</span>
          <strong id="phaseText" style="color: var(--accent); font-size: 15px;">DRYING</strong>
        </div>
        <div class="metric-row">
          <span>Thời gian mẻ (Elapsed):</span>
          <strong id="elapsedText">0s</strong>
        </div>
        <div class="metric-row">
          <span>Mã mẻ rang (Batch Code):</span>
          <strong>B20261027-02</strong>
        </div>
        <div class="metric-row">
          <span>Ngưỡng cảnh báo quá nhiệt:</span>
          <span style="color: var(--danger); font-weight: 700;">&gt; 205.0 °C</span>
        </div>
      </div>

      <!-- Card 2: Gas & Airflow Dynamics -->
      <div class="card">
        <div class="card-title">
          <span>🎛️ Cân Bằng Nhiệt Lửa & Gió</span>
          <span id="modeBadge" class="badge" style="background:#10b981; color:#fff;">TỰ ĐỘNG</span>
        </div>
        <div style="margin: 12px 0;">
          <div style="display:flex; justify-content:space-between; font-size:14px; font-weight:600;">
            <span>🔥 Công suất Lửa (Gas / Burner):</span>
            <span id="gasVal">0%</span>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar progress-gas" id="gasBar" style="width: 0%;"></div>
          </div>
        </div>
        <div style="margin: 16px 0;">
          <div style="display:flex; justify-content:space-between; font-size:14px; font-weight:600;">
            <span>💨 Lưu lượng Gió (Airflow Damper):</span>
            <span id="airVal">0%</span>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar progress-air" id="airBar" style="width: 0%;"></div>
          </div>
        </div>
        <div class="metric-row">
          <span>Góc mở van Servo (GPIO 18):</span>
          <strong id="servoVal" style="color: var(--accent);">0°</strong>
        </div>

        <!-- Slider can thiệp trực tiếp -->
        <div class="slider-box">
          <div class="slider-label">
            <span>🔥 Thanh trượt Lửa Gas (Kéo &gt;60% để gây lỗi):</span>
            <span id="sliderGasVal" style="color:#f59e0b; font-weight:700;">50%</span>
          </div>
          <input type="range" id="gasSlider" min="0" max="100" value="50" oninput="onGasSliderChange(this.value)">
        </div>

        <!-- Interactive Control Buttons -->
        <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 14px;">
          <button class="btn-action" style="background:#10b981; color:#fff;" onclick="forceAuto()">🤖 Chạy Tự Động Chuẩn</button>
          <button class="btn-action" style="background:#dc2626; color:#fff;" onclick="forceOverheat()">🔥 Tự Ý Tăng Quá Nhiệt</button>
          <button class="btn-action" style="background:#3b82f6; color:#fff;" onclick="toggleDump()">💨 Xả Gió Khẩn Cấp</button>
          <button class="btn-action" style="background:#374151; color:#fff;" onclick="resetTimer()">⏱️ Đặt Lại Mẻ Mới (0s)</button>
        </div>
      </div>

      <!-- Card 3: Environment & Backend Sync -->
      <div class="card">
        <div class="card-title">
          <span>🏭 Môi Trường & Backend Django</span>
        </div>
        <div class="metric-row">
          <span>Nhiệt độ phòng (DHT22):</span>
          <strong id="envT">-- °C</strong>
        </div>
        <div class="metric-row">
          <span>Độ ẩm hạt / Ống xả:</span>
          <strong id="envH">-- %</strong>
        </div>
        <div class="metric-row">
          <span>Đồng bộ Django API:</span>
          <strong id="apiStatus" style="color: var(--success); font-size: 13px;">Đang gửi...</strong>
        </div>
        <div style="margin-top: 18px;">
          <span style="font-size: 13px; color: var(--text-muted); font-weight: 600;">TRẠNG THÁI ĐÈN CẢNH BÁO (3 MỨC):</span>
          <div class="leds">
            <div class="led-pill"><div class="led-circle" id="ledG"></div> Xanh (&le;195°C)</div>
            <div class="led-pill"><div class="led-circle" id="ledY"></div> Vàng (&gt;195°C)</div>
            <div class="led-pill"><div class="led-circle" id="ledR"></div> Đỏ (&gt;205°C)</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    function updateData() {
      fetch('/api/status')
        .then(function(res) { return res.json(); })
        .then(function(d) {
          document.getElementById('tempBean').innerText = d.temperature_c.toFixed(1) + ' °C';
          document.getElementById('phaseText').innerText = d.phase;
          var m = Math.floor(d.elapsed_s / 60);
          var s = d.elapsed_s % 60;
          document.getElementById('elapsedText').innerText = (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s + ' (' + d.elapsed_s + 's)';
          
          document.getElementById('gasVal').innerText = d.gas_percent.toFixed(0) + '%';
          document.getElementById('gasBar').style.width = d.gas_percent + '%';
          
          document.getElementById('airVal').innerText = d.air_percent.toFixed(0) + '%';
          document.getElementById('airBar').style.width = d.air_percent + '%';
          document.getElementById('servoVal').innerText = d.servo_angle + '°';
          
          document.getElementById('envT').innerText = d.env_temp.toFixed(1) + ' °C';
          document.getElementById('envH').innerText = d.env_humidity.toFixed(0) + ' %';
          document.getElementById('apiStatus').innerText = d.last_api_status;

          var bStatus = document.getElementById('badgeStatus');
          var mBadge = document.getElementById('modeBadge');
          var banner = document.getElementById('modeBanner');
          var bText = document.getElementById('bannerText');
          var bSub = document.getElementById('bannerSub');
          var lG = document.getElementById('ledG');
          var lY = document.getElementById('ledY');
          var lR = document.getElementById('ledR');

          if (d.is_alert) {
            banner.className = 'status-banner banner-alert';
            bText.innerText = '🚨 BÁO ĐỘNG SỰ CỐ: QUÁ NHIỆT LỒNG RANG (' + d.temperature_c.toFixed(1) + '°C > 205°C)! DO TỰ Ý TĂNG NHIỆT ĐỘ!';
            bSub.innerText = 'SỰ CỐ!';
            bStatus.className = 'badge badge-alert';
            bStatus.innerText = '⚠️ [ALRT] QUÁ NHIỆT / LỖI!';
            mBadge.innerText = 'CAN THIỆP';
            mBadge.style.background = '#dc2626';
            lG.className = 'led-circle';
            lY.className = 'led-circle';
            lR.className = 'led-circle led-red-on';
          } else if (d.emergency_dump) {
            banner.className = 'status-banner banner-warn';
            bText.innerText = '💨 ĐANG XẢ NHIỆT KHẨN CẤP (QUẠT HÚT 100%, TẮT LỬA 0%)';
            bSub.innerText = 'DUMP ACTIVE';
            bStatus.className = 'badge badge-warn';
            bStatus.innerText = '[DUMP] LÀM NGUỘI';
            mBadge.innerText = 'XẢ MẺ';
            mBadge.style.background = '#3b82f6';
            lG.className = 'led-circle led-green-on';
            lY.className = 'led-circle';
            lR.className = 'led-circle';
          } else if (d.is_warning) {
            banner.className = 'status-banner banner-warn';
            bText.innerText = '⚡ GIAI ĐOẠN FIRST CRACK: LÒ ĐANG Ở VÙNG NHIỆT CAO (195°C - 205°C)';
            bSub.innerText = 'CẢNH BÁO';
            bStatus.className = 'badge badge-warn';
            bStatus.innerText = '⚡ [WARN] FIRST CRACK';
            mBadge.innerText = d.is_auto ? 'TỰ ĐỘNG' : 'CAN THIỆP';
            mBadge.style.background = '#f59e0b';
            lG.className = 'led-circle';
            lY.className = 'led-circle led-yellow-on';
            lR.className = 'led-circle';
          } else {
            banner.className = 'status-banner banner-auto';
            bText.innerText = '🤖 HỆ THỐNG ĐANG TỰ ĐỘNG CHẠY THEO ĐƯỜNG CONG RANG TIÊU CHUẨN (AN TOÀN)';
            bSub.innerText = 'AN TOÀN';
            bStatus.className = 'badge badge-ok';
            bStatus.innerText = '✅ [OK] AN TOÀN';
            mBadge.innerText = 'TỰ ĐỘNG';
            mBadge.style.background = '#10b981';
            lG.className = 'led-circle led-green-on';
            lY.className = 'led-circle';
            lR.className = 'led-circle';
          }
        })
        .catch(function(e) { console.error(e); });
    }

    function forceOverheat() {
      fetch('/api/force_overheat', { method: 'POST' }).then(updateData);
    }
    function forceAuto() {
      fetch('/api/force_auto', { method: 'POST' }).then(updateData);
    }
    function toggleDump() {
      fetch('/api/dump', { method: 'POST' }).then(updateData);
    }
    function resetTimer() {
      fetch('/api/reset', { method: 'POST' }).then(updateData);
    }
    function onGasSliderChange(val) {
      document.getElementById('sliderGasVal').innerText = val + '%';
      fetch('/api/set_gas?val=' + val, { method: 'POST' }).then(updateData);
    }

    setInterval(updateData, 1000);
    window.onload = updateData;
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", HTML_DASHBOARD);
}

void handleStatus() {
  String json = "{\"group\":\"" + String(GROUP_NAME) + "\","
    + "\"batch_code\":\"" + currentBatchCode + "\","
    + "\"elapsed_s\":" + String(elapsedSeconds) + ","
    + "\"temperature_c\":" + String(currentTemp, 1) + ","
    + "\"base_temp\":" + String(baseTemp, 1) + ","
    + "\"gas_percent\":" + String(gasPercent, 1) + ","
    + "\"air_percent\":" + String(airPercent, 1) + ","
    + "\"servo_angle\":" + String(servoAngle) + ","
    + "\"env_temp\":" + String(envTemp, 1) + ","
    + "\"env_humidity\":" + String(envHumidity, 1) + ","
    + "\"phase\":\"" + roastingPhase + "\","
    + "\"is_auto\":" + (isAutoMode ? "true" : "false") + ","
    + "\"is_alert\":" + (isAlert ? "true" : "false") + ","
    + "\"is_warning\":" + (isWarning ? "true" : "false") + ","
    + "\"emergency_dump\":" + (emergencyDumpActive ? "true" : "false") + ","
    + "\"last_api_status\":\"" + lastApiStatus + "\"}";

  server.send(200, "application/json", json);
}

void handleForceOverheat() {
  manualOverride = true;
  manualInterference = true;
  isAutoMode = false;
  overrideGas = 95.0;
  overrideAir = 15.0;
  emergencyDumpActive = false;
  updatePhysicsAndControls();
  server.send(200, "text/plain", "OVERHEAT_FORCED");
}

void handleForceAuto() {
  manualOverride = false;
  manualInterference = false;
  isAutoMode = true;
  emergencyDumpActive = false;
  updatePhysicsAndControls();
  server.send(200, "text/plain", "AUTO_RESTORED");
}

void handleSetGas() {
  if (server.hasArg("val")) {
    float val = server.arg("val").toFloat();
    overrideGas = val;
    manualOverride = true;
    if (val >= 60.0) {
      manualInterference = true;
      isAutoMode = false;
    } else {
      manualInterference = false;
      isAutoMode = true;
    }
    updatePhysicsAndControls();
  }
  server.send(200, "text/plain", "OK");
}

void handleDumpToggle() {
  emergencyDumpActive = !emergencyDumpActive;
  if (emergencyDumpActive) {
    manualOverride = false;
    manualInterference = false;
  }
  updatePhysicsAndControls();
  server.send(200, "text/plain", emergencyDumpActive ? "DUMP_ON" : "DUMP_OFF");
}

void handleReset() {
  elapsedSeconds = 0;
  isAutoMode = true;
  manualInterference = false;
  manualOverride = false;
  emergencyDumpActive = false;
  updatePhysicsAndControls();
  server.send(200, "text/plain", "OK");
}

// ==============================================================================
// 8. SETUP & MAIN LOOP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n==================================================================");
  Serial.println("  12_DH-THMT_Nhom14 - TRAM DIEU KHIEN LO RANG CA PHE ESP32 FULL RIG");
  Serial.println("==================================================================");

  // Cấu hình GPIO
  pinMode(PIN_BTN_DUMP, INPUT_PULLUP);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Khởi động Servo van gió
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  exhaustServo.setPeriodHertz(50);
  exhaustServo.attach(PIN_SERVO, 500, 2400);
  exhaustServo.write(servoAngle);

  // Khởi động DHT22
  dht.begin();

  // Khởi động I2C LCD 20x4
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("12_DH-THMT_Nhom14   ");
  lcd.setCursor(0, 1);
  lcd.print("Industrial Roaster  ");
  lcd.setCursor(0, 2);
  lcd.print("Full Rig ESP32 Wokwi");
  lcd.setCursor(0, 3);
  lcd.print("Auto Mode: READY    ");

  // Kết nối WiFi Wokwi
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(250);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
  }

  // Cấu hình Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/dump", HTTP_POST, handleDumpToggle);
  server.on("/api/force_overheat", HTTP_POST, handleForceOverheat);
  server.on("/api/force_auto", HTTP_POST, handleForceAuto);
  server.on("/api/set_gas", HTTP_POST, handleSetGas);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.begin();
  Serial.println("Web Server hoat dong tren cong 80 (Map -> localhost:8088)");

  // Cập nhật tính toán ban đầu
  updatePhysicsAndControls();
  updateLCD();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Chu kỳ cập nhật tính toán và LCD (mỗi 1 giây)
  if (currentMillis - lastTickTime >= 1000) {
    lastTickTime = currentMillis;
    elapsedSeconds++;

    updatePhysicsAndControls();
    updateLCD();
  }

  // Chu kỳ gửi dữ liệu về Django REST API (mỗi 3 giây theo đặc tả)
  if (currentMillis - lastApiSendTime >= API_INTERVAL) {
    sendReadingToBackend();
    lastApiSendTime = millis();
  }
}
