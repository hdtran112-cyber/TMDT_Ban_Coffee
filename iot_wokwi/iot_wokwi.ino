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

// Ngưỡng nhiệt độ cảnh báo theo tiêu chuẩn đặc tả
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
#define PIN_LED_GREEN    12  // LED Xanh: Normal / Roasting (< 195°C)
#define PIN_LED_YELLOW   14  // LED Vàng: Cảnh báo First Crack (195°C - 204.9°C)
#define PIN_LED_RED      27  // LED Đỏ: Báo động quá nhiệt (>= 205°C)
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
float currentTemp        = 125.0;
float baseTemp           = 125.0;

float gasPercent         = 0.0;
float airPercent         = 25.0;
int servoAngle           = 45;

float envTemp            = 28.0;
float envHumidity        = 60.0;

bool isAutoMode          = true;  // Khởi động chạy TỰ ĐỘNG bình thường
bool manualInterference  = false; // Phát hiện can thiệp vặn núm Gas
bool isAlert             = false; // Quá nhiệt nguy hiểm (>= 205°C) -> Đèn Đỏ + Còi
bool isWarning           = false; // Cảnh báo nhạy cảm (195°C - 204.9°C) -> Đèn Vàng
bool emergencyDumpActive = false; // Xả nhiệt khẩn cấp
bool manualOverride      = false; // Lệnh test can thiệp từ nút bấm Web
float overrideGas        = 0.0;
float overrideAir        = 25.0;
bool lastButtonState     = HIGH;
String roastingPhase     = "DRYING";

int lastHttpCode         = 0;
String lastApiStatus     = "Chưa gửi";

unsigned long lastTickTime    = 0;
unsigned long lastApiSendTime = 0;
const unsigned long API_INTERVAL = 3000; // Gửi dữ liệu mỗi 3 giây

// Đường cong rang tự động chuẩn (S-Curve Profile 112°C -> 188°C an toàn ở giai đoạn đầu)
struct AutoPoint {
  int second;
  float temp;
  float air;
};

