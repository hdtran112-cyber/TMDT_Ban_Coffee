import { http } from "./http";

// Tất cả hàm giỏ hàng cần đăng nhập (http.js tự gắn token)

export function getCart() {
  return http("/cart/");
}

export function addToCart(productId, quantity = 1) {
  return http("/cart/items/", {
    method: "POST",
    body: { product_id: productId, quantity },
  });
}

export function updateCartItem(itemId, quantity) {
  return http(`/cart/items/${itemId}/`, { method: "PATCH", body: { quantity } });
}

export function removeCartItem(itemId) {
  return http(`/cart/items/${itemId}/`, { method: "DELETE" });
}
