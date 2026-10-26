import { Link, NavLink, Outlet } from "react-router-dom";

export default function Layout() {
  return (
    <>
      <header className="site-header">
        <div className="inner">
          <Link to="/" className="brand">Cà Phê Rang</Link>
          <nav>
            <NavLink to="/" end>Sản phẩm</NavLink>
            <NavLink to="/cart">Giỏ hàng</NavLink>
            <NavLink to="/orders">Đơn của tôi</NavLink>
            <NavLink to="/trace">Truy xuất</NavLink>
            <NavLink to="/login">Đăng nhập</NavLink>
          </nav>
        </div>
      </header>
      <main className="page">
        <Outlet />
      </main>
    </>
  );
}