const AutoPoint autoProfile[] = {
  {0,   125.0, 20.0}, // Nạp hạt
  {30,  110.0, 22.0}, // Turning Point
  {60,  128.0, 25.0}, // Sấy hạt (Drying)
  {120, 150.0, 30.0}, // Vàng hóa (Yellowing)
  {180, 168.0, 38.0}, // Phản ứng Maillard
  {240, 182.0, 45.0}, // Tiếp cận First Crack (<195°C -> Đèn Xanh)
  {300, 198.0, 52.0}, // First Crack (198°C -> Đèn Vàng)
  {360, 202.0, 60.0}  // Phát triển (202°C -> Đèn Vàng, an toàn < 205°C)
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
  // 1. Đọc nút bấm xả mẻ khẩn cấp trên mạch (Active LOW với INPUT_PULLUP)
  bool btnState = digitalRead(PIN_BTN_DUMP);
  if (btnState == LOW && lastButtonState == HIGH) {
    emergencyDumpActive = !emergencyDumpActive;
    if (emergencyDumpActive) {
      manualOverride = false;
      manualInterference = false;
    }
    delay(50);
  }
  lastButtonState = btnState;

  // 2. Đọc chiết áp Lửa (Gas) và Gió (Airflow) từ Wokwi
  int rawGas = analogRead(PIN_POT_GAS);
  int rawAir = analogRead(PIN_POT_AIR);
  float measuredGas = ((float)rawGas / 4095.0) * 100.0;
  float measuredAir = ((float)rawAir / 4095.0) * 100.0;

  // Nếu người dùng xoay biến trở Gas trên Wokwi, ưu tiên giá trị thực từ biến trở
  static int prevGasRaw = -1;
  if (prevGasRaw >= 0 && abs(rawGas - prevGasRaw) > 50) {
    manualOverride = false; // Hủy override từ web khi người dùng vặn núm
  }
  prevGasRaw = rawGas;

  if (manualOverride) {
    gasPercent = overrideGas;
    airPercent = overrideAir;
  } else {
    gasPercent = measuredGas;
    airPercent = measuredAir;
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

  // 5. CÔNG THỨC NHIỆT ĐỘ CHUẨN ĐÚNG THEO YÊU CẦU:
  // - Vặn bên trái hết cỡ (gasPercent < 30%): CHẠY TỰ ĐỘNG BÌNH THƯỜNG -> ĐÈN XANH (<195°C)
  // - Kéo chỉnh nhiệt thêm tí (gasPercent 30% - 65%): CẢNH BÁO -> ĐÈN VÀNG (196°C - 204°C)
  // - Kéo quá tay (gasPercent > 65%): QUÁ NHIỆT NGUY HIỂM -> ĐÈN ĐỎ + CÒI HÚ (>=205°C)
  baseTemp = getAutoBaseTemp(elapsedSeconds);

  if (emergencyDumpActive) {
    gasPercent = 0.0;
    airPercent = 100.0;
    manualInterference = false;
    isAutoMode = false;
    currentTemp -= 3.0; // Làm nguội khẩn cấp
    if (currentTemp < 85.0) currentTemp = 85.0;
  }
  else if (gasPercent > 65.0) {
    // 🔴 KÉO QUÁ TAY: VƯỢT NGƯỠNG 205°C -> BÁO ĐỘNG ĐỎ
    isAutoMode = false;
    manualInterference = true;
    // Map 65% -> 100% tương ứng 207.0°C -> 228.0°C
    currentTemp = 207.0 + ((gasPercent - 65.0) / 35.0) * 21.0;
  }
  else if (gasPercent >= 30.0) {
    // 🟡 KÉO THÊM TÍ: VÙNG CẢNH BÁO FIRST CRACK -> ĐÈN VÀNG (196.0°C - 204.0°C)
    isAutoMode = false;
    manualInterference = true;
    // Map 30% -> 65% tương ứng 196.0°C -> 204.0°C
    currentTemp = 196.0 + ((gasPercent - 30.0) / 35.0) * 8.0;
  }
  else {
    // 🟢 VẶN VỀ TRÁI HẾT CỠ / TỰ ĐỘNG: AN TOÀN BÌNH THƯỜNG -> ĐÈN XANH
    isAutoMode = true;
    manualInterference = false;
    manualOverride = false;
    currentTemp = baseTemp;
  }

  if (currentTemp < 85.0) currentTemp = 85.0;
  if (currentTemp > 240.0) currentTemp = 240.0;

  roastingPhase = determinePhase(currentTemp);

  // 6. XỬ LÝ 3 MỨC ĐÈN VÀ CÒI BÁO ĐỘNG ĐÚNG CHUẨN:
  if (currentTemp >= ALERT_MAX_TEMP) {
    // 🔴 ĐÈN ĐỎ SÁNG RỰC + CÒI HÚ BÁO ĐỘNG (>= 205°C)
    isAlert = true;
    isWarning = false;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_BUZZER, 1200, 250); // Còi kêu tít tít báo động
  }
  else if (currentTemp >= WARN_TEMP) {
    // 🟡 ĐÈN VÀNG SÁNG CẢNH BÁO (195°C - 204.9°C), Tắt còi, Tắt đỏ, Tắt xanh
    isAlert = false;
    isWarning = true;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
  }
  else {
    // 🟢 ĐÈN XANH SÁNG BÌNH THƯỜNG (< 195°C), Tắt còi, Tắt đỏ, Tắt vàng
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
  // Dòng 0: Mã mẻ & Trạng thái: [AUTO], [WARN], [ALRT] hoặc [DUMP]
  lcd.setCursor(0, 0);
  char line0[21];
  const char* stTag = "[AUTO]";
  if (emergencyDumpActive) stTag = "[DUMP]";
  else if (isAlert)       stTag = "[ALRT]";
  else if (isWarning)     stTag = "[WARN]";

  snprintf(line0, sizeof(line0), "B:%-9s %s", currentBatchCode.c_str(), stTag);
  lcd.print(line0);

  // Dòng 1: BT:165.2C|Env:28C 60%
  lcd.setCursor(0, 1);
  char line1[21];
  snprintf(line1, sizeof(line1), "BT:%4.1fC|Env:%2.0fC%2.0f%%", currentTemp, envTemp, envHumidity);
  lcd.print(line1);

  // Dòng 2: Gas: 25%  | Air: 35%
  lcd.setCursor(0, 2);
  char line2[21];
  snprintf(line2, sizeof(line2), "Gas:%3.0f%% | Air:%3.0f%%", gasPercent, airPercent);
  lcd.print(line2);

  // Dòng 3: Phase: DRYING / YELLOWING / FIRST CRACK / DEVELOPMENT
  lcd.setCursor(0, 3);
  char line3[21];
  snprintf(line3, sizeof(line3), "Phase:%-14s", roastingPhase.c_str());
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

  // Payload chuẩn: {"batch_code": "B20261027-02", "elapsed_s": 90, "temperature_c": 198.5}
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
    isAutoMode ? "TỰ ĐỘNG" : "CAN THIỆP NÚM XOAY",
    isAlert ? "QUÁ NHIỆT (ĐỎ)!" : (isWarning ? "CẢNH BÁO (VÀNG)" : "BÌNH THƯỜNG (XANH)"),
    lastApiStatus.c_str());

  http.end();
  server.handleClient();
}

