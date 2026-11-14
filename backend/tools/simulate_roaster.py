"""
Script mô phỏng thiết bị rang cà phê gửi dữ liệu IoT lên backend.
Đọc khóa từ biến môi trường DEVICE_KEY (hoặc tham số dòng lệnh).
"""
import os
import sys
import time
import json
import argparse
import urllib.request
import urllib.error

# Thiết lập đường dẫn backend để import model nếu cần
from pathlib import Path
BASE_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE_DIR))

# Đọc cấu hình mặc định
DEFAULT_API_URL = "http://127.0.0.1:8000/api/iot/readings/"
DEVICE_KEY = os.environ.get("DEVICE_KEY", "my-test-secret-key-123")


def send_reading(api_url, key, batch_code, elapsed_s, temp_c):
    data = {
        "batch_code": batch_code,
        "elapsed_s": elapsed_s,
        "temperature_c": round(temp_c, 1),
    }
    payload = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(
        api_url,
        data=payload,
        headers={
            "Content-Type": "application/json",
            "X-Device-Key": key,
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req) as resp:
            body = resp.read().decode("utf-8")
            res_json = json.loads(body)
            alert = " [QUÁ NHIỆT]" if res_json.get("is_alert") else ""
            print(f"[OK] Giây {elapsed_s:03d}s | Nhiệt độ: {temp_c:.1f}°C{alert}")
            return True
    except urllib.error.HTTPError as e:
        error_content = e.read().decode("utf-8")
        print(f"[LỖI HTTP {e.code}] {error_content}")
        return False
    except Exception as e:
        print(f"[LỖI KẾT NỐI] {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Mô phỏng cảm biến IoT gửi nhiệt độ lò rang")
    parser.add_argument("--batch", type=str, required=True, help="Mã mẻ rang (VD: B20261027-02)")
    parser.add_argument("--key", type=str, default=DEVICE_KEY, help="Khóa bí mật X-Device-Key")
    parser.add_argument("--interval", type=float, default=2.0, help="Chu kỳ gửi (giây)")
    args = parser.parse_args()

    print("=" * 60)
    print(f"BẮT ĐẦU MÔ PHỎNG THIẾT BỊ RANG CHO MẺ: {args.batch}")
    print(f"Server API: {DEFAULT_API_URL}")
    print(f"X-Device-Key: {args.key[:4]}****")
    print(f"Chu kỳ gửi: {args.interval}s")
    print("=" * 60)

    # Đường cong rang thực tế: Khởi điểm lồng nhiệt -> Hồi nhiệt -> Gia nhiệt -> Crack
    curve = [
        (0, 180.0),
        (30, 130.0),
        (60, 105.0),  # Turning point
        (90, 115.0),
        (120, 132.0),
        (150, 148.0),
        (180, 162.0),
        (210, 175.0),
        (240, 185.0),
        (270, 195.0),
        (300, 203.0),
        (330, 207.5), # Cảnh báo nếu ngưỡng là 205°C
    ]

    for elapsed_s, temp in curve:
        success = send_reading(DEFAULT_API_URL, args.key, args.batch, elapsed_s, temp)
        if not success:
            print("Dừng mô phỏng do gặp lỗi từ server.")
            break
        time.sleep(args.interval)

    print("\nHoàn tất chuỗi mô phỏng gửi dữ liệu cảm biến!")


if __name__ == "__main__":
    main()