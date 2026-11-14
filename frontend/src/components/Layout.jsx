import { useEffect, useState } from "react";
import { Link, NavLink, Outlet, useNavigate } from "react-router-dom";
import { getMe, getToken, logout } from "../api/auth";

export default function Layout() {
  const navigate = useNavigate();
  const [user, setUser] = useState(null);

  // Lấy thông tin người dùng khi mở trang và mỗi lần đăng nhập/đăng xuất
  useEffect(() => {
    function loadUser() {
      if (!getToken()) {
        setUser(null);
        return;
      }
      getMe()
        .then(setUser)
        .catch(() => setUser(null));
    }
    loadUser();
    window.addEventListener("auth-changed", loadUser);
    return () => window.removeEventListener("auth-changed", loadUser);
  }, []);

  function onLogout() {
    logout();
    navigate("/");
  }

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
            {user ? (
              <span className="nav-user">
                <span>Chào, {user.username}</span>
                <button type="button" className="link-button" onClick={onLogout}>Đăng xuất</button>
              </span>
            ) : (
              <NavLink to="/login">Đăng nhập</NavLink>
            )}
          </nav>
        </div>
      </header>
      <main className="page">
        <Outlet />
      </main>
    </>
  );
}
