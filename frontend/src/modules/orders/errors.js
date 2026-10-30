// Đổi lỗi từ API thành câu tiếng Việt để hiện cho khách.
export function apiErrorText(err, fallback = "Có lỗi xảy ra, thử lại sau.") {
  if (err?.status === 401) return "Bạn cần đăng nhập để tiếp tục.";
  const data = err?.data;
  if (data && typeof data === "object") {
    const text = Object.values(data)
      .flat()
      .filter((v) => typeof v === "string")
      .join(" ");
    if (text) return text;
  }
  return fallback;
}

// Lỗi 400 dạng {tên_trường: ["thông báo"]} -> {tên_trường: "thông báo"}
// để hiện ngay dưới từng ô nhập.
export function fieldErrors(err) {
  const data = err?.data;
  if (!data || typeof data !== "object" || Array.isArray(data)) return {};
  const result = {};
  for (const [key, value] of Object.entries(data)) {
    result[key] = [].concat(value).filter((v) => typeof v === "string").join(" ");
  }
  return result;
}
