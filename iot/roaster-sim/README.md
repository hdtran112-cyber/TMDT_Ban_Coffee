# Thiết Bị Mô Phỏng Lò Rang Cà Phê IoT ESP32 (PlatformIO & Wokwi)

Dự án: **TMDT_Ban_Coffee** - Module C: Giám Sát & Truy Xuất Mẻ Rang Bằng IoT  
**Nhóm:** IOT - NHÓM 9  
**Sinh viên:** Lương Thế Cường  
**MSSV:** 1250080025  
**Lớp:** 12_ĐH_THMT  

---

## 1. Giới thiệu

Thư mục này chứa mã nguồn và sơ đồ mạch mô phỏng thiết bị giám sát lò rang cà phê chạy vi điều khiển **ESP32** trên nền tảng **PlatformIO** và trình giả lập **Wokwi**.

Hệ thống cung cấp:
- **Mô phỏng phần cứng trên Wokwi:** ESP32 DevKit, màn hình LCD 20x4 I2C, cảm biến nhiệt độ / núm vặn biến trở, cảm biến môi trường DHT22, 4 module Relay điều khiển đèn LED, và nút bấm vật lý Start/Stop.
- **Tích hợp Web Server Localhost (:8180):** ESP32 phục vụ trực tiếp một trang Web Dashboard chuẩn giao diện cà phê tại `http://localhost:8180/` (thông qua tính năng `net.forward` của Wokwi).
- **Đồng bộ dữ liệu thời gian thực với Django Backend:** Gửi điểm đo nhiệt độ mẻ rang định kỳ lên API `http://host.wokwi.internal:8000/api/iot/readings/` có kèm mã định danh bảo mật `X-Device-Key`.

---

## 2. Cấu trúc thư mục

```
iot/roaster-sim/
├── platformio.ini       # Cấu hình PlatformIO (board esp32dev, thư viện LiquidCrystal_I2C, DHT, ArduinoJson)
├── wokwi.toml           # Cấu hình nạp firmware Wokwi và port-forwarding (localhost:8180 -> target:80)
├── diagram.json         # Sơ đồ mạch điện Wokwi (ESP32, LCD 2004, DHT22, Potentiometers, 4 Relays, LEDs)
├── analysis_server.py   # Script Python phân tích mẻ rang & RoR (Rate of Rise)
├── include/
│   ├── secrets.h        # Khóa bảo mật & cấu hình mạng (đã được .gitignore bảo vệ)
│   └── secrets.h.example # File mẫu cấu hình secrets
├── src/
│   └── main.cpp         # Toàn bộ mã nguồn vi điều khiển ESP32 và Web Server tích hợp
└── README.md            # Tài liệu hướng dẫn sử dụng
```

---

## 3. Sơ đồ mạch & Cổng kết nối (Pinout)

| Linh kiện | Chân kết nối ESP32 | Mô tả |
|---|---|---|
| **Màn hình LCD 20x4 (I2C)** | SDA: GPIO 21, SCL: GPIO 22, VCC: 5V, GND | Hiển thị SV: Luong The Cuong, MSSV, Mã mẻ, Nhiệt độ |
| **Biến trở 1 (Temp Knob)** | SIG: GPIO 34 (ADC1), VCC: 3.3V, GND | Giả lập điều chỉnh nhiệt độ lồng rang (100°C - 240°C) |
| **Biến trở 2 (Fan Knob)** | SIG: GPIO 35 (ADC1), VCC: 3.3V, GND | Điều chỉnh tốc độ quạt hoặc luồng gió |
| **Cảm biến DHT22** | SDA: GPIO 15, VCC: 3.3V, GND | Đo nhiệt độ và độ ẩm môi trường phòng rang |
| **Nút bấm (Start/Stop)** | Pin: GPIO 13, GND (INPUT_PULLUP) | Nút cứng chuyển trạng thái Bắt đầu / Dừng rang |
| **Relay 1 + LED Đỏ** | IN: GPIO 26 | Điện trở gia nhiệt lò rang (Heater) |
| **Relay 2 + LED Xanh lá** | IN: GPIO 27 | Quạt làm nguội cà phê (Cooling Fan) |
| **Relay 3 + LED Xanh dương** | IN: GPIO 14 | Động cơ quay lồng rang (Drum Motor) |
| **Relay 4 + LED Vàng** | IN: GPIO 12 | Quạt hút khói / Đèn cảnh báo quá nhiệt (Exhaust/Alert) |

