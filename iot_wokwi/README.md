# ĐẶC TẢ KỸ THUẬT: HỆ THỐNG IOT GIÁM SÁT & ĐIỀU KHIỂN LÒ RANG CÀ PHÊ CÔNG NGHIỆP (ESP32 + WOKWI FULL RIG)

**Dự án:** TMDT_Ban_Coffee (Website TMĐT bán lẻ cà phê tích hợp truy xuất nguồn gốc & giám sát mẻ rang IoT)  
**Nhóm thực hiện:** 12_ĐH-THMT_Nhom14  
**Trạng thái:** ✅ HOÀN THÀNH TOÀN DIỆN MODULE IOT  

---

## 1. Danh sách linh kiện & Sơ đồ nối chân (`diagram.json`)

Hệ thống thiết kế theo mô hình trạm điều khiển lò rang cà phê công nghiệp chuẩn với 9 nhóm linh kiện:

| STT | Linh kiện | ID Wokwi | Chân kết nối ESP32 | Chức năng |
|:---:|---|---|---|---|
| 1 | **ESP32 DevKit V1** | `esp` | Vi điều khiển trung tâm | Xử lý cân bằng nhiệt, điều khiển ngoại vi, Web Server & đẩy API |
| 2 | **Màn hình LCD 20x4 (I2C)** | `lcd` | SDA: GPIO 21, SCL: GPIO 22, VCC: 5V, GND | Hiển thị 4 dòng: Mã mẻ, Bean Temp, Env Temp, Gas %, Air %, Phase |
| 3 | **Cảm biến DHT22** | `dht` | SDA: GPIO 4, VCC: 3V3, GND | Đo nhiệt độ và độ ẩm môi trường xưởng rang / ống xả |
| 4 | **Chiết áp Gas/Lửa** | `pot_gas` | SIG: GPIO 34 (ADC1), VCC: 3V3, GND | Chỉnh công suất ngọn lửa đốt lồng rang (0% - 100%) |
| 5 | **Chiết áp Gió/Airflow** | `pot_air` | SIG: GPIO 35 (ADC1), VCC: 3V3, GND | Chỉnh lưu lượng gió hút khói (0% - 100%) |
| 6 | **Động cơ Servo van gió** | `servo` | PWM: GPIO 18, V+: 5V, GND | Quay góc 0° - 180° theo % lưu lượng gió |
| 7 | **LED Xanh lá (Normal)** | `led_green` | Anode: GPIO 12, Cathode: GND | Lò hoạt động bình thường (< 195.0°C) |
| 8 | **LED Vàng (Warning)** | `led_yellow` | Anode: GPIO 14, Cathode: GND | Cảnh báo tiệm cận First Crack (195.0°C - 204.9°C) |
| 9 | **LED Đỏ (Alert)** | `led_red` | Anode: GPIO 27, Cathode: GND | Báo động quá nhiệt nguy hiểm (>= 205.0°C) |
| 10 | **Còi Buzzer báo động** | `buzzer` | Signal: GPIO 26, GND | Kêu bíp cảnh báo khi quá nhiệt (>= 205.0°C) |
| 11 | **Nút nhấn Xả mẻ / Emergency** | `btn_dump` | Pin: GPIO 19 (INPUT_PULLUP), GND | Nhấn kích hoạt chế độ xả nhiệt khẩn cấp (Air 100%, Gas 0%) |

---

## 2. Mô hình vận hành 3 cấp độ nhiệt & Đèn cảnh báo

1. **Khởi động / Chạy tự động (Mức Xanh - `< 195°C`):**
   - Núm vặn Gas nằm góc trái (`< 30%`).
   - Lò tự động gia nhiệt theo đường cong S-Curve chuẩn: Nạp hạt -> Sấy (Drying) -> Vàng hóa (Yellowing).
   - **Đèn Xanh sáng**, Đèn Vàng tắt, Đèn Đỏ tắt, Còi tắt.
   - LCD dòng 0: `B:B20261027-02 [AUTO]`.

2. **Kéo chỉnh nhiệt độ thêm tí (Mức Vàng Cảnh Báo - `195°C - 204.9°C`):**
   - Vặn núm Gas nhẹ lên khoảng `30% - 65%`.
   - Nhiệt độ lò tăng lên vùng `196.0°C - 204.0°C` (tiệm cận First Crack).
   - **Đèn Vàng sáng**, Đèn Xanh tắt, Đèn Đỏ tắt, Còi tắt (chưa nguy hiểm).
   - LCD dòng 0: `B:B20261027-02 [WARN]`.

3. **Kéo quá tay (Mức Đỏ Báo Động Quá Nhiệt - `≥ 205.0°C`):**
   - Vặn núm Gas sang phải nhiều hơn (`> 65%`).
   - Nhiệt độ lò vượt ngưỡng an toàn lên `207.0°C - 228.0°C`.
   - **Đèn Đỏ sáng rực**, Đèn Xanh tắt, Đèn Vàng tắt.
   - **Còi Buzzer hú tít tít liên tục**, LCD dòng 0: `B:B20261027-02 [ALRT]`.
   - Backend Django ghi nhận bản ghi `is_alert: true`.

4. **Khôi phục trạng thái an toàn:**
   - Vặn núm Gas về lại sát góc trái (`< 30%`): Hệ thống lập tức khôi phục Đèn Xanh, còi tắt ngay.
   - Hoặc bấm nút `🤖 Chạy Tự Động Chuẩn` trên Web Dashboard: Xóa còi hú, xóa đèn đỏ, khôi phục chế độ bình thường.

---

## 3. Giao tiếp Backend Django REST Framework (Hợp đồng API)
- **Endpoint:** `POST http://10.0.2.2:8000/api/iot/readings/` (hoặc `http://host.wokwi.internal:8000/api/iot/readings/`)
- **Headers:**
  - `Content-Type: application/json`
  - `X-Device-Key: my-test-secret-key-123`
- **Payload:**
  ```json
  {
    "batch_code": "B20261027-02",
    "elapsed_s": 90,
    "temperature_c": 198.5
  }
  ```
- **Chu kỳ gửi:** 3 giây/lần.

---

## 4. Giao diện Web Dashboard Localhost (`http://localhost:8088/`)
- Truy cập trực tiếp tại `http://localhost:8088/` khi Wokwi đang chạy.
- Đồng bộ 100% dữ liệu thực từ phần cứng Wokwi:
  - Tên nhóm: **12_ĐH-THMT_Nhom14**.
  - Nhiệt độ hạt Bean Temp tức thời và Pha rang (Drying / Yellowing / First Crack / Development).
  - Trạng thái 3 đèn LED màu (Xanh / Vàng / Đỏ) mô phỏng thời gian thực.
  - Các nút thao tác nhanh: `Chạy Tự Động Chuẩn`, `Thử Mức Vàng (198°C)`, `Thử Mức Đỏ (212°C)`, `Xả Gió Khẩn Cấp`.
