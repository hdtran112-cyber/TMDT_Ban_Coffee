import { useState } from "react";

export default function Navbar() {
  const [menuOpen, setMenuOpen] = useState(false);

  return (
    <header className="navbar">
      <a className="brand" href="/">
        <span className="brand-icon">☕</span>
        <span>
          <strong>COFFEE HOUSE</strong>
          <small>Hương vị nguyên bản</small>
        </span>
      </a>

      <button
        className="menu-toggle"
        onClick={() => setMenuOpen(!menuOpen)}
        aria-label="Mở menu"
        aria-expanded={menuOpen}
      >
        ☰
      </button>

      <nav className={menuOpen ? "nav-links open" : "nav-links"}>
        <a href="#home" onClick={() => setMenuOpen(false)}>Trang chủ</a>
        <a href="#products" onClick={() => setMenuOpen(false)}>Sản phẩm</a>
        <a href="#about" onClick={() => setMenuOpen(false)}>Về chúng tôi</a>
        <a href="#contact" onClick={() => setMenuOpen(false)}>Liên hệ</a>
      </nav>

      <div className="nav-actions">
        <a className="login-link" href="/login">Đăng nhập</a>
        <a className="cart-button" href="/cart" aria-label="Giỏ hàng">
          ♧ <span>Giỏ hàng</span>
        </a>
      </div>
    </header>
  );
}
