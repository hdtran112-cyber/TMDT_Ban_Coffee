# MODULE MÔ PHỎNG PHẦN CỨNG IOT LÒ RANG CÀ PHÊ (ESP32 + WOKWI)

**Hệ thống:** TMDT_Ban_Coffee (Website TMĐT bán lẻ cà phê rang xay)  
**Nhóm thực hiện:** 12_ĐH-THMT_Nhom14  

---

## 1. Kiến trúc phần cứng mô phỏng (`diagram.json`)
Sơ đồ mạch gồm 4 linh kiện chuẩn Wokwi:
1. **Vi điều khiển:** `board-esp32-devkit-c-v4` (ESP32 38-pin / DevKit V1).
2. **Biến trở xoay tạo sự cố (Potentiometer):** `wokwi-potentiometer`
   - Chức năng: Giả lập tác động ngoại cảnh / lỗi gia nhiệt lồng rang (tạo độ lệch nhiệt cộng thêm từ `0°C` đến `+35°C`).
   - Nối chân: `GND` -> `esp:GND.1`, `VCC` -> `esp:3V3`, `SIG` -> `esp:34` (Analog Input).
3. **Màn hình hiển thị tại chỗ (LCD 1602 I2C):** `wokwi-lcd1602` (I2C address `0x27`).
   - Chức năng:
     - Dòng 1: Mã mẻ rang và trạng thái (`B20261027-02 [OK]` hoặc `[ALERT!]`).
     - Dòng 2: Nhiệt độ tức thời và mã nhóm (`T: 185.0°C Nhom14`).
   - Nối chân: `GND` -> `esp:GND.2`, `VCC` -> `esp:5V`, `SDA` -> `esp:21`, `SCL` -> `esp:22`.
4. **Đèn báo động quá nhiệt (LED Đỏ):** `wokwi-led`
   - Chức năng: Bật sáng ngay lập tức khi nhiệt độ lồng rang vượt ngưỡng an toàn (`> 205.0°C`).
   - Nối chân: `A` (Anode) -> `esp:2`, `C` (Cathode) -> `esp:GND.1`.

---

## 2. Kết nối Backend Django & Giao tiếp API
- **Endpoint tiếp nhận dữ liệu:** `POST http://10.0.2.2:8000/api/iot/readings/`
- **HTTP Headers:**
  - `Content-Type: application/json`
  - `X-Device-Key: my-test-secret-key-123`
- **Payload JSON mẫu:**
  ```json
  {
    "batch_code": "B20261027-02",
    "elapsed_s": 90,
    "temperature_c": 195.5
  }
  ```
- **Chu kỳ gửi:** 3 giây/lần.

---

## 3. Giao diện Web Dashboard Localhost (`http://localhost:8180/`)
- Mở trình duyệt truy cập `http://localhost:8180/` khi Wokwi đang chạy.
- Giao diện hiển thị trực quan thông số nhiệt độ thời gian thực, thời gian rang, mã mẻ, trạng thái đèn LED đỏ cảnh báo và trạng thái kết nối Backend Django.
- Tên nhóm hiển thị trên giao diện: **12_ĐH-THMT_Nhom14**.

---

## 4. Hướng dẫn chạy mô phỏng
1. Mở file `iot_wokwi/diagram.json` trong VS Code.
2. Nhấn nút **Play ▶️ (Start Simulation)** của Wokwi trên góc phải hoặc thanh công cụ.
3. Màn hình LCD 1602 sẽ bật sáng, hiển thị tên nhóm và các thông số mẻ rang.
4. Xoay núm biến trở để kiểm tra cảnh báo quá nhiệt (khi nhiệt độ vượt `205°C`, LED đỏ sẽ sáng và LCD đổi thành `[ALERT!]`).
5. Truy cập `http://localhost:8180/` trên trình duyệt máy tính để xem Web Dashboard thời gian thực.
