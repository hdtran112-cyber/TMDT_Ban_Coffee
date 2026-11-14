#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==============================================================================
// 1. CẤU HÌNH THÔNG TIN HỆ THỐNG THEO ĐẶC TẢ
// ==============================================================================
const char* GROUP_NAME       = "12_DH-THMT_Nhom14";
const char* DEFAULT_BATCH    = "B20261027-02";

// Ngưỡng nhiệt độ cảnh báo an toàn (°C)
const float TARGET_MAX_TEMP  = 205.0;

// Cấu hình mạng WiFi (Wokwi ảo)
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// Cấu hình Django Backend API (theo đặc tả kỹ thuật: 10.0.2.2 hoặc host.wokwi.internal)
String djangoApiUrl     = "http://10.0.2.2:8000/api/iot/readings/";
String deviceKey        = "my-test-secret-key-123";
String currentBatchCode = DEFAULT_BATCH;

// ==============================================================================
// 2. ĐỊNH NGHĨA CHÂN PHẦN CỨNG (PINOUT THEO ĐẶC TẢ)
// ==============================================================================
#define PIN_POT_FAULT  34  // Potentiometer: Can thiệp ngoại cảnh / lỗi gia nhiệt (+0°C .. +35°C)
#define PIN_LED_ALERT  2   // Đèn LED đỏ: Cảnh báo quá nhiệt
#define I2C_SDA        21  // LCD 1602 SDA
#define I2C_SCL        22  // LCD 1602 SCL

// Khởi tạo màn hình LCD 16x2 (I2C 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo Web Server (Cổng 80 -> Forward localhost:8180)
WebServer server(80);

// ==============================================================================
// 3. ĐƯỜNG CONG RANG TIÊU CHUẨN & BIẾN TRẠNG THÁI
// ==============================================================================
int elapsedSeconds   = 0;
float currentTemp    = 180.0;
float faultOffset    = 0.0;
bool isAlert         = false;
bool isRoasting      = true;

int lastHttpCode     = 0;
String lastApiStatus = "Chưa gửi";

unsigned long lastTickTime    = 0;
unsigned long lastApiSendTime = 0;
const unsigned long API_INTERVAL = 3000; // Gửi dữ liệu mỗi 3 giây

// Bảng đường cong nhiệt độ chuẩn của lò rang (S-Curve Profile)
struct TempPoint {
  int second;
  float temp;
};

const TempPoint standardCurve[] = {
  {0,   180.0}, // Nạp hạt (Charge Temp)
  {30,  130.0}, // Giảm nhiệt
  {60,  105.0}, // Điểm chuyển hồi (Turning Point)
  {90,  118.0}, // Giai đoạn sấy khô (Drying Phase)
  {120, 135.0}, // Bắt đầu đổi màu (Yellowing)
  {150, 152.0}, // Phản ứng Maillard
  {180, 168.0}, // Gia nhiệt tăng tốc
  {210, 180.0}, // Caramel hóa
  {240, 190.0}, // Tiếp cận First Crack
  {270, 198.0}, // Chuẩn bị First Crack
  {300, 205.0}, // Nổ đợt 1 (First Crack - Chạm ngưỡng)
  {330, 208.5}  // Phát triển (Development - Quá nhiệt nếu không can thiệp)
};
const int CURVE_SIZE = sizeof(standardCurve) / sizeof(standardCurve[0]);

float calculateBaseTemp(int sec) {
  if (sec <= standardCurve[0].second) return standardCurve[0].temp;
  int maxSec = standardCurve[CURVE_SIZE - 1].second;
  if (sec >= maxSec) return standardCurve[CURVE_SIZE - 1].temp;

  for (int i = 0; i < CURVE_SIZE - 1; i++) {
    if (sec >= standardCurve[i].second && sec <= standardCurve[i+1].second) {
      float ratio = (float)(sec - standardCurve[i].second) / (standardCurve[i+1].second - standardCurve[i].second);
      return standardCurve[i].temp + ratio * (standardCurve[i+1].temp - standardCurve[i].temp);
    }
  }
  return 180.0;
}

// ==============================================================================
// 4. ĐỌC CẢM BIẾN, TÍNH TOÁN & CẬP NHẬT HIỂN THỊ
// ==============================================================================
void updateTelemetry() {
  float baseTemp = calculateBaseTemp(elapsedSeconds);
  int rawPot = analogRead(PIN_POT_FAULT);
  faultOffset = ((float)rawPot / 4095.0) * 35.0;
  currentTemp = baseTemp + faultOffset;

  isAlert = (currentTemp > TARGET_MAX_TEMP);
  digitalWrite(PIN_LED_ALERT, isAlert ? HIGH : LOW);
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print(currentBatchCode);
  if (isAlert) {
    lcd.print(" [ALERT!]");
  } else {
    lcd.print(" [OK]   ");
  }

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print((char)223);
  lcd.print("C Nhom14 ");
}

