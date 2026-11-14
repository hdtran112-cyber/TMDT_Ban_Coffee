#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ArduinoJson.h>

#include "secrets.h"

// ==============================================================================
// 1. CẤU HÌNH THÔNG TIN HỆ THỐNG
// ==============================================================================
const char* GROUP_NAME  = "12_DH-THMT_Nhom14";

// Cấu hình mạng WiFi (Wokwi ảo)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// Cấu hình Django Backend API
String djangoApiUrl = API_URL;
String deviceKey = DEVICE_KEY;
String currentBatchCode = DEFAULT_BATCH_CODE;

// Cấu hình ngưỡng nhiệt độ cảnh báo
float targetMaxTemp = 205.0;

// ==============================================================================
// 2. ĐỊNH NGHĨA CHÂN PHẦN CỨNG (PIN CONFIGURATION)
// ==============================================================================
#define PIN_POT_TEMP     34  // Biến trở giả lập nhiệt độ lồng rang (ADC1)
#define PIN_POT_FAN      35  // Biến trở điều chỉnh tốc độ quạt (ADC1)
#define PIN_DHT          15  // Cảm biến môi trường DHT22
#define DHTTYPE          DHT22

#define PIN_BTN          13  // Nút nhấn vật lý Start / Stop mẻ rang

#define PIN_RELAY_HEATER 26  // Relay 1: Điện trở nhiệt lồng rang (Heater - Đỏ)
#define PIN_RELAY_FAN    27  // Relay 2: Quạt làm nguội cà phê (Cooling Fan - Xanh lá)
#define PIN_RELAY_DRUM   14  // Relay 3: Động cơ quay lồng (Drum Motor - Xanh dương)
#define PIN_RELAY_EXHAUST 12 // Relay 4: Quạt hút khói / Cảnh báo (Exhaust Fan - Vàng)

// Khởi tạo thiết bị ngoại vi
LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(PIN_DHT, DHTTYPE);
WebServer server(80); // Khớp với net.forward: localhost:8180 -> target:80

// ==============================================================================
// 3. TRẠNG THÁI HỆ THỐNG & ĐƯỜNG CONG RANG
// ==============================================================================
bool isRoasting = true;
String currentMode = "AUTO"; // "AUTO" (Đường cong mẫu) hoặc "MANUAL" (Núm vặn biến trở)
int elapsedSeconds = 0;
float currentTemp = 180.0;
bool isAlert = false;
float ambientTemp = 28.0;
float ambientHum = 65.0;

// Trạng thái các Relay
bool relayHeater = true;
bool relayFan = false;
bool relayDrum = true;
bool relayExhaust = false;

// Trạng thái kết nối API Django gần nhất
int lastApiCode = 0;
String lastApiResponse = "Chưa gửi";

unsigned long lastTickTime = 0;
unsigned long lastApiSendTime = 0;
const unsigned long API_SEND_INTERVAL = 3000; // Gửi API mỗi 3 giây

// Bảng đường cong nhiệt độ rang chuẩn (S-Curve Profile: 12 mốc thời gian)
struct RoastPoint {
  int second;
  float temp;
};

const RoastPoint roastProfile[] = {
  {0,   180.0}, // Nạp hạt (Charge Temp)
  {30,  130.0}, // Điểm giảm nhiệt
  {60,  105.0}, // Điểm chuyển hồi (Turning Point)
  {90,  118.0}, // Giai đoạn sấy khô (Drying Phase)
  {120, 135.0}, // Bắt đầu đổi màu vàng (Yellowing)
  {150, 152.0}, // Phản ứng Maillard
  {180, 168.0}, // Tăng tốc độ gia nhiệt
  {210, 180.0}, // Caramel hóa
  {240, 190.0}, // Tiếp cận nổ một
  {270, 198.0}, // Chuẩn bị nổ một (Pre-First Crack)
  {300, 205.0}, // Nổ đợt 1 (First Crack - Chạm ngưỡng)
  {330, 208.5}  // Phát triển (Development - Vượt ngưỡng cảnh báo)
};
const int PROFILE_POINTS_COUNT = sizeof(roastProfile) / sizeof(roastProfile[0]);

