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

// Cấu hình Django Backend API (10.0.2.2 hoặc host.wokwi.internal)
String djangoApiUrl     = "http://10.0.2.2:8000/api/iot/readings/";
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
// 3. BIẾN TRẠNG THÁI & MÔ HÌNH VẬT LÝ LÒ RANG CÔNG NGHIỆP
// ==============================================================================
int elapsedSeconds       = 0;
float currentTemp        = 110.0;
float baseTemp           = 110.0;
float gasEffect          = 0.0;
float airCooling         = 0.0;

float gasPercent         = 50.0;
float airPercent         = 30.0;
int servoAngle           = 54;

float envTemp            = 28.0;
float envHumidity        = 60.0;

bool isAlert             = false;
bool isWarning           = false;
bool emergencyDumpActive = false;
bool lastButtonState     = HIGH;
String roastingPhase     = "DRYING";

int lastHttpCode         = 0;
String lastApiStatus     = "Chưa gửi";

unsigned long lastTickTime    = 0;
unsigned long lastApiSendTime = 0;
const unsigned long API_INTERVAL = 3000; // Gửi dữ liệu mỗi 3 giây

// Đường cong nhiệt độ nền chuẩn (Base Roasting Curve 110°C -> 195°C)
struct TempPoint {
  int second;
  float temp;
};

const TempPoint standardBaseCurve[] = {
  {0,   110.0},
  {45,  122.0},
  {90,  136.0},
  {150, 154.0},
  {210, 170.0},
  {270, 183.0},
  {330, 191.0},
  {360, 195.0}
};
const int BASE_CURVE_SIZE = sizeof(standardBaseCurve) / sizeof(standardBaseCurve[0]);

float calculateBaseTemp(int sec) {
  if (sec <= standardBaseCurve[0].second) return standardBaseCurve[0].temp;
  int maxSec = standardBaseCurve[BASE_CURVE_SIZE - 1].second;
  if (sec >= maxSec) return standardBaseCurve[BASE_CURVE_SIZE - 1].temp;

  for (int i = 0; i < BASE_CURVE_SIZE - 1; i++) {
    if (sec >= standardBaseCurve[i].second && sec <= standardBaseCurve[i+1].second) {
      float ratio = (float)(sec - standardBaseCurve[i].second) / (standardBaseCurve[i+1].second - standardBaseCurve[i].second);
      return standardBaseCurve[i].temp + ratio * (standardBaseCurve[i+1].temp - standardBaseCurve[i].temp);
    }
  }
  return 195.0;
}