---

## 4. Hướng dẫn chạy mô phỏng trên VS Code (Giống như hình mẫu)

### Bước 1: Mở thư mục dự án
Mở VS Code tại thư mục `iot/roaster-sim` (hoặc mở toàn bộ repo `TMDT_Ban_Coffee`).

### Bước 2: Cài đặt Extension
Đảm bảo đã cài đặt 2 extension sau trong VS Code:
1. **PlatformIO IDE** (bởi PlatformIO)
2. **Wokwi Simulator** (bởi Wokwi)

### Bước 3: Chạy mô phỏng Wokwi
1. Nhấp mở file `diagram.json`.
2. Bấm phím tắt `F1` (hoặc `Ctrl + Shift + P`), gõ và chọn:  
   `Wokwi: Start Simulator` (hoặc bấm nút Play màu xanh trên thanh công cụ Wokwi).
3. Màn hình Wokwi Simulator sẽ xuất hiện:
   - LCD 20x4 sáng xanh và hiển thị thông tin:
     ```
     SV: Luong The Cuong
     MSSV: 1250080025 | AUT
     Me: B20261027-01
     T: 180.0°C [R]
     ```
   - Terminal của VS Code bên dưới sẽ in log dữ liệu liên tục:
     ```json
     Data: {"batch":"B20261027-01", "elapsed_s":1, "temp":180.0, "alert":0, "http":201}
     ```

### Bước 4: Mở giao diện Web Localhost
1. Mở trình duyệt web (Chrome, Edge) hoặc dùng Simple Browser trong VS Code (`Ctrl + Shift + P` -> `Simple Browser: Show`).
2. Truy cập địa chỉ:
   ```
   http://localhost:8180/
   ```
3. Bạn sẽ thấy giao diện **"☕ Hệ Thống Giám Sát Lò Rang Cà Phê IoT"**:
   - Theo dõi nhiệt độ, thời gian, trạng thái cảnh báo quá nhiệt theo thời gian thực (tự động cập nhật mỗi 1s).
   - Nút bật/tắt trực tiếp 4 Relay (Heater, Cooling Fan, Drum Motor, Exhaust Fan).
   - Đổi chế độ **Tự động (AUTO)** theo đường cong chuẩn hoặc **Thủ công (MANUAL)** bằng núm vặn biến trở.
   - Thay đổi mã mẻ rang đang giám sát.

---

## 5. Kết nối với Backend Django của TMDT_Ban_Coffee

1. Khởi động Django Backend trong terminal:
   ```bash
   cd backend
   # Kích hoạt môi trường và đặt khóa bí mật
   set DEVICE_KEY=my-test-secret-key-123
   python manage.py runserver
   ```
2. Do trong `settings.py` đã cấu hình `ALLOWED_HOSTS = [..., "host.wokwi.internal"]`, ESP32 trong Wokwi sẽ tự động kết nối và gửi dữ liệu thành công với mã `201 Created`.
3. Kiểm tra kết quả truy xuất trên Frontend React:
   ```
   http://localhost:5173/trace/B20261027-01
   ```
   Biểu đồ đường cong nhiệt độ Chart.js sẽ tự động vẽ các điểm đo do ESP32 vừa gửi lên!

---

## 6. Chạy Script Phân Tích (Analysis Server)

Nếu muốn phân tích sâu đường cong nhiệt độ và tốc độ gia nhiệt RoR (°C/phút) hoặc chạy dashboard phụ:
```bash
cd iot/roaster-sim
python analysis_server.py
```
Mở trình duyệt tại `http://localhost:8180/` để xem bảng dữ liệu và đánh giá chất lượng mẻ rang.
