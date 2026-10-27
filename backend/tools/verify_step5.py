"""
Script kiểm tra Bước 5: API Thiết bị IoT (POST /api/iot/readings/)
"""
import os
import sys
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))

# Thiết lập biến môi trường DEVICE_KEY để test
TEST_KEY = "my-test-secret-key-123"
os.environ["DEVICE_KEY"] = TEST_KEY

import django
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from django.conf import settings
settings.DEVICE_KEY = TEST_KEY

from rest_framework.test import APIClient
from roasting.models import RoastBatch, RoastReading

client = APIClient()

print("=" * 60)
print("KIỂM TRA BƯỚC 5: API THIẾT BỊ IOT")
print("=" * 60)

# 1. Test thiếu hoặc sai khóa thiết bị -> 401
print("\n[Test 1] Gửi request thiếu hoặc sai X-Device-Key (Kỳ vọng: 401)")
res_bad_key = client.post(
    "/api/iot/readings/",
    {"batch_code": "B20261027-02", "elapsed_s": 10, "temperature_c": 150.0},
    format="json",
    HTTP_X_DEVICE_KEY="sai-khoa"
)
print("Status Code:", res_bad_key.status_code)
if res_bad_key.status_code == 401:
    print("-> ĐẠT: Đã chặn thiết bị sai khóa (401).")
else:
    print("-> CHƯA ĐẠT:", res_bad_key.data)

# 2. Test gửi vào mẻ đã kết thúc ('finished') -> 400
print("\n[Test 2] Gửi điểm đo vào mẻ đã kết thúc (Kỳ vọng: 400)")
batch_finished = RoastBatch.objects.filter(status="finished").first()
if batch_finished:
    res_finished = client.post(
        "/api/iot/readings/",
        {"batch_code": batch_finished.code, "elapsed_s": 10, "temperature_c": 150.0},
        format="json",
        HTTP_X_DEVICE_KEY=TEST_KEY
    )
    print(f"Status Code khi gửi vào mẻ {batch_finished.code} ({batch_finished.status}):", res_finished.status_code)
    if res_finished.status_code == 400:
        print("-> ĐẠT: Đã từ chối ghi dữ liệu vào mẻ đã kết thúc (400).")
    else:
        print("-> CHƯA ĐẠT:", res_finished.data)
else:
    print("-> Bỏ qua: Không tìm thấy mẻ finished (chạy seed_batches trước).")

# 3. Test gửi nhiệt độ vượt ngưỡng vào mẻ 'roasting' -> 201 & is_alert = True
print("\n[Test 3] Gửi nhiệt độ vượt ngưỡng vào mẻ đang rang (Kỳ vọng: 201 & is_alert=True)")
batch_roasting = RoastBatch.objects.filter(status="roasting").first()
if batch_roasting:
    high_temp = float(batch_roasting.target_max_temp_c) + 5.0
    res_valid = client.post(
        "/api/iot/readings/",
        {"batch_code": batch_roasting.code, "elapsed_s": 999, "temperature_c": high_temp},
        format="json",
        HTTP_X_DEVICE_KEY=TEST_KEY
    )
    print("Status Code:", res_valid.status_code)
    if res_valid.status_code == 201:
        data = res_valid.data
        print(f"-> Ghi nhận: temp={data.get('temperature_c')}°C, target={batch_roasting.target_max_temp_c}°C, is_alert={data.get('is_alert')}")
        if data.get("is_alert") is True:
            print("-> ĐẠT: Điểm đo lưu thành công và tự động đánh dấu is_alert = True.")
        else:
            print("-> CHƯA ĐẠT: is_alert chưa được bật True khi vượt ngưỡng.")
    else:
        print("-> CHƯA ĐẠT:", res_valid.data)
else:
    print("-> Bỏ qua: Không tìm thấy mẻ roasting.")

print("\n" + "=" * 60)