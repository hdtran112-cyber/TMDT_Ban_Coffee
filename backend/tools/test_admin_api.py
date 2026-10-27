"""
Script test nhanh các API quản trị mẻ rang (Roasting Admin APIs)
Cách chạy:
    python tools/test_admin_api.py
"""
import json
import urllib.request
import urllib.error

BASE_URL = "http://127.0.0.1:8000/api"

def test_api():
    print("=== 1. Test xem danh sách mẻ rang (GET /api/admin/batches/) ===")
    try:
        req = urllib.request.Request(f"{BASE_URL}/admin/batches/", method="GET")
        with urllib.request.urlopen(req) as resp:
            print("Status:", resp.status)
            print("Data:", resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        print(f"Lỗi HTTP {e.code}: Phải có token xác thực quyền Admin (Đúng thiết kế bảo mật).")

if __name__ == "__main__":
    test_api()