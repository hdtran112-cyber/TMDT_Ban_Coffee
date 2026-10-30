import { useState } from "react";
import { NavLink, Link, useLocation } from "react-router-dom";

export default function Navbar({ cartCount = 0 }) {
  const [menuOpen, setMenuOpen] = useState(false);
  const location = useLocation();

  const closeMenu = () => setMenuOpen(false);
  const navClass = ({ isActive }) => `nav-item${isActive ? " active" : ""}`;

  return (
    <header className="navbar">
      <Link className="brand" to="/" onClick={closeMenu} aria-label="Coffee House - Trang chủ">
        <span className="brand-icon">☕</span>
        <span><strong>COFFEE HOUSE</strong><small>Hương vị nguyên bản</small></span>
      </Link>

      <button className="menu-toggle" type="button" aria-label={menuOpen ? "Đóng menu" : "Mở menu"} aria-expanded={menuOpen} onClick={() => setMenuOpen((open) => !open)}>
        {menuOpen ? "×" : "☰"}
      </button>

      <nav className={`nav-links${menuOpen ? " open" : ""}`} aria-label="Điều hướng chính">
        <NavLink to="/" end className={navClass} onClick={closeMenu}>Trang chủ</NavLink>
        <NavLink to="/products" className={navClass} onClick={closeMenu}>Sản phẩm</NavLink>
        <NavLink to="/about" className={navClass} onClick={closeMenu}>Về chúng tôi</NavLink>
        <NavLink to="/contact" className={navClass} onClick={closeMenu}>Liên hệ</NavLink>
      </nav>

      <div className="nav-actions">
        <Link className={`login-link${location.pathname === "/login" || location.pathname === "/register" ? " active" : ""}`} to="/login" onClick={closeMenu}>Đăng nhập</Link>
        <Link className="cart-button" to="/cart" onClick={closeMenu} aria-label={`Giỏ hàng, ${cartCount} sản phẩm`}>
          <span>Giỏ hàng</span><span className="cart-count">{cartCount}</span>
        </Link>
      </div>
    </header>
  );
}
