#ifndef SECRETS_H
#define SECRETS_H

// Cấu hình mạng WiFi Wokwi ảo
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""

// Khóa bảo mật thiết bị (khớp với biến môi trường DEVICE_KEY ở settings.py)
#define DEVICE_KEY "my-test-secret-key-123"

// Endpoint API Django Backend (10.0.2.2 hoặc host.wokwi.internal trỏ về localhost máy tính)
#define API_URL "http://10.0.2.2:8000/api/iot/readings/"

// Mã mẻ rang mặc định
#define DEFAULT_BATCH_CODE "B20261027-02"

#endif // SECRETS_H