// ==============================================================================
// 4. HÀM TÍNH TOÁN NHIỆT ĐỘ & ĐIỀU KHIỂN RELAY
// ==============================================================================
float calculateAutoTemp(int seconds) {
  if (seconds <= roastProfile[0].second) return roastProfile[0].temp;
  int maxSec = roastProfile[PROFILE_POINTS_COUNT - 1].second;
  if (seconds >= maxSec) return roastProfile[PROFILE_POINTS_COUNT - 1].temp;

  // Nội suy tuyến tính giữa 2 điểm
  for (int i = 0; i < PROFILE_POINTS_COUNT - 1; i++) {
    if (seconds >= roastProfile[i].second && seconds <= roastProfile[i+1].second) {
      float t = (float)(seconds - roastProfile[i].second) / (roastProfile[i+1].second - roastProfile[i].second);
      return roastProfile[i].temp + t * (roastProfile[i+1].temp - roastProfile[i].temp);
    }
  }
  return 180.0;
}

void applyRelays() {
  digitalWrite(PIN_RELAY_HEATER, relayHeater ? HIGH : LOW);
  digitalWrite(PIN_RELAY_FAN,    relayFan ? HIGH : LOW);
  digitalWrite(PIN_RELAY_DRUM,   relayDrum ? HIGH : LOW);
  digitalWrite(PIN_RELAY_EXHAUST,relayExhaust ? HIGH : LOW);
}

void updateSensors() {
  // Đọc cảm biến môi trường DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h)) ambientHum = h;
  if (!isnan(t)) ambientTemp = t;

  // Tính nhiệt độ lồng rang tùy theo chế độ
  if (currentMode == "MANUAL") {
    int rawPot = analogRead(PIN_POT_TEMP);
    // Ánh xạ ADC (0-4095) sang khoảng nhiệt độ rang (100.0 - 240.0 °C)
    currentTemp = 100.0 + ((float)rawPot / 4095.0) * 140.0;
  } else {
    if (isRoasting) {
      currentTemp = calculateAutoTemp(elapsedSeconds);
    }
  }

  // Tự động kiểm tra cảnh báo quá nhiệt
  isAlert = (currentTemp > targetMaxTemp);

  // Tự động bật relay cảnh báo / quạt hút khói nếu vượt ngưỡng
  if (isAlert) {
    relayExhaust = true;
  }
  applyRelays();
}

// Cập nhật hiển thị lên màn hình LCD 20x4
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("12_DH-THMT_Nhom14   ");

  lcd.setCursor(0, 1);
  lcd.print("HE THONG LO RANG    ");

  lcd.setCursor(0, 2);
  lcd.print("Me: ");
  lcd.print(currentBatchCode);
  for (int i = 4 + currentBatchCode.length(); i < 20; i++) lcd.print(" ");

  lcd.setCursor(0, 3);
  if (isAlert) {
    lcd.print("!QUA NHIET: ");
  } else {
    lcd.print("T: ");
  }
  lcd.print(currentTemp, 1);
  lcd.print((char)223);
  lcd.print("C ");
  lcd.print(isRoasting ? "[R]" : "[D]");
}

// ==============================================================================
// 5. GỬI DỮ LIỆU LÊN DJANGO BACKEND API
// ==============================================================================
void sendReadingToDjango() {
  if (WiFi.status() != WL_CONNECTED) {
    lastApiResponse = "Mất kết nối WiFi";
    return;
  }

  HTTPClient http;
  http.begin(djangoApiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  // Tạo JSON payload
  StaticJsonDocument<256> doc;
  doc["batch_code"] = currentBatchCode;
  doc["elapsed_s"] = elapsedSeconds;
  doc["temperature_c"] = serialized(String(currentTemp, 1));

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);
  lastApiCode = httpResponseCode;

  if (httpResponseCode > 0) {
    lastApiResponse = String(httpResponseCode) + " " + (httpResponseCode == 201 ? "Created" : "Response");
  } else {
    lastApiResponse = "Lỗi kết nối: " + http.errorToString(httpResponseCode);
  }

  // Xuất log ra Serial Monitor theo định dạng chuẩn
  Serial.print("Data: {\"batch\":\"");
  Serial.print(currentBatchCode);
  Serial.print("\", \"elapsed_s\":");
  Serial.print(elapsedSeconds);
  Serial.print(", \"temp\":");
  Serial.print(currentTemp, 1);
  Serial.print(", \"alert\":");
  Serial.print(isAlert ? 1 : 0);
  Serial.print(", \"http\":");
  Serial.print(httpResponseCode);
  Serial.println("}");

  http.end();
}

