# Thiết Bị Mô Phỏng Lò Rang Cà Phê IoT ESP32 (PlatformIO & Wokwi)

Dự án: **TMDT_Ban_Coffee** - Module C: Giám Sát & Truy Xuất Mẻ Rang Bằng IoT  
**Nhóm:** 12_ĐH-THMT_Nhom14  

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
| **Màn hình LCD 20x4 (I2C)** | SDA: GPIO 21, SCL: GPIO 22, VCC: 5V, GND | Hiển thị: 12_DH-THMT_Nhom14, Mã mẻ, Nhiệt độ |
| **Biến trở 1 (Temp Knob)** | SIG: GPIO 34 (ADC1), VCC: 3.3V, GND | Giả lập điều chỉnh nhiệt độ lồng rang (100°C - 240°C) |
| **Biến trở 2 (Fan Knob)** | SIG: GPIO 35 (ADC1), VCC: 3.3V, GND | Điều chỉnh tốc độ quạt hoặc luồng gió |
| **Cảm biến DHT22** | SDA: GPIO 15, VCC: 3.3V, GND | Đo nhiệt độ và độ ẩm môi trường phòng rang |
| **Nút bấm (Start/Stop)** | Pin: GPIO 13, GND (INPUT_PULLUP) | Nút cứng chuyển trạng thái Bắt đầu / Dừng rang |
| **Relay 1 + LED Đỏ** | IN: GPIO 26 | Điện trở gia nhiệt lò rang (Heater) |
| **Relay 2 + LED Xanh lá** | IN: GPIO 27 | Quạt làm nguội cà phê (Cooling Fan) |
| **Relay 3 + LED Xanh dương** | IN: GPIO 14 | Động cơ quay lồng rang (Drum Motor) |
| **Relay 4 + LED Vàng** | IN: GPIO 12 | Quạt hút khói / Đèn cảnh báo quá nhiệt (Exhaust/Alert) |

---

## 4. Hướng dẫn chạy mô phỏng trên VS Code

### Cách 1: Chạy mô phỏng Wokwi trong VS Code
1. Cài extension **Wokwi Simulator** trong VS Code.
2. Nhấp mở file `diagram.json`.
3. Bấm nút Play ▶️ màu xanh lá để bắt đầu mô phỏng.
4. Màn hình LCD 20x4 sẽ hiển thị:
   ```
   12_DH-THMT_Nhom14
   HE THONG LO RANG
   Me: B20261027-01
   T: 180.0°C [R]
   ```
5. Mở trình duyệt Web tại: `http://localhost:8180/` để xem Web Dashboard.

### Cách 2: Chạy trực tiếp trên trình duyệt Web Wokwi (Không cần cài Extension)
1. Truy cập [https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32).
2. Dán nội dung của file `diagram.json` vào tab `diagram.json` trên Wokwi.
3. Dán mã nguồn từ `src/main.cpp` vào tab code.
4. Bấm **Play** để xem mô phỏng hoạt động trực tiếp trên web!

### Cách 3: Chạy nhanh Web Dashboard bằng Analysis Server
```bash
cd iot/roaster-sim
python analysis_server.py
```
Mở trình duyệt tại `http://localhost:8180/` để xem bảng giám sát và điều khiển.
