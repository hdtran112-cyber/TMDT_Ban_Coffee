"""
Script kiểm tra Bước 4: Phân quyền và API Quản trị mẻ rang (Admin Batches)
"""
import os
import sys
from pathlib import Path

# Thêm thư mục backend vào sys.path để Python nhận diện được package config
BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))

import django

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from django.contrib.auth.models import User
from rest_framework.authtoken.models import Token
from rest_framework.test import APIClient

client = APIClient()

print("=" * 60)
print("KIỂM TRA BƯỚC 4: API QUẢN TRỊ MẺ RANG")
print("=" * 60)

# 1. Kiểm tra khi không gửi Token xác thực
print("\n[Test 1] Truy cập không có Token (Kỳ vọng: 401 Unauthorized)")
res_no_auth = client.get("/api/admin/batches/")
print(f"Status Code: {res_no_auth.status_code}")
if res_no_auth.status_code == 401:
    print("-> ĐẠT: Đã chặn truy cập không xác thực (401).")
else:
    print(f"-> CHƯA ĐẠT: Nhận mã {res_no_auth.status_code} thay vì 401.")

# 2. Kiểm tra với tài khoản thường (is_staff=False)
print("\n[Test 2] Truy cập bằng Token khách thường (Kỳ vọng: 403 Forbidden)")
user_normal, _ = User.objects.get_or_create(username="khach_thuong", defaults={"is_staff": False})
token_normal, _ = Token.objects.get_or_create(user=user_normal)

client.credentials(HTTP_AUTHORIZATION="Token " + token_normal.key)
res_forbidden = client.get("/api/admin/batches/")
print(f"Status Code: {res_forbidden.status_code}")
if res_forbidden.status_code == 403:
    print("-> ĐẠT: Đã chặn tài khoản thường không có quyền admin (403).")
else:
    print(f"-> CHƯA ĐẠT: Nhận mã {res_forbidden.status_code} thay vì 403.")

# 3. Kiểm tra với tài khoản Admin (is_staff=True) để tạo mẻ mới
print("\n[Test 3] Tạo mẻ rang mới bằng Token Admin (Kỳ vọng: 201 Created)")
admin_user, _ = User.objects.get_or_create(username="admin_cuong", defaults={"is_staff": True})
token_admin, _ = Token.objects.get_or_create(user=admin_user)

client.credentials(HTTP_AUTHORIZATION="Token " + token_admin.key)
payload = {
    "product_id": 1,
    "roast_level": "medium",
    "roasted_at": "2026-10-27T08:00:00Z",
    "green_weight_g": 1200,
    "target_max_temp_c": 208.5,
    "notes": "Mẻ rang tạo tự động qua verify script"
}
res_create = client.post("/api/admin/batches/", payload, format="json")
print(f"Status Code: {res_create.status_code}")

if res_create.status_code == 201:
    batch_data = res_create.data
    code = batch_data.get("code")
    status_batch = batch_data.get("status")
    print(f"-> Mã mẻ sinh tự động: {code}")
    print(f"-> Trạng thái ban đầu: {status_batch}")
    
    import re
    if re.match(r"^B\d{8}-\d{2}$", str(code)) and status_batch == "roasting":
        print("-> ĐẠT: Mã sinh đúng quy chuẩn (BYYYYMMDD-NN) và status là 'roasting'.")
    else:
        print("-> CẢNH BÁO: Mã mẻ hoặc trạng thái chưa đúng quy ước.")
else:
    print(f"-> CHƯA ĐẠT: Lỗi tạo mẻ: {res_create.data}")

print("\n" + "=" * 60)