// Xác định 4 pha rang theo đặc tả
String determinePhase(float temp) {
  if (temp < 140.0) return "DRYING";
  if (temp < 175.0) return "YELLOWING";
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
    Serial.println(emergencyDumpActive ? "[NUT NHAN] Kich hoat XA MIE KHAN CAP (Air 100%, Gas 0%)!" : "[NUT NHAN] Tat che do xa me khap cap.");
    delay(50); // Chống dội phím
  }
  lastButtonState = btnState;

  // 2. Đọc chiết áp Lửa (Gas) và Gió (Airflow)
  int rawGas = analogRead(PIN_POT_GAS);
  int rawAir = analogRead(PIN_POT_AIR);

  float measuredGas = ((float)rawGas / 4095.0) * 100.0;
  float measuredAir = ((float)rawAir / 4095.0) * 100.0;

  if (emergencyDumpActive) {
    gasPercent = 0.0;
    airPercent = 100.0;
  } else {
    gasPercent = measuredGas;
    airPercent = measuredAir;
  }

  // 3. Điều khiển Servo góc van gió
  servoAngle = map((int)airPercent, 0, 100, 0, 180);
  exhaustServo.write(servoAngle);

  // 4. Đọc cảm biến DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    envHumidity = h;
    envTemp = t;
  }

  // 5. Tính toán cân bằng nhiệt độ hạt (Bean Temp)
  // currentTemp = baseTemp + gasEffect - airCooling
  baseTemp = calculateBaseTemp(elapsedSeconds);
  gasEffect = (gasPercent / 100.0) * 30.0;      // Lửa cộng thêm tối đa +30°C
  airCooling = (airPercent / 100.0) * 12.0;    // Gió làm mát tối đa -12°C

  currentTemp = baseTemp + gasEffect - airCooling;
  roastingPhase = determinePhase(currentTemp);

  // 6. Xử lý trạng thái cảnh báo & Điều khiển LED / Buzzer
  if (currentTemp > ALERT_MAX_TEMP) {
    // Quá nhiệt nguy hiểm (> 205°C)
    isAlert = true;
    isWarning = false;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_BUZZER, 1200, 250); // Còi kêu cảnh báo
  } else if (currentTemp > WARN_TEMP) {
    // Cảnh báo giai đoạn nhạy cảm / Chuẩn bị First crack (195°C - 205°C)
    isAlert = false;
    isWarning = true;
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
  } else {
    // Bình thường (<= 195°C)
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
  // Dòng 0: B:B20261027-02 [OK] hoặc [ALRT]
  lcd.setCursor(0, 0);
  char line0[21];
  snprintf(line0, sizeof(line0), "B:%-10s %s", currentBatchCode.c_str(), isAlert ? "[ALRT]" : (emergencyDumpActive ? "[DUMP]" : "[OK] "));
  lcd.print(line0);

  // Dòng 1: BT:198.5C | Env:31C
  lcd.setCursor(0, 1);
  char line1[21];
  snprintf(line1, sizeof(line1), "BT:%5.1fC|Env:%2.0fC %2.0f%%", currentTemp, envTemp, envHumidity);
  lcd.print(line1);

  // Dòng 2: Gas: 75%  | Air: 40%
  lcd.setCursor(0, 2);
  char line2[21];
  snprintf(line2, sizeof(line2), "Gas:%3.0f%% | Air:%3.0f%%", gasPercent, airPercent);
  lcd.print(line2);

  // Dòng 3: Phase: FIRST CRACK
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

  HTTPClient http;
  http.begin(djangoApiUrl);
  http.setConnectTimeout(400); // 400ms timeout để không nghẽn Web Server
  http.setTimeout(400);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  // Payload theo hợp đồng: {"batch_code": "B20261027-02", "elapsed_s": 90, "temperature_c": 198.5}
  String payload = "{\"batch_code\":\"" + currentBatchCode + "\",\"elapsed_s\":" + String(elapsedSeconds) + ",\"temperature_c\":" + String(currentTemp, 1) + "}";

  int httpCode = http.POST(payload);
  lastHttpCode = httpCode;

  if (httpCode > 0) {
    lastApiStatus = String(httpCode) + " " + (httpCode == 201 ? "Created" : "OK");
  } else {
    lastApiStatus = "Lỗi HTTP: " + http.errorToString(httpCode);
  }

  Serial.printf("[IoT] Giây: %ds | BT: %.1f°C (Base: %.1f + Gas: %.1f - Air: %.1f) | Gas: %.0f%% | Air: %.0f%% (Servo %d°) | Phase: %s | Trang thai: %s | API: %s\n",
    elapsedSeconds, currentTemp, baseTemp, gasEffect, airCooling, gasPercent, airPercent, servoAngle, roastingPhase.c_str(), isAlert ? "QUÁ NHIỆT!" : (isWarning ? "CẢNH BÁO" : "BÌNH THƯỜNG"), lastApiStatus.c_str());

  http.end();
  server.handleClient();
}

// ==============================================================================
// 7. GIAO DIỆN WEB DASHBOARD LOCALHOST (:8180 QUA WOKWI NET.FORWARD)
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
      --primary-light: #8c5835;
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
    body { background: var(--bg); color: var(--text); padding: 24px; }
    .container { max-width: 960px; margin: 0 auto; }
    .header {
      background: linear-gradient(135deg, #2b1704 0%, #4a2810 50%, #1f2937 100%);
      padding: 24px 28px;
      border-radius: 16px;
      margin-bottom: 24px;
      border: 1px solid #4a3424;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 10px 25px rgba(0,0,0,0.4);
    }
    .header h1 { font-size: 24px; font-weight: 700; color: #fde68a; }
    .header p { font-size: 14px; color: #d1d5db; margin-top: 4px; }
    .badge-group { display: inline-block; background: #374151; color: #60a5fa; padding: 4px 12px; border-radius: 20px; font-weight: 600; font-size: 13px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin-bottom: 24px; }
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
    @keyframes pulse { 0% { opacity: 0.6; } 50% { opacity: 1; } 100% { opacity: 0.6; } }
    .progress-bar-bg { background: #111827; height: 10px; border-radius: 5px; overflow: hidden; margin-top: 6px; }
    .progress-bar { height: 100%; border-radius: 5px; transition: width 0.3s ease; }
    .progress-gas { background: linear-gradient(90deg, #f59e0b, #ef4444); }
    .progress-air { background: linear-gradient(90deg, #38bdf8, #3b82f6); }
    .leds { display: flex; gap: 14px; margin-top: 10px; align-items: center; }
    .led-pill { display: flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 600; padding: 6px 12px; border-radius: 20px; background: #111827; }
    .led-circle { width: 12px; height: 12px; border-radius: 50%; background: #444; }
    .led-green-on { background: #10b981; box-shadow: 0 0 10px #10b981; }
    .led-yellow-on { background: #f59e0b; box-shadow: 0 0 10px #f59e0b; }
    .led-red-on { background: #ef4444; box-shadow: 0 0 12px #ef4444; }
    .btn-action { width: 100%; padding: 12px; border: none; border-radius: 10px; font-weight: 700; cursor: pointer; transition: 0.2s; margin-top: 12px; }
    .btn-dump { background: #dc2626; color: white; }
    .btn-dump:hover { background: #b91c1c; }
    .btn-reset { background: #374151; color: white; margin-top: 8px; }
    .btn-reset:hover { background: #4b5563; }
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

    <div class="grid">
      <!-- Card 1: Bean Temperature & Phase -->
      <div class="card">
        <div class="card-title">
          <span>🌡️ Nhiệt Độ Lồng Rang (Bean Temp)</span>
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
          <span>Ngưỡng quá nhiệt (Alert):</span>
          <span style="color: var(--danger); font-weight: 700;">&gt; 205.0 °C</span>
        </div>
      </div>

      <!-- Card 2: Gas & Airflow Dynamics -->
      <div class="card">
        <div class="card-title">
          <span>🎛️ Cân Bằng Nhiệt Lửa & Gió</span>
          <span id="dumpBadge" class="badge" style="background:#222; color:#888;">AUTO</span>
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
        <button class="btn-action btn-dump" onclick="toggleDump()">🚨 Nhấn Xả Nhiệt Khẩn Cấp (Dump Button)</button>
        <button class="btn-action btn-reset" onclick="resetTimer()">⏱️ Đặt lại thời gian mẻ (0s)</button>
      </div>

      <!-- Card 3: Environment & Backend Sync -->
      <div class="card">
        <div class="card-title">
          <span>🏭 Môi Trường & Kết Nối Backend</span>
        </div>
        <div class="metric-row">
          <span>Nhiệt độ xưởng (DHT22):</span>
          <strong id="envT">-- °C</strong>
        </div>
        <div class="metric-row">
          <span>Độ ẩm xưởng / Ống xả:</span>
          <strong id="envH">-- %</strong>
        </div>
        <div class="metric-row">
          <span>Đồng bộ Django Backend API:</span>
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
          var dBadge = document.getElementById('dumpBadge');
          var lG = document.getElementById('ledG');
          var lY = document.getElementById('ledY');
          var lR = document.getElementById('ledR');

          if (d.emergency_dump) {
            dBadge.innerText = 'DUMP ACTIVE';
            dBadge.style.background = '#dc2626';
            dBadge.style.color = '#fff';
          } else {
            dBadge.innerText = 'AUTO CONTROL';
            dBadge.style.background = '#222';
            dBadge.style.color = '#888';
          }

          if (d.is_alert) {
            bStatus.className = 'badge badge-alert';
            bStatus.innerText = '⚠️ [ALRT] QUÁ NHIỆT!';
            lG.className = 'led-circle';
            lY.className = 'led-circle';
            lR.className = 'led-circle led-red-on';
          } else if (d.is_warning) {
            bStatus.className = 'badge badge-warn';
            bStatus.innerText = '⚡ [WARN] FIRST CRACK';
            lG.className = 'led-circle';
            lY.className = 'led-circle led-yellow-on';
            lR.className = 'led-circle';
          } else {
            bStatus.className = 'badge badge-ok';
            bStatus.innerText = '✅ [OK] AN TOÀN';
            lG.className = 'led-circle led-green-on';
            lY.className = 'led-circle';
            lR.className = 'led-circle';
          }
        })
        .catch(function(e) { console.error(e); });
    }

    function toggleDump() {
      fetch('/api/dump', { method: 'POST' }).then(updateData);
    }

    function resetTimer() {
      fetch('/api/reset', { method: 'POST' }).then(updateData);
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
    + "\"is_alert\":" + (isAlert ? "true" : "false") + ","
    + "\"is_warning\":" + (isWarning ? "true" : "false") + ","
    + "\"emergency_dump\":" + (emergencyDumpActive ? "true" : "false") + ","
    + "\"last_api_status\":\"" + lastApiStatus + "\"}";

  server.send(200, "application/json", json);
}

void handleDumpToggle() {
  emergencyDumpActive = !emergencyDumpActive;
  server.send(200, "text/plain", emergencyDumpActive ? "DUMP_ON" : "DUMP_OFF");
}

void handleReset() {
  elapsedSeconds = 0;
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
  lcd.print("Connecting WiFi...  ");

  // Kết nối WiFi
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
  server.on("/api/reset", HTTP_POST, handleReset);
  server.begin();
  Serial.println("Web Server hoat dong tren cong 80 (Map -> localhost:8180)");

  lcd.clear();
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
