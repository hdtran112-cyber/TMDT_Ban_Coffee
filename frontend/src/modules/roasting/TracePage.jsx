import React, { useEffect, useState } from "react";
import { useParams, useNavigate } from "react-router-dom";
import { QRCodeSVG } from "qrcode.react";
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from "chart.js";
import { Line } from "react-chartjs-2";
import { getTrace } from "../../api/roasting";

// Đăng ký các module ChartJS
ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

export default function TracePage() {
  const { code: routeCode } = useParams();
  const navigate = useNavigate();

  const [inputCode, setInputCode] = useState(routeCode || "");
  const [batchData, setBatchData] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  const fetchBatch = async (codeToFetch) => {
    if (!codeToFetch || !codeToFetch.trim()) return;
    setLoading(true);
    setError(null);
    setBatchData(null);

    try {
      const data = await getTrace(codeToFetch.trim());
      console.log("Dữ liệu mẻ rang nhận được:", data);

      if (data && (data.code || data.batch_code)) {
        setBatchData(data);
      } else {
        setError("Dữ liệu trả về không đúng định dạng.");
      }
    } catch (err) {
      console.error("Lỗi khi tải mẻ rang:", err);
      setError(
        err?.detail ||
        err?.message ||
        "Không tìm thấy mẻ rang. Vui lòng kiểm tra lại mã."
      );
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    if (routeCode) {
      setInputCode(routeCode);
      fetchBatch(routeCode);
    }
  }, [routeCode]);

  const handleSearch = (e) => {
    e.preventDefault();
    if (!inputCode.trim()) return;
    navigate(`/trace/${inputCode.trim()}`);
  };

  // Chuẩn bị dữ liệu hiển thị biểu đồ
  const readings = batchData?.readings || [];
  const chartLabels = readings.map((r) => `${(r.elapsed_s / 60).toFixed(1)}p`);
  const chartTemps = readings.map((r) => r.temperature_c);

  const chartData = {
    labels: chartLabels,
    datasets: [
      {
        label: "Nhiệt độ lồng rang (°C)",
        data: chartTemps,
        borderColor: "#b45309",
        backgroundColor: "rgba(180, 83, 9, 0.2)",
        tension: 0.3,
        pointRadius: 3,
        pointHoverRadius: 6,
      },
    ],
  };

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { position: "top" },
      title: {
        display: true,
        text: "Đường cong nhiệt độ mẻ rang (Roast Profile)",
      },
    },
    scales: {
      y: {
        title: { display: true, text: "Nhiệt độ (°C)" },
        suggestedMin: 80,
      },
      x: {
        title: { display: true, text: "Thời gian (phút)" },
      },
    },
  };

  return (
<div style={{ maxWidth: 880, margin: "20px auto", padding: "0 16px", fontFamily: "sans-serif" }}>
      {/* Khối tìm kiếm */}
      <div style={{ background: "#fdf8f6", padding: 20, borderRadius: 8, marginBottom: 24, border: "1px solid #fed7aa" }}>
        <h2 style={{ margin: "0 0 12px 0", color: "#9a3412" }}>Nguồn gốc mẻ rang cà phê</h2>
        <form onSubmit={handleSearch} style={{ display: "flex", gap: 10 }}>
          <input
            type="text"
            value={inputCode}
            onChange={(e) => setInputCode(e.target.value)}
            placeholder="Nhập mã mẻ rang (ví dụ: B20261025-01)"
            style={{ flex: 1, padding: "10px 14px", border: "1px solid #cbd5e1", borderRadius: 6, fontSize: 15 }}
          />
          <button
            type="submit"
            style={{ padding: "10px 20px", background: "#c2410c", color: "#fff", border: "none", borderRadius: 6, cursor: "pointer", fontWeight: "bold" }}
          >
            Tra
          </button>
        </form>
      </div>

      {loading && (
        <div style={{ textAlign: "center", padding: 30, color: "#64748b" }}>
          Đang truy xuất thông tin mẻ rang...
        </div>
      )}

      {error && (
        <div style={{ padding: 16, background: "#fee2e2", color: "#b91c1c", borderRadius: 6, marginBottom: 20, textAlign: "center" }}>
          {error}
        </div>
      )}

      {batchData && (
        <div>
          {/* Thông tin mẻ rang + QR Code */}
          <div style={{ display: "flex", flexWrap: "wrap", gap: 20, background: "#fff", border: "1px solid #e2e8f0", borderRadius: 8, padding: 24, marginBottom: 24 }}>
            <div style={{ flex: 2, minWidth: 260, lineHeight: 1.8 }}>
              <h3 style={{ margin: "0 0 10px 0", color: "#1e293b" }}>
                Mã mẻ: <span style={{ color: "#ea580c" }}>{batchData.code}</span>
              </h3>
              <div><strong>Sản phẩm:</strong> {batchData.product?.name} ({batchData.product?.bean_type})</div>
              <div><strong>Mức độ rang:</strong> {batchData.roast_level}</div>
              <div><strong>Thời gian rang:</strong> {batchData.roasted_at ? new Date(batchData.roasted_at).toLocaleString("vi-VN") : "N/A"}</div>
              <div>
                <strong>Độ tươi:</strong>{" "}
                <span style={{ color: "#16a34a", fontWeight: "bold" }}>{batchData.freshness_label}</span> ({batchData.freshness_days} ngày)
              </div>
              <div style={{ marginTop: 8, padding: "8px 12px", background: "#f8fafc", borderRadius: 6, display: "inline-block" }}>
                Thời gian: <strong>{batchData.summary?.duration_s}s</strong> | Đỉnh nhiệt: <strong>{batchData.summary?.max_temp_c}°C</strong> | Vượt ngưỡng:{" "}
                <strong style={{ color: batchData.summary?.alert_count > 0 ? "#dc2626" : "inherit" }}>
                  {batchData.summary?.alert_count} lần
                </strong>
</div>
            </div>

            <div style={{ flex: 1, minWidth: 150, display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center" }}>
              <QRCodeSVG value={window.location.href} size={135} />
              <span style={{ fontSize: 12, color: "#64748b", marginTop: 8 }}>Quét để tra cứu mẻ này</span>
            </div>
          </div>

          {/* Khối vẽ biểu đồ */}
          <div style={{ background: "#fff", border: "1px solid #e2e8f0", borderRadius: 8, padding: 20, height: 380 }}>
            {readings.length > 0 ? (
              <Line data={chartData} options={chartOptions} />
            ) : (
              <div style={{ textAlign: "center", color: "#94a3b8", paddingTop: 140 }}>
                Mẻ rang này chưa có dữ liệu cảm biến gửi lên.
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
