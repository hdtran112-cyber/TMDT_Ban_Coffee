import { http } from "./http";

// Tất cả hàm đơn hàng cần đăng nhập (http.js tự gắn token)

export function createOrder(payload) {
  return http("/orders/", { method: "POST", body: payload });
}

export function getOrders() {
  return http("/orders/");
}

export function getOrder(id) {
  return http(`/orders/${id}/`);
}

export function cancelOrder(id) {
  return http(`/orders/${id}/cancel/`, { method: "POST" });
}

export function payOrder(id) {
  return http(`/orders/${id}/pay/`, { method: "POST" });
}