// ==============================================================================
// 7. GIAO DIỆN WEB DASHBOARD LOCALHOST (:8088 QUA WOKWI NET.FORWARD)
// (Chỉ đóng vai trò theo dõi giám sát, đồng bộ 100% với VS Code Wokwi)
// ==============================================================================
const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>☕ Trạm Giám Sát Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14</title>
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
    .led-circle { width: 14px; height: 14px; border-radius: 50%; background: #444; transition: 0.3s; }
    .led-green-on { background: #10b981; box-shadow: 0 0 12px #10b981; }
    .led-yellow-on { background: #f59e0b; box-shadow: 0 0 12px #f59e0b; }
    .led-red-on { background: #ef4444; box-shadow: 0 0 16px #ef4444; animation: pulse 0.8s infinite; }
    
    .btn-action { width: 100%; padding: 12px; border: none; border-radius: 8px; font-weight: 700; cursor: pointer; transition: 0.2s; font-size: 13px; }
    .btn-action:hover { filter: brightness(1.2); transform: translateY(-1px); }
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
          <span id="badgeStatus" class="badge badge-ok">[OK] BÌNH THƯỜNG</span>
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
          <span>Ngưỡng cảnh báo vàng / đỏ:</span>
          <span><strong style="color:var(--warning);">≥195°C</strong> / <strong style="color:var(--danger);">≥205°C</strong></span>
        </div>
      </div>

      <!-- Card 2: Gas & Airflow Dynamics -->
      <div class="card">
        <div class="card-title">
          <span>🎛️ Cân Bằng Nhiệt Lửa & Gió (Từ VS Code)</span>
          <span id="modeBadge" class="badge" style="background:#10b981; color:#fff;">TỰ ĐỘNG</span>
        </div>
        <div style="margin: 12px 0;">
          <div style="display:flex; justify-content:space-between; font-size:14px; font-weight:600;">
            <span>🔥 Núm vặn Lửa Gas (pot_gas):</span>
            <span id="gasVal">0%</span>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar progress-gas" id="gasBar" style="width: 0%;"></div>
          </div>
          <div style="font-size: 12px; color: var(--text-muted); margin-top: 4px;">
            &bull; &lt;30%: Tự Động (Xanh) &bull; 30-65%: Vàng &bull; &gt;65%: Đỏ
          </div>
        </div>
        <div style="margin: 16px 0;">
          <div style="display:flex; justify-content:space-between; font-size:14px; font-weight:600;">
            <span>💨 Núm vặn Gió Air (pot_air):</span>
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

        <!-- Interactive Control Buttons -->
        <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 14px;">
          <button class="btn-action" style="background:#10b981; color:#fff;" onclick="forceAuto()">🤖 Chạy Tự Động Chuẩn</button>
          <button class="btn-action" style="background:#f59e0b; color:#fff;" onclick="testWarn()">🟡 Thử Mức Vàng (198°C)</button>
          <button class="btn-action" style="background:#dc2626; color:#fff;" onclick="testAlert()">🔴 Thử Mức Đỏ (212°C)</button>
          <button class="btn-action" style="background:#3b82f6; color:#fff;" onclick="toggleDump()">💨 Xả Gió Khẩn Cấp</button>
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
            <div class="led-pill"><div class="led-circle" id="ledG"></div> Xanh (&lt;195°C)</div>
            <div class="led-pill"><div class="led-circle" id="ledY"></div> Vàng (195-204°C)</div>
            <div class="led-pill"><div class="led-circle" id="ledR"></div> Đỏ (&ge;205°C)</div>
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
            bText.innerText = '🚨 BÁO ĐỘNG ĐỎ: QUÁ NHIỆT LỒNG RANG (' + d.temperature_c.toFixed(1) + '°C ≥ 205°C)! DO KÉO LỬA QUÁ TAY!';
            bSub.innerText = 'SỰ CỐ!';
            bStatus.className = 'badge badge-alert';
            bStatus.innerText = '🔴 [ALRT] QUÁ NHIỆT!';
            mBadge.innerText = 'QUÁ TAY (ĐỎ)';
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
            bText.innerText = '⚡ CẢNH BÁO VÀNG: NHIỆT ĐỘ TIỆM CẬN FIRST CRACK (' + d.temperature_c.toFixed(1) + '°C: 195°C - 204°C)';
            bSub.innerText = 'CẢNH BÁO';
            bStatus.className = 'badge badge-warn';
            bStatus.innerText = '🟡 [WARN] FIRST CRACK';
            mBadge.innerText = d.is_auto ? 'TỰ ĐỘNG' : 'CHỈNH THÊM TÍ';
            mBadge.style.background = '#f59e0b';
            lG.className = 'led-circle';
            lY.className = 'led-circle led-yellow-on';
            lR.className = 'led-circle';
          } else {
            banner.className = 'status-banner banner-auto';
            bText.innerText = '🤖 HỆ THỐNG ĐANG CHẠY BÌNH THƯỜNG (AN TOÀN < 195°C)';
            bSub.innerText = 'AN TOÀN';
            bStatus.className = 'badge badge-ok';
            bStatus.innerText = '🟢 [OK] BÌNH THƯỜNG';
            mBadge.innerText = 'TỰ ĐỘNG';
            mBadge.style.background = '#10b981';
            lG.className = 'led-circle led-green-on';
            lY.className = 'led-circle';
            lR.className = 'led-circle';
          }
        })
        .catch(function(e) { console.error(e); });
    }

    function forceAuto() {
      fetch('/api/force_auto', { method: 'POST' }).then(updateData);
    }
    function testWarn() {
      fetch('/api/test_warn', { method: 'POST' }).then(updateData);
    }
    function testAlert() {
      fetch('/api/test_alert', { method: 'POST' }).then(updateData);
    }
    function toggleDump() {
      fetch('/api/dump', { method: 'POST' }).then(updateData);
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

void handleForceAuto() {
  manualOverride = false;
  manualInterference = false;
  isAutoMode = true;
  emergencyDumpActive = false;
  overrideGas = 0.0;
  overrideAir = 25.0;
  updatePhysicsAndControls();
  updateLCD();
  server.send(200, "text/plain", "AUTO_RESTORED");
}

void handleTestWarn() {
  manualOverride = true;
  manualInterference = true;
  isAutoMode = false;
  emergencyDumpActive = false;
  overrideGas = 48.0; // Vùng vàng 30-65%
  overrideAir = 30.0;
  updatePhysicsAndControls();
  updateLCD();
  server.send(200, "text/plain", "WARN_TRIGGERED");
}

void handleTestAlert() {
  manualOverride = true;
  manualInterference = true;
  isAutoMode = false;
  emergencyDumpActive = false;
  overrideGas = 85.0; // Vùng đỏ >65%
  overrideAir = 15.0;
  updatePhysicsAndControls();
  updateLCD();
  server.send(200, "text/plain", "ALERT_TRIGGERED");
}

void handleDumpToggle() {
  emergencyDumpActive = !emergencyDumpActive;
  if (emergencyDumpActive) {
    manualOverride = false;
    manualInterference = false;
  }
  updatePhysicsAndControls();
  updateLCD();
  server.send(200, "text/plain", emergencyDumpActive ? "DUMP_ON" : "DUMP_OFF");
}

void handleReset() {
  elapsedSeconds = 0;
  isAutoMode = true;
  manualInterference = false;
  manualOverride = false;
  emergencyDumpActive = false;
  overrideGas = 0.0;
  updatePhysicsAndControls();
  updateLCD();
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

  // Trạng thái ban đầu: Đèn Xanh bật, Vàng tắt, Đỏ tắt, Còi tắt
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
  server.on("/api/force_auto", HTTP_POST, handleForceAuto);
  server.on("/api/test_warn", HTTP_POST, handleTestWarn);
  server.on("/api/test_alert", HTTP_POST, handleTestAlert);
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
