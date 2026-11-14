"""
Hệ thống Giám sát & Phân tích Mẻ rang Cà phê IoT (Analysis Server)
Dự án: TMDT_Ban_Coffee - Module C: Giám sát mẻ rang IoT
Nhóm: IOT - NHÓM 9

Công dụng:
- Cung cấp Web Server phân tích tại http://localhost:8180/ (hoặc làm server dự phòng)
- Kết nối tới Django Backend API để lấy dữ liệu đường cong mẻ rang và phân tích RoR (Rate of Rise)
- Hỗ trợ mô phỏng và kiểm tra luồng dữ liệu IoT hoàn chỉnh
"""

import sys
import json
import urllib.request
import urllib.error
from http.server import HTTPServer, BaseHTTPRequestHandler
import argparse

DEFAULT_PORT = 8180
DJANGO_API_BASE = "http://127.0.0.1:8000/api"
DEFAULT_BATCH = "B20261027-01"

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>☕ Phân Tích Mẻ Rang Cà Phê IoT | Localhost:{port}</title>
  <style>
    :root {{
      --primary: #6F4E37;
      --primary-dark: #4A2C11;
      --accent: #D4A373;
      --bg: #F8F9FA;
      --card-bg: #FFFFFF;
      --text: #212529;
      --text-muted: #6C757D;
      --success: #28A745;
      --danger: #DC3545;
      --warning: #FFC107;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }}
    body {{ background: var(--bg); color: var(--text); padding: 20px; }}
    .container {{ max-width: 900px; margin: 0 auto; }}
    .header-card {{
      background: linear-gradient(135deg, var(--primary-dark), var(--primary));
      color: white;
      padding: 24px 28px;
      border-radius: 16px;
      margin-bottom: 24px;
      box-shadow: 0 8px 24px rgba(74, 44, 17, 0.2);
    }}
    .header-card h1 {{ font-size: 24px; margin-bottom: 8px; }}
    .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 24px; }}
    @media (max-width: 768px) {{ .grid {{ grid-template-columns: 1fr; }} }}
    .card {{
      background: var(--card-bg);
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 4px 15px rgba(0,0,0,0.05);
      border: 1px solid #E9ECEF;
    }}
    .card-title {{
      font-size: 17px;
      font-weight: 700;
      color: var(--primary-dark);
      margin-bottom: 16px;
      padding-bottom: 10px;
      border-bottom: 2px solid #F1F3F5;
      display: flex;
      justify-content: space-between;
    }}
    .metric-row {{ display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px dashed #E9ECEF; }}
    .metric-row:last-child {{ border-bottom: none; }}
    .metric-label {{ color: var(--text-muted); font-weight: 500; }}
    .metric-val {{ font-weight: 700; }}
    .btn {{
      padding: 10px 18px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
      border: none;
      background: var(--primary);
      color: white;
      transition: 0.2s;
    }}
    .btn:hover {{ background: var(--primary-dark); }}
    .search-box {{ display: flex; gap: 10px; margin-bottom: 15px; }}
    .search-box input {{ flex: 1; padding: 10px; border-radius: 8px; border: 1px solid #CED4DA; }}
    table {{ width: 100%; border-collapse: collapse; margin-top: 15px; font-size: 14px; }}
    th, td {{ padding: 10px; text-align: left; border-bottom: 1px solid #E9ECEF; }}
    th {{ background: #F8F9FA; color: var(--text-muted); font-weight: 600; }}
    .badge-alert {{ color: var(--danger); font-weight: bold; }}
  </style>
</head>
<body>
  <div class="container">
    <div class="header-card">
      <h1>☕ Trung Tâm Phân Tích Mẻ Rang Cà Phê IoT - IOT - NHÓM 9</h1>
      <p><strong>Nhóm:</strong> IOT - NHÓM 9 &nbsp;|&nbsp; <strong>Sinh viên:</strong> Lương Thế Cường &nbsp;|&nbsp; <strong>MSSV:</strong> 1250080025 &nbsp;|&nbsp; <strong>Lớp:</strong> 12_ĐH_THMT</p>
    </div>

    <div class="card" style="margin-bottom: 20px;">
      <div class="card-title">🔍 Tra cứu & Phân tích Mẻ rang từ Django Backend</div>
      <div class="search-box">
        <input type="text" id="batchInput" value="{default_batch}" placeholder="Nhập mã mẻ rang (VD: B20261027-01)">
        <button class="btn" onclick="loadTraceData()">Phân Tích Mẻ</button>
      </div>
      <div id="statusMsg" style="font-size: 13px; color: var(--text-muted);">Đang kết nối tới: {api_base}/trace/...</div>
    </div>

    <div class="grid">
      <div class="card">
        <div class="card-title">📊 Thông Tin Tổng Hợp Mẻ Rang</div>
        <div class="metric-row"><span class="metric-label">Tên sản phẩm:</span><span class="metric-val" id="prodName">--</span></div>
        <div class="metric-row"><span class="metric-label">Mức rang:</span><span class="metric-val" id="roastLevel">--</span></div>
        <div class="metric-row"><span class="metric-label">Độ tươi:</span><span class="metric-val" id="freshness">--</span></div>
        <div class="metric-row"><span class="metric-label">Thời gian rang:</span><span class="metric-val" id="duration">--</span></div>
        <div class="metric-row"><span class="metric-label">Nhiệt độ đỉnh (Max):</span><span class="metric-val" id="maxTemp" style="color:var(--primary); font-size:18px;">-- °C</span></div>
        <div class="metric-row"><span class="metric-label">Số lần vượt ngưỡng:</span><span class="metric-val" id="alertCount">0</span></div>
      </div>

      <div class="card">
        <div class="card-title">🔥 Phân Tích Tốc Độ Gia Nhiệt (RoR)</div>
        <div class="metric-row"><span class="metric-label">Tổng số điểm đo:</span><span class="metric-val" id="totalReadings">0</span></div>
        <div class="metric-row"><span class="metric-label">Điểm hồi nhiệt (Turning Point):</span><span class="metric-val" id="turningPoint">~60s (105°C)</span></div>
        <div class="metric-row"><span class="metric-label">Tốc độ RoR trung bình:</span><span class="metric-val" id="avgRor">-- °C/phút</span></div>
        <div class="metric-row"><span class="metric-label">Trạng thái chất lượng:</span><span class="metric-val" id="qualityState" style="color:var(--success);">ĐẠT TIÊU CHUẨN</span></div>
      </div>
    </div>

    <div class="card">
      <div class="card-title">📈 Chi Tiết Dữ Liệu Các Điểm Đo IoT</div>
      <table id="readingsTable">
        <thead>
          <tr>
            <th>Thời gian (Giây)</th>
            <th>Thời gian (Phút:Giây)</th>
            <th>Nhiệt độ (°C)</th>
            <th>RoR (°C/phút)</th>
            <th>Trạng thái</th>
          </tr>
        </thead>
        <tbody>
          <tr><td colspan="5" style="text-align:center;">Chưa có dữ liệu</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <script>
    function loadTraceData() {{
      var batch = document.getElementById('batchInput').value.trim();
      if (!batch) return;

      document.getElementById('statusMsg').innerText = 'Đang tải dữ liệu từ API cho mẻ: ' + batch + '...';

      fetch('/api/proxy/trace/' + encodeURIComponent(batch))
        .then(function(res) {{
          if (!res.ok) throw new Error('Mã mẻ không tồn tại hoặc lỗi server (HTTP ' + res.status + ')');
          return res.json();
        }})
        .then(function(data) {{
          document.getElementById('statusMsg').innerText = '✅ Đã tải thành công mẻ rang ' + data.code;
          document.getElementById('prodName').innerText = data.product ? data.product.name : '--';
          document.getElementById('roastLevel').innerText = (data.roast_level || '--').toUpperCase();
          document.getElementById('freshness').innerText = (data.freshness_label || '') + ' (' + data.freshness_days + ' ngày)';
          document.getElementById('duration').innerText = (data.summary ? data.summary.duration_s : 0) + 's';
          document.getElementById('maxTemp').innerText = (data.summary ? data.summary.max_temp_c : 0).toFixed(1) + ' °C';
          document.getElementById('alertCount').innerText = data.summary ? data.summary.alert_count : 0;

          var readings = data.readings || [];
          document.getElementById('totalReadings').innerText = readings.length;

          // Render bảng
          var tbody = document.querySelector('#readingsTable tbody');
          tbody.innerHTML = '';
          if (readings.length === 0) {{
            tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;">Mẻ này chưa có điểm đo nào</td></tr>';
            return;
          }}

          var prevSec = null, prevTemp = null;
          var totalRor = 0, rorCount = 0;

          readings.forEach(function(r) {{
            var tr = document.createElement('tr');
            var m = Math.floor(r.elapsed_s / 60);
            var s = r.elapsed_s % 60;
            var timeFmt = (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;

            var rorStr = '--';
            if (prevSec !== null && r.elapsed_s > prevSec) {{
              var dtMin = (r.elapsed_s - prevSec) / 60.0;
              var dTemp = r.temperature_c - prevTemp;
              var ror = dTemp / dtMin;
              rorStr = (ror >= 0 ? '+' : '') + ror.toFixed(1);
              totalRor += ror;
              rorCount++;
            }}

            prevSec = r.elapsed_s;
            prevTemp = r.temperature_c;

            var isOver = r.temperature_c > 205.0;
            var statusBadge = isOver ? '<span class="badge-alert">⚠️ Quá nhiệt</span>' : '<span style="color:var(--success);">Bình thường</span>';

            tr.innerHTML = '<td>' + r.elapsed_s + 's</td><td>' + timeFmt + '</td><td><strong>' + r.temperature_c.toFixed(1) + ' °C</strong></td><td>' + rorStr + '</td><td>' + statusBadge + '</td>';
            tbody.appendChild(tr);
          }});

          if (rorCount > 0) {{
            document.getElementById('avgRor').innerText = (totalRor / rorCount).toFixed(1) + ' °C/phút';
          }}
        }})
        .catch(function(err) {{
          document.getElementById('statusMsg').innerText = '❌ ' + err.message;
        }});
    }}

    window.onload = loadTraceData;
  </script>
</body>
</html>
"""

class RoasterAnalysisHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            content = HTML_TEMPLATE.format(
                port=self.server.server_port,
                default_batch=DEFAULT_BATCH,
                api_base=DJANGO_API_BASE
            ).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        elif self.path.startswith("/api/proxy/trace/"):
            batch_code = self.path.split("/api/proxy/trace/")[-1].split("?")[0]
            url = f"{DJANGO_API_BASE}/trace/{batch_code}/"
            try:
                req = urllib.request.Request(url)
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = resp.read()
                    self.send_response(resp.status)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Access-Control-Allow-Origin", "*")
                    self.end_headers()
                    self.wfile.write(data)
            except urllib.error.HTTPError as e:
                self.send_response(e.code)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(e.read())
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"detail": str(e)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Xuất log ngắn gọn
        print(f"[AnalysisServer] {self.address_string()} - {format % args}")


def main():
    parser = argparse.ArgumentParser(description="Analysis Server cho mô phỏng lò rang cà phê IoT")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"Cổng HTTP (mặc định {DEFAULT_PORT})")
    args = parser.parse_args()

    server_address = ("", args.port)
    httpd = HTTPServer(server_address, RoasterAnalysisHandler)
    print("=" * 65)
    print(f"☕ CHẠY ANALYSIS SERVER TẠI: http://localhost:{args.port}/")
    print(f"Kết nối dữ liệu tới Django Backend: {DJANGO_API_BASE}")
    print("Nhấn Ctrl + C để dừng server")
    print("=" * 65)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nĐã dừng server.")


if __name__ == "__main__":
    main()
