import { NavLink, Outlet } from "react-router-dom";

export default function Layout() {
  return (
    <>
      <nav style={{ display: "flex", gap: 16, padding: 12, borderBottom: "1px solid #ddd" }}>
        <NavLink to="/">Sản phẩm</NavLink>
        <NavLink to="/cart">Giỏ hàng</NavLink>
        <NavLink to="/orders">Đơn của tôi</NavLink>
        <NavLink to="/trace">Truy xuất</NavLink>
        <NavLink to="/login">Đăng nhập</NavLink>
      </nav>
      <main style={{ padding: 16 }}>
        <Outlet />
      </main>
    </>
  );
}
