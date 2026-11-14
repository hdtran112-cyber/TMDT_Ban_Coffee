# ĐẶC TẢ KỸ THUẬT: HỆ THỐNG IOT GIÁM SÁT & ĐIỀU KHIỂN LÒ RANG CÀ PHÊ CÔNG NGHIỆP (ESP32 + WOKWI FULL RIG)

**Dự án:** TMDT_Ban_Coffee (Website TMĐT bán lẻ cà phê tích hợp truy xuất nguồn gốc & giám sát mẻ rang IoT)  
**Nhóm thực hiện:** 12_ĐH-THMT_Nhom14  

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
| 7 | **LED Xanh lá (Normal)** | `led_green` | Anode: GPIO 12, Cathode: GND | Lò hoạt động bình thường (<= 195.0°C) |
| 8 | **LED Vàng (Warning)** | `led_yellow` | Anode: GPIO 14, Cathode: GND | Cảnh báo giai đoạn nhạy cảm (195.0°C - 205.0°C) |
| 9 | **LED Đỏ (Alert)** | `led_red` | Anode: GPIO 27, Cathode: GND | Báo động quá nhiệt nguy hiểm (> 205.0°C) |
| 10 | **Còi Buzzer báo động** | `buzzer` | Signal: GPIO 26, GND | Kêu bíp cảnh báo khi quá nhiệt (> 205.0°C) |
| 11 | **Nút nhấn Xả mẻ / Emergency** | `btn_dump` | Pin: GPIO 19 (INPUT_PULLUP), GND | Nhấn kích hoạt chế độ xả nhiệt khẩn cấp (Air 100%, Gas 0%) |

---

## 2. Mô hình vật lý & Thuật toán cân bằng nhiệt
- **Nhiệt độ nền tự động (`baseTemp`):** Tăng dần theo đường cong rang chuẩn từ `110.0°C` lên `195.0°C` theo thời gian `elapsed_s`.
- **Tác động của Lửa (`gasEffect`):** Lửa càng lớn, nhiệt cộng thêm càng cao: `gasEffect = (gasPercent / 100) * 30.0` (tối đa `+30.0°C`).
- **Tác động làm mát của Gió (`airCooling`):** Gió hút càng mạnh, nhiệt giảm bớt: `airCooling = (airPercent / 100) * 12.0` (tối đa `-12.0°C`).
- **Nhiệt độ tức thời:**
  $$\text{currentTemp} = \text{baseTemp} + \text{gasEffect} - \text{airCooling}$$
- **4 Pha rang hiển thị LCD 20x4:**
  - `< 140°C`: `DRYING`
  - `140°C - 175°C`: `YELLOWING`
  - `175°C - 196°C`: `FIRST CRACK`
  - `> 196°C`: `DEVELOPMENT`

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

## 4. Giao diện Web Dashboard Localhost (`http://localhost:8180/`)
- Truy cập trực tiếp tại `http://localhost:8180/` khi Wokwi đang chạy.
- Hiển thị trực quan:
  - Tên nhóm: **12_ĐH-THMT_Nhom14**.
  - Nhiệt độ hạt Bean Temp tức thời và Pha rang (Drying / Yellowing / First Crack / Development).
  - Thanh tiến trình công suất Lửa (Gas %) và Lưu lượng Gió (Airflow %).
  - Góc mở thực tế của van Servo (0° - 180°).
  - Nhiệt độ và độ ẩm xưởng đo từ DHT22.
  - Trạng thái 3 đèn LED màu (Xanh / Vàng / Đỏ) và nút bấm kích hoạt xả mẻ khẩn cấp.
