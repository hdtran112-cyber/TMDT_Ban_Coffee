import { http } from "./http";

// Token lưu ở localStorage với khóa "token": http.js đọc đúng khóa này
// để tự gắn vào header Authorization cho giỏ hàng, đơn hàng...
const TOKEN_KEY = "token";

export function getToken() {
  return localStorage.getItem(TOKEN_KEY);
}

// Báo cho Layout biết trạng thái đăng nhập vừa đổi để cập nhật thanh menu
function notifyAuthChanged() {
  window.dispatchEvent(new Event("auth-changed"));
}

export async function login(username, password) {
  const data = await http("/auth/login/", { method: "POST", body: { username, password } });
  localStorage.setItem(TOKEN_KEY, data.token);
  notifyAuthChanged();
  return data;
}

export function register(username, email, password) {
  return http("/auth/register/", { method: "POST", body: { username, email, password } });
}

export function getMe() {
  return http("/auth/me/");
}

export function logout() {
  localStorage.removeItem(TOKEN_KEY);
  notifyAuthChanged();
}
