export const ROAST_LABEL = { light: "Rang nhạt", medium: "Rang vừa", dark: "Rang đậm" };

export function formatPrice(vnd) {
  return `${vnd.toLocaleString("vi-VN")}đ`;
}
