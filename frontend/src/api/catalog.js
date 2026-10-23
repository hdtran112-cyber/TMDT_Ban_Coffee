import { http } from "./http";

export function getProducts(params = {}) {
  const query = new URLSearchParams(
    Object.entries(params).filter(([, v]) => v !== "" && v != null)
  ).toString();
  return http(`/products/${query ? `?${query}` : ""}`);
}

export function getProduct(id) {
  return http(`/products/${id}/`);
}