// ==============================================================================
// 6. GIAO DIỆN WEB LOCALHOST (CỔNG 8180 QUA WOKWI NET.FORWARD)
// ==============================================================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>☕ Giám Sát Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14 | Localhost</title>
  <style>
    :root {
      --primary: #6F4E37;
      --primary-dark: #4A2C11;
      --accent: #D4A373;
      --bg: #F8F9FA;
      --card-bg: #FFFFFF;
      --text: #212529;
      --text-muted: #6C757D;
      --success: #28A745;
      --danger: #DC3545;
      --warning: #FFC107;
      --info: #17A2B8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; min-height: 100vh; }
    .container { max-width: 860px; margin: 0 auto; }
    
    /* Header Banner */
    .header-card {
      background: linear-gradient(135deg, var(--primary-dark), var(--primary));
      color: white;
      padding: 24px 28px;
      border-radius: 16px;
      margin-bottom: 24px;
      box-shadow: 0 8px 24px rgba(74, 44, 17, 0.2);
    }
    .header-card h1 { font-size: 24px; margin-bottom: 8px; display: flex; align-items: center; gap: 10px; }
    .header-card p { font-size: 14px; opacity: 0.9; }
    .badge-class { display: inline-block; background: rgba(255,255,255,0.2); padding: 3px 10px; border-radius: 12px; font-size: 12px; margin-top: 6px; }

    /* Grid Layout */
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 24px; }
    @media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }

    /* Cards */
    .card {
      background: var(--card-bg);
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 4px 15px rgba(0,0,0,0.05);
      border: 1px solid #E9ECEF;
    }
    .card-title {
      font-size: 17px;
      font-weight: 700;
      color: var(--primary-dark);
      margin-bottom: 18px;
      padding-bottom: 10px;
      border-bottom: 2px solid #F1F3F5;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }

    /* Metric items */
    .metric-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px dashed #E9ECEF; font-size: 15px; }
    .metric-row:last-child { border-bottom: none; }
    .metric-label { color: var(--text-muted); font-weight: 500; display: flex; align-items: center; gap: 8px; }
    .metric-val { font-weight: 700; font-size: 16px; }
    .temp-big { font-size: 28px; color: var(--primary); }
    .alert-badge { background: #FEE2E2; color: var(--danger); padding: 4px 10px; border-radius: 20px; font-size: 12px; font-weight: 700; animation: pulse 1.5s infinite; }
    .normal-badge { background: #D1E7DD; color: var(--success); padding: 4px 10px; border-radius: 20px; font-size: 12px; font-weight: 700; }
    
    @keyframes pulse {
      0% { opacity: 0.6; }
      50% { opacity: 1; }
      100% { opacity: 0.6; }
    }

    /* Relays Section */
    .relay-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 14px;
      margin-bottom: 10px;
      background: #F8F9FA;
      border-radius: 10px;
      border-left: 4px solid #CBD5E1;
    }
    .relay-item.active { border-left-color: var(--success); background: #F0FDF4; }
    .relay-info h4 { font-size: 14px; margin-bottom: 2px; }
    .relay-info span { font-size: 12px; color: var(--text-muted); font-weight: 600; }
    .btn-toggle {
      padding: 8px 16px;
      border: none;
      border-radius: 8px;
      font-size: 13px;
      font-weight: 700;
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn-on { background: var(--success); color: white; }
    .btn-on:hover { background: #218838; }
    .btn-off { background: var(--danger); color: white; }
    .btn-off:hover { background: #C82333; }

    /* Controls & Actions */
    .action-panel { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 15px; }
    .btn-action {
      flex: 1;
      padding: 12px 16px;
      border-radius: 10px;
      font-weight: 700;
      font-size: 14px;
      cursor: pointer;
      border: none;
      transition: 0.2s;
      text-align: center;
    }
    .btn-primary { background: var(--primary); color: white; }
    .btn-primary:hover { background: var(--primary-dark); }
    .btn-secondary { background: #E9ECEF; color: var(--text); }
    .btn-secondary:hover { background: #DEE2E6; }

    /* Input controls */
    .form-group { margin-top: 15px; }
    .form-group label { display: block; font-size: 13px; color: var(--text-muted); font-weight: 600; margin-bottom: 6px; }
    .input-row { display: flex; gap: 8px; }
    .input-row input { flex: 1; padding: 9px 12px; border: 1px solid #CED4DA; border-radius: 8px; font-size: 14px; }
    .btn-save { padding: 9px 16px; background: var(--accent); color: var(--primary-dark); font-weight: 700; border: none; border-radius: 8px; cursor: pointer; }

    .footer { text-align: center; color: var(--text-muted); font-size: 12px; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header Thông tin nhóm -->
    <div class="header-card">
      <h1>☕ Hệ Thống Giám Sát Lò Rang Cà Phê IoT - 12_ĐH-THMT_Nhom14</h1>
      <p><strong>Nhóm thực hiện:</strong> 12_ĐH-THMT_Nhom14 &nbsp;|&nbsp; <strong>Dự án:</strong> TMDT_Ban_Coffee</p>
      <div class="badge-class">Module C: Giám sát & Truy xuất mẻ rang IoT</div>
    </div>

    <div class="grid">
      <!-- Card Giám sát -->
      <div class="card">
        <div class="card-title">
          <span>📊 Giám sát Lò Rang (Monitoring)</span>
          <span id="alertBadge" class="normal-badge">AN TOÀN</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">🌡️ Nhiệt độ lồng rang:</span>
          <span class="metric-val temp-big" id="tempVal">-- °C</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">⏱️ Thời gian mẻ rang:</span>
          <span class="metric-val" id="timeVal">00:00 (0s)</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">🏷️ Mã mẻ hiện tại:</span>
          <span class="metric-val" id="batchVal">--</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">🎯 Ngưỡng cảnh báo:</span>
          <span class="metric-val" id="targetTemp">205.0 °C</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">🍃 Môi trường phòng (DHT22):</span>
          <span class="metric-val" id="ambientVal">--°C / --%</span>
        </div>

        <div class="metric-row">
          <span class="metric-label">🌐 Trạng thái Django API:</span>
          <span class="metric-val" id="apiStatus" style="font-size:13px; color:var(--info);">Đang kiểm tra...</span>
        </div>
      </div>

      <!-- Card Điều khiển Relay -->
      <div class="card">
        <div class="card-title">
          <span>⚙️ Điều khiển Cơ cấu Relay</span>
          <span style="font-size:12px; color:var(--text-muted);" id="roastStatus">ĐANG RANG</span>
        </div>

        <!-- Relay 1: Heater -->
        <div class="relay-item" id="itemHeater">
          <div class="relay-info">
            <h4>🔴 Điện trở nhiệt (Heater)</h4>
            <span id="txtHeater">ĐANG BẬT</span>
          </div>
          <button class="btn-toggle btn-off" onclick="toggleRelay('heater')">Tắt</button>
        </div>

        <!-- Relay 2: Cooling Fan -->
        <div class="relay-item" id="itemFan">
          <div class="relay-info">
            <h4>🟢 Quạt làm nguội (Cooling Fan)</h4>
            <span id="txtFan">ĐANG TẮT</span>
          </div>
          <button class="btn-toggle btn-on" onclick="toggleRelay('fan')">Bật</button>
        </div>

        <!-- Relay 3: Drum Motor -->
        <div class="relay-item" id="itemDrum">
          <div class="relay-info">
            <h4>🔵 Mô-tơ quay lồng (Drum Motor)</h4>
            <span id="txtDrum">ĐANG BẬT</span>
          </div>
          <button class="btn-toggle btn-off" onclick="toggleRelay('drum')">Tắt</button>
        </div>

        <!-- Relay 4: Exhaust Fan -->
        <div class="relay-item" id="itemExhaust">
          <div class="relay-info">
            <h4>🟡 Quạt hút khói / Cảnh báo</h4>
            <span id="txtExhaust">ĐANG TẮT</span>
          </div>
          <button class="btn-toggle btn-on" onclick="toggleRelay('exhaust')">Bật</button>
        </div>
      </div>
    </div>

    <!-- Panel Tùy chỉnh & Chế độ hoạt động -->
    <div class="card">
      <div class="card-title">
        <span>🚀 Cấu hình Mô phỏng & Chế độ Chạy</span>
        <span id="modeBadge" style="font-weight:700; color:var(--primary);">CHẾ ĐỘ: TỰ ĐỘNG (AUTO)</span>
      </div>

      <div class="action-panel">
        <button class="btn-action btn-primary" id="btnToggleRoast" onclick="toggleRoasting()">Tạm dừng mẻ rang</button>
        <button class="btn-action btn-secondary" onclick="toggleMode()">Chuyển Chế độ Auto / Manual</button>
        <button class="btn-action btn-secondary" onclick="resetElapsed()">Đặt lại thời gian (0s)</button>
      </div>

      <div class="form-group">
        <label>Đổi Mã Mẻ Rang (Batch Code):</label>
        <div class="input-row">
          <input type="text" id="inputBatchCode" placeholder="Ví dụ: B20261027-01">
          <button class="btn-save" onclick="updateBatchCode()">Cập nhật</button>
        </div>
      </div>
    </div>

    <div class="footer">
      ESP32 Simulator Wokwi | HTTP Server :80 (Mapped :8180) | Django API Sync: host.wokwi.internal:8000
    </div>
  </div>

  <script>
    function formatTime(totalSeconds) {
      var m = Math.floor(totalSeconds / 60);
      var s = totalSeconds % 60;
      return (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s + ' (' + totalSeconds + 's)';
    }

    function fetchStatus() {
      fetch('/api/status')
        .then(function(res) { return res.json(); })
        .then(function(data) {
          document.getElementById('tempVal').innerText = data.temperature_c.toFixed(1) + ' °C';
          document.getElementById('timeVal').innerText = formatTime(data.elapsed_s);
          document.getElementById('batchVal').innerText = data.batch_code;
          document.getElementById('targetTemp').innerText = data.target_max_temp_c.toFixed(1) + ' °C';
          document.getElementById('ambientVal').innerText = data.ambient_temp.toFixed(1) + '°C / ' + data.ambient_hum.toFixed(1) + '%';
          document.getElementById('apiStatus').innerText = data.last_api_status;

          // Alert badge
          var alertBadge = document.getElementById('alertBadge');
          if (data.is_alert) {
            alertBadge.className = 'alert-badge';
            alertBadge.innerText = '⚠️ QUÁ NHIỆT!';
          } else {
            alertBadge.className = 'normal-badge';
            alertBadge.innerText = 'AN TOÀN';
          }

          // Roasting status
          document.getElementById('roastStatus').innerText = data.is_roasting ? 'ĐANG RANG' : 'ĐÃ DỪNG';
          document.getElementById('btnToggleRoast').innerText = data.is_roasting ? 'Tạm dừng mẻ rang' : 'Bắt đầu mẻ rang';
          document.getElementById('modeBadge').innerText = 'CHẾ ĐỘ: ' + (data.mode === 'AUTO' ? 'TỰ ĐỘNG (AUTO)' : 'THỦ CÔNG (MANUAL)');

          // Update relays UI
          updateRelayItem('Heater', data.relays.heater, 'heater');
          updateRelayItem('Fan', data.relays.cooling_fan, 'fan');
          updateRelayItem('Drum', data.relays.drum_motor, 'drum');
          updateRelayItem('Exhaust', data.relays.exhaust_fan, 'exhaust');
        })
        .catch(function(err) {
          console.error("Lỗi cập nhật dữ liệu:", err);
        });
    }

    function updateRelayItem(name, state, key) {
      var item = document.getElementById('item' + name);
      var txt = document.getElementById('txt' + name);
      var btn = item.querySelector('button');

      if (state) {
        item.classList.add('active');
        txt.innerText = 'ĐANG BẬT';
        btn.className = 'btn-toggle btn-off';
        btn.innerText = 'Tắt';
      } else {
        item.classList.remove('active');
        txt.innerText = 'ĐANG TẮT';
        btn.className = 'btn-toggle btn-on';
        btn.innerText = 'Bật';
      }
    }

    function toggleRelay(key) {
      fetch('/api/relay?relay=' + key, { method: 'POST' })
        .then(function() { fetchStatus(); });
    }

    function toggleRoasting() {
      fetch('/api/config?action=toggle_roast', { method: 'POST' })
        .then(function() { fetchStatus(); });
    }

    function toggleMode() {
      fetch('/api/config?action=toggle_mode', { method: 'POST' })
        .then(function() { fetchStatus(); });
    }

    function resetElapsed() {
      fetch('/api/config?action=reset_elapsed', { method: 'POST' })
        .then(function() { fetchStatus(); });
    }

    function updateBatchCode() {
      var val = document.getElementById('inputBatchCode').value.trim();
      if (!val) return;
      fetch('/api/config?batch_code=' + encodeURIComponent(val), { method: 'POST' })
        .then(function() {
          alert('Đã cập nhật mã mẻ rang thành: ' + val);
          fetchStatus();
        });
    }

    // Tự động cập nhật mỗi 1 giây
    setInterval(fetchStatus, 1000);
    window.onload = fetchStatus;
  </script>
</body>
</html>
)rawliteral";

// ==============================================================================
// 7. CÁC ROUTE CỦA WEB SERVER
// ==============================================================================
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["group"] = String(GROUP_NAME);
  doc["batch_code"] = currentBatchCode;
  doc["temperature_c"] = currentTemp;
  doc["elapsed_s"] = elapsedSeconds;
  doc["target_max_temp_c"] = targetMaxTemp;
  doc["is_alert"] = isAlert;
  doc["is_roasting"] = isRoasting;
  doc["mode"] = currentMode;
  doc["ambient_temp"] = ambientTemp;
  doc["ambient_hum"] = ambientHum;
  doc["last_api_status"] = lastApiResponse;

  JsonObject relays = doc.createNestedObject("relays");
  relays["heater"] = relayHeater;
  relays["cooling_fan"] = relayFan;
  relays["drum_motor"] = relayDrum;
  relays["exhaust_fan"] = relayExhaust;

  String jsonStr;
  serializeJson(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

void handleRelay() {
  if (server.hasArg("relay")) {
    String r = server.arg("relay");
    if (r == "heater") relayHeater = !relayHeater;
    else if (r == "fan") relayFan = !relayFan;
    else if (r == "drum") relayDrum = !relayDrum;
    else if (r == "exhaust") relayExhaust = !relayExhaust;
    applyRelays();
  }
  server.send(200, "text/plain", "OK");
}

void handleConfig() {
  if (server.hasArg("action")) {
    String act = server.arg("action");
    if (act == "toggle_roast") {
      isRoasting = !isRoasting;
    } else if (act == "toggle_mode") {
      currentMode = (currentMode == "AUTO") ? "MANUAL" : "AUTO";
    } else if (act == "reset_elapsed") {
      elapsedSeconds = 0;
    }
  }
  if (server.hasArg("batch_code")) {
    currentBatchCode = server.arg("batch_code");
  }
  server.send(200, "text/plain", "OK");
}

// ==============================================================================
// 8. SETUP & LOOP CHÍNH
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================================");
  Serial.println("  12_DH-THMT_Nhom14 | TMDT_BAN_COFFEE - LO RANG IOT ESP32");
  Serial.println("========================================================");

  // Cấu hình chân GPIO
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_RELAY_HEATER, OUTPUT);
  pinMode(PIN_RELAY_FAN, OUTPUT);
  pinMode(PIN_RELAY_DRUM, OUTPUT);
  pinMode(PIN_RELAY_EXHAUST, OUTPUT);
  applyRelays();

  // Khởi tạo LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("12_DH-THMT_Nhom14   ");
  lcd.setCursor(0, 1);
  lcd.print("COFFEE ROASTER IOT  ");
  lcd.setCursor(0, 2);
  lcd.print("Dang ket noi WiFi...");

  // Khởi tạo DHT
  dht.begin();

  // Kết nối WiFi Wokwi ảo
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(300);
    Serial.print(".");
    retry++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected! IP: ");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 3);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP());
  } else {
    Serial.println("WiFi Timeout - Running offline mode");
  }

  // Cấu hình WebServer
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_POST, handleRelay);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.begin();
  Serial.println("Web Server khoi tao tren cong 80 (Forwarded toi localhost:8180)");

  delay(1000);
  lcd.clear();
}

void loop() {
  server.handleClient();

  // Xử lý nút bấm vật lý (Start / Stop)
  if (digitalRead(PIN_BTN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(PIN_BTN) == LOW) {
      isRoasting = !isRoasting;
      Serial.print("[NUT BAM] Doi trang thai rang: ");
      Serial.println(isRoasting ? "ROASTING" : "STOPPED");
      while (digitalRead(PIN_BTN) == LOW) delay(10);
    }
  }

  unsigned long currentMillis = millis();

  // Chu kỳ mỗi 1 giây: Cập nhật đồng hồ và cảm biến
  if (currentMillis - lastTickTime >= 1000) {
    lastTickTime = currentMillis;
    if (isRoasting) {
      elapsedSeconds++;
    }
    updateSensors();
    updateLCD();
  }

  // Chu kỳ gửi dữ liệu lên Django Backend API (mỗi 3 giây)
  if (isRoasting && (currentMillis - lastApiSendTime >= API_SEND_INTERVAL)) {
    lastApiSendTime = currentMillis;
    sendReadingToDjango();
  }
}
