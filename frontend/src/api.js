export const API_URL = import.meta.env.VITE_API_URL || "http://127.0.0.1:8000/api";

export async function getProducts(params = {}) {
  const query = new URLSearchParams(params).toString();
  const res = await fetch(`${API_URL}/products/${query ? "?" + query : ""}`);
  if (!res.ok) throw new Error("Không tải được danh sách sản phẩm");
  return res.json();
}
