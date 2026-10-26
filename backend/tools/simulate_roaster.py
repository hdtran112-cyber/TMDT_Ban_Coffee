"""
Script mô phỏng thiết bị IoT gửi điểm đo nhiệt độ mẻ rang theo chu kỳ.
Cách chạy:
    python tools/simulate_roaster.py --batch B20261026-01
"""
import argparse
import os
import time
import urllib.request
import urllib.error
import json

DEFAULT_API_URL = "http://127.0.0.1:8000/api/iot/readings/"
DEFAULT_DEVICE_KEY = os.environ.get("IOT_DEVICE_KEY", "roaster-secret-key-2026")

# Đường cong nhiệt độ rang cà phê thực tế (Charge -> Turning Point -> Drying -> Maillard -> First Crack -> Drop)
ROAST_CURVE = [
    (0, 180.0),
    (60, 98.5),    # Turning point (hạt mới đổ vào hút nhiệt)
    (120, 118.0),
    (180, 135.4),
    (240, 150.2),
    (300, 163.8),
    (360, 175.0),
    (420, 184.5),
    (480, 192.0),
    (540, 198.2),  # First crack (nổ lần 1)
    (600, 202.5),
    (660, 205.4),  # Đạt đỉnh chuẩn
]


def send_reading(api_url, device_key, batch_code, elapsed_s, temp_c):
    payload = json.dumps({
        "batch_code": batch_code,
        "elapsed_s": elapsed_s,
        "temperature_c": temp_c,
    }).encode("utf-8")

    req = urllib.request.Request(
        api_url,
        data=payload,
        headers={
            "Content-Type": "application/json",
            "X-Device-Key": device_key,
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            alert_str = " [CẢNH BÁO VƯỢT NGƯỠNG!]" if data.get("is_alert") else ""
            print(f"[OK] {elapsed_s:3d}s -> {temp_c:5.1f}°C{alert_str}")
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8")
        print(f"[LỖI {e.code}] {err_body}")


def main():
    parser = argparse.ArgumentParser(description="Mô phỏng cảm biến máy rang IoT")
    parser.add_argument("--batch", required=True, help="Mã mẻ rang (VD: B20261026-01)")
    parser.add_argument("--url", default=DEFAULT_API_URL, help="URL API nhận điểm đo")
    parser.add_argument("--key", default=DEFAULT_DEVICE_KEY, help="X-Device-Key")
    parser.add_argument("--interval", type=float, default=0.5, help="Thời gian nghỉ giữa các lần gửi (giây)")
    parser.add_argument("--overheat", action="store_true", help="Giả lập thêm điểm quá nhiệt để test cảnh báo")
    args = parser.parse_args()

    curve = list(ROAST_CURVE)
    if args.overheat:
        curve.append((720, 218.0))  # Điểm vượt ngưỡng để kích hoạt is_alert = True

    print(f"=== Bắt đầu mô phỏng IoT cho mẻ {args.batch} ===")
    for elapsed_s, temp_c in curve:
        send_reading(args.url, args.key, args.batch, elapsed_s, temp_c)
        time.sleep(args.interval)
    print("=== Hoàn tất gửi dữ liệu mẻ rang ===")


if __name__ == "__main__":
    main()