const API_URL = import.meta.env.VITE_API_URL || "http://127.0.0.1:8000/api";

export async function http(path, { method = "GET", body } = {}, retried = false) {
  const headers = { "Content-Type": "application/json" };
  const token = localStorage.getItem("token");
  if (token) headers.Authorization = `Token ${token}`;

  const res = await fetch(`${API_URL}${path}`, {
    method,
    headers,
    body: body ? JSON.stringify(body) : undefined,
  });

  // Token hỏng hoặc hết hạn: bỏ token, thử lại đúng một lần
  if (res.status === 401 && token && !retried) {
    localStorage.removeItem("token");
    return http(path, { method, body }, true);
  }

  const data = res.status === 204 ? null : await res.json().catch(() => null);
  if (!res.ok) {
    const err = new Error(`API lỗi ${res.status}`);
    err.status = res.status;
    err.data = data;
    throw err;
  }
  return data;
}