// ==============================================================================
// 5. GỬI DỮ LIỆU HTTP POST VỀ DJANGO BACKEND
// ==============================================================================
void sendReadingToBackend() {
  if (WiFi.status() != WL_CONNECTED) {
    lastApiStatus = "Mất kết nối WiFi";
    return;
  }

  HTTPClient http;
  http.begin(djangoApiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  // Tạo payload JSON theo đúng đặc tả:
  // {"batch_code": "B20261027-02", "elapsed_s": 90, "temperature_c": 195.5}
  String payload = "{\"batch_code\":\"" + currentBatchCode + "\",\"elapsed_s\":" + String(elapsedSeconds) + ",\"temperature_c\":" + String(currentTemp, 1) + "}";

  int httpCode = http.POST(payload);
  lastHttpCode = httpCode;

  if (httpCode > 0) {
    lastApiStatus = String(httpCode) + " " + (httpCode == 201 ? "Created" : "Response");
  } else {
    lastApiStatus = "Lỗi HTTP: " + http.errorToString(httpCode);
  }

  Serial.print("[IoT] Giây: ");
  Serial.print(elapsedSeconds);
  Serial.print("s | Nhiet do: ");
  Serial.print(currentTemp, 1);
  Serial.print(" C (Goc: ");
  Serial.print(calculateBaseTemp(elapsedSeconds), 1);
  Serial.print(" + Lech: ");
  Serial.print(faultOffset, 1);
  Serial.print(" C) | Canh bao: ");
  Serial.print(isAlert ? "CO (BAT DEN RED)" : "KHONG");
  Serial.print(" | Django API: ");
  Serial.println(lastApiStatus);

  http.end();
}

// ==============================================================================
// 6. GIAO DIỆN WEB LOCALHOST (:8180 QUA WOKWI NET.FORWARD)
// ==============================================================================
const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>☕ Giám Sát Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14 | Localhost:8180</title>
  <style>
    :root {
      --primary: #6F4E37;
      --primary-dark: #4A2C11;
      --bg: #F4F6F9;
      --card-bg: #FFFFFF;
      --text: #212529;
      --danger: #DC3545;
      --success: #28A745;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; }
    .container { max-width: 800px; margin: 0 auto; }
    .header {
      background: linear-gradient(135deg, var(--primary-dark), var(--primary));
      color: white;
      padding: 22px 26px;
      border-radius: 14px;
      margin-bottom: 20px;
      box-shadow: 0 6px 20px rgba(74,44,17,0.15);
    }
    .header h1 { font-size: 22px; margin-bottom: 6px; }
    .header p { font-size: 14px; opacity: 0.9; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 18px; margin-bottom: 20px; }
    @media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }
    .card {
      background: var(--card-bg);
      border-radius: 12px;
      padding: 20px;
      border: 1px solid #E9ECEF;
      box-shadow: 0 4px 12px rgba(0,0,0,0.04);
    }
    .card-title {
      font-size: 16px;
      font-weight: 700;
      color: var(--primary-dark);
      margin-bottom: 15px;
      padding-bottom: 8px;
      border-bottom: 2px solid #F1F3F5;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .metric { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px dashed #E9ECEF; font-size: 15px; }
    .metric:last-child { border-bottom: none; }
    .metric-label { color: #6C757D; font-weight: 500; }
    .metric-val { font-weight: 700; font-size: 16px; }
    .temp-large { font-size: 32px; color: var(--primary); }
    .badge-ok { background: #D1E7DD; color: var(--success); padding: 4px 12px; border-radius: 16px; font-weight: 700; font-size: 13px; }
    .badge-alert { background: #F8D7DA; color: var(--danger); padding: 4px 12px; border-radius: 16px; font-weight: 700; font-size: 13px; animation: blink 1.2s infinite; }
    @keyframes blink { 0% { opacity: 0.5; } 50% { opacity: 1; } 100% { opacity: 0.5; } }
    .led-indicator { display: inline-block; width: 14px; height: 14px; border-radius: 50%; background: #CCC; margin-right: 6px; }
    .led-on { background: #DC3545; box-shadow: 0 0 10px #DC3545; }
    .btn { width: 100%; margin-top: 15px; padding: 11px; border-radius: 8px; border: none; font-weight: 700; cursor: pointer; background: var(--primary); color: white; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>☕ Giám Sát Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14</h1>
      <p><strong>Dự án:</strong> TMDT_Ban_Coffee &nbsp;|&nbsp; <strong>Nhóm:</strong> 12_ĐH-THMT_Nhom14</p>
    </div>

    <div class="grid">
      <div class="card">
        <div class="card-title">
          <span>📊 Thông Số Lò Rang</span>
          <span id="statusBadge" class="badge-ok">[OK] AN TOÀN</span>
        </div>
        <div class="metric">
          <span class="metric-label">🌡️ Nhiệt độ tức thời:</span>
          <span class="metric-val temp-large" id="tempVal">-- °C</span>
        </div>
        <div class="metric">
          <span class="metric-label">⏱️ Thời gian mẻ:</span>
          <span class="metric-val" id="timeVal">0s</span>
        </div>
        <div class="metric">
          <span class="metric-label">🏷️ Mã mẻ rang:</span>
          <span class="metric-val">B20261027-02</span>
        </div>
        <div class="metric">
          <span class="metric-label">🎯 Ngưỡng cảnh báo:</span>
          <span class="metric-val">205.0 °C</span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">🚨 Cảnh Báo & Can Thiệp Sự Cố</div>
        <div class="metric">
          <span class="metric-label">🔴 Đèn LED Đỏ (GPIO 2):</span>
          <span class="metric-val" id="ledStatus"><span class="led-indicator" id="ledDot"></span> TẮT</span>
        </div>
        <div class="metric">
          <span class="metric-label">🎛️ Độ lệch biến trở:</span>
          <span class="metric-val" id="faultVal">+0.0 °C</span>
        </div>
        <div class="metric">
          <span class="metric-label">🌐 Kết nối Django API:</span>
          <span class="metric-val" id="apiStatus" style="font-size: 13px; color: var(--primary);">Đang gửi...</span>
        </div>
        <button class="btn" onclick="resetTimer()">Đặt lại thời gian (0s)</button>
      </div>
    </div>
  </div>

  <script>
    function fetchState() {
      fetch('/api/status')
        .then(function(res) { return res.json(); })
        .then(function(d) {
          document.getElementById('tempVal').innerText = d.temperature_c.toFixed(1) + ' °C';
          var m = Math.floor(d.elapsed_s / 60);
          var s = d.elapsed_s % 60;
          document.getElementById('timeVal').innerText = (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s + ' (' + d.elapsed_s + 's)';
          document.getElementById('faultVal').innerText = '+' + d.fault_offset.toFixed(1) + ' °C';
          document.getElementById('apiStatus').innerText = d.last_api_status;

          var badge = document.getElementById('statusBadge');
          var dot = document.getElementById('ledDot');
          var ledTxt = document.getElementById('ledStatus');

          if (d.is_alert) {
            badge.className = 'badge-alert';
            badge.innerText = '⚠️ [ALERT!] QUÁ NHIỆT';
            dot.className = 'led-indicator led-on';
            ledTxt.innerHTML = '<span class="led-indicator led-on"></span> BẬT SÁNG';
          } else {
            badge.className = 'badge-ok';
            badge.innerText = '✅ [OK] AN TOÀN';
            dot.className = 'led-indicator';
            ledTxt.innerHTML = '<span class="led-indicator"></span> TẮT';
          }
        })
        .catch(function(e) { console.error(e); });
    }

    function resetTimer() {
      fetch('/api/reset', { method: 'POST' }).then(fetchState);
    }

    setInterval(fetchState, 1000);
    window.onload = fetchState;
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", HTML_DASHBOARD);
}

void handleStatus() {
  String jsonOut = "{\"group\":\"" + String(GROUP_NAME) + "\",\"batch_code\":\"" + currentBatchCode + "\",\"elapsed_s\":" + String(elapsedSeconds) + ",\"temperature_c\":" + String(currentTemp, 1) + ",\"fault_offset\":" + String(faultOffset, 1) + ",\"is_alert\":" + (isAlert ? "true" : "false") + ",\"last_api_status\":\"" + lastApiStatus + "\"}";
  server.send(200, "application/json", jsonOut);
}

void handleReset() {
  elapsedSeconds = 0;
  server.send(200, "text/plain", "OK");
}

// ==============================================================================
// 7. SETUP & MAIN LOOP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n========================================================");
  Serial.println("  12_DH-THMT_Nhom14 - IOT LO RANG CA PHE ESP32 WOKWI");
  Serial.println("========================================================");

  pinMode(PIN_LED_ALERT, OUTPUT);
  digitalWrite(PIN_LED_ALERT, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("12_DH-THMT_Nhom14");
  lcd.setCursor(0, 1);
  lcd.print("Ket noi WiFi...");

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

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.begin();
  Serial.println("Web Server hoat dong tren cong 80 (Map -> localhost:8180)");

  lcd.clear();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  if (currentMillis - lastTickTime >= 1000) {
    lastTickTime = currentMillis;
    if (isRoasting) {
      elapsedSeconds++;
    }
    updateTelemetry();
    updateLCD();
  }

  if (isRoasting && (currentMillis - lastApiSendTime >= API_INTERVAL)) {
    lastApiSendTime = currentMillis;
    sendReadingToBackend();
  }
}
