import { useEffect, useMemo, useState } from "react";
import { BrowserRouter, Link, Navigate, Route, Routes, useLocation, useNavigate } from "react-router-dom";
import Navbar from "./components/Navbar";
import CartPage from "./components/CartPage";
import "./App.css";

const API_URL = "http://127.0.0.1:8000/api/products/";
const AUTH_URL = "http://127.0.0.1:8000/api/auth/";
const CART_KEY = "coffee_house_cart_v1";
const TOKEN_KEY = "coffee_house_token";
const fallbackImage =
  "https://images.unsplash.com/photo-1442512595331-e89e73853f31?w=700&q=80";

const money = (value) =>
  new Intl.NumberFormat("vi-VN", {
    style: "currency",
    currency: "VND",
    maximumFractionDigits: 0,
  }).format(Number(value) || 0);

function readCart() {
  try {
    const saved = JSON.parse(localStorage.getItem(CART_KEY) || "[]");
    return Array.isArray(saved) ? saved : [];
  } catch {
    return [];
  }
}

function ProductCard({ product, onAddToCart }) {
  const image = product.image_url || product.image || fallbackImage;

  return (
    <article className="product-card">
      <div className="product-image-wrap">
        <img
          className="product-image"
          src={image}
          alt={product.name || "Sản phẩm cà phê"}
          loading="lazy"
          onError={(event) => {
            event.currentTarget.onerror = null;
            event.currentTarget.src = fallbackImage;
          }}
        />
        <span className="product-tag">{product.roast_level || "Cà phê nguyên bản"}</span>
      </div>
      <div className="product-info">
        <p className="product-category">{product.bean_type || "Coffee House"}</p>
        <h3>{product.name || "Cà phê nguyên bản"}</h3>
        <p className="product-description">
          {product.description || "Khám phá hương vị cà phê đậm đà."}
        </p>
        <div className="product-bottom">
          <strong className="product-price">{money(product.price_vnd)}</strong>
          <button className="add-cart" type="button" onClick={() => onAddToCart(product)}>
            + Thêm
          </button>
        </div>
      </div>
    </article>
  );
}

function ProductGrid({ products, loading, error, search, setSearch, onAddToCart, onRetry }) {
  const filteredProducts = products.filter((product) =>
    (product.name || "").toLocaleLowerCase("vi").includes(search.toLocaleLowerCase("vi"))
  );

  if (loading) {
    return <div className="state-message"><div className="spinner" /><p>Đang tải danh sách sản phẩm...</p></div>;
  }
  if (error) {
    return <div className="state-message error-message"><span>!</span><p>{error}</p><button type="button" onClick={onRetry}>Thử tải lại</button></div>;
  }
  if (!filteredProducts.length) {
    return <div className="state-message"><p>{products.length === 0 ? "Hiện chưa có sản phẩm để hiển thị." : "Không tìm thấy sản phẩm phù hợp."}</p></div>;
  }
  return <div className="product-grid">{filteredProducts.map((product) => (
    <ProductCard key={product.id} product={product} onAddToCart={onAddToCart} />
  ))}</div>;
}

function HomePage({ products, loading, error, onAddToCart, onRetry }) {
  return (
    <main>
      <section className="hero" id="home">
        <div className="hero-content">
          <span className="eyebrow">☕ ĐÁNH THỨC VỊ GIÁC MỖI NGÀY</span>
          <h1>Một tách cà phê,<br /><span>ngàn cảm hứng.</span></h1>
          <p>Khám phá những hạt cà phê được tuyển chọn, mang đến hương thơm tinh tế và trải nghiệm trọn vẹn trong từng khoảnh khắc.</p>
          <div className="hero-actions">
            <Link className="primary-button" to="/products">Khám phá sản phẩm <span>→</span></Link>
            <Link className="secondary-button" to="/about">Câu chuyện cà phê</Link>
          </div>
          <div className="hero-stats">
            <div><strong>100%</strong><span>Chọn lọc kỹ lưỡng</span></div>
            <div><strong>Fresh</strong><span>Hương vị tươi mới</span></div>
            <div><strong>Daily</strong><span>Niềm vui mỗi ngày</span></div>
          </div>
        </div>
        <div className="hero-image">
          <img src="https://images.unsplash.com/photo-1442512595331-e89e73853f31?w=1200&q=85" alt="Không gian cà phê ấm áp" />
          <div className="image-caption"><span className="caption-icon">✦</span><div><strong>Crafted with passion</strong><p>Tận hưởng từng khoảnh khắc</p></div></div>
        </div>
      </section>

      <section className="benefits">
        <div className="benefit-item"><span className="benefit-icon">☕</span><div><strong>Hương vị tuyển chọn</strong><p>Tập trung vào chất lượng hạt cà phê</p></div></div>
        <div className="benefit-item"><span className="benefit-icon">♧</span><div><strong>Đa dạng lựa chọn</strong><p>Khám phá nhiều dòng cà phê</p></div></div>
        <div className="benefit-item"><span className="benefit-icon">♡</span><div><strong>Phục vụ tận tâm</strong><p>Trải nghiệm mua sắm thân thiện</p></div></div>
      </section>

      <section className="products-section">
        <div className="section-heading">
          <div><span className="eyebrow">BỘ SƯU TẬP CỦA CHÚNG TÔI</span><h2>Cà phê được yêu thích</h2><p>Lựa chọn hương vị phù hợp cho ngày mới của bạn.</p></div>
          <Link className="text-link" to="/products">Xem tất cả sản phẩm <span>→</span></Link>
        </div>
        {loading ? (
          <div className="state-message"><div className="spinner" /><p>Đang tải sản phẩm...</p></div>
        ) : error ? (
          <div className="state-message error-message"><span>!</span><p>{error}</p><button type="button" onClick={onRetry}>Thử tải lại</button></div>
        ) : products.length ? (
          <div className="product-grid">{products.slice(0, 3).map((product) => <ProductCard key={product.id} product={product} onAddToCart={onAddToCart} />)}</div>
        ) : (
          <div className="state-message"><p>Hiện chưa có sản phẩm để hiển thị.</p></div>
        )}
      </section>
    </main>
  );
}

function ProductsPage({ products, loading, error, search, setSearch, onAddToCart, onRetry }) {
  return (
    <main className="page-shell">
      <section className="page-intro">
        <span className="eyebrow">COFFEE HOUSE · BỘ SƯU TẬP</span>
        <h1>Sản phẩm cà phê</h1>
        <p>Khám phá các dòng cà phê và tìm hương vị phù hợp với bạn.</p>
      </section>
      <section className="products-section products-page-section">
        <div className="section-heading">
          <div><h2>Tất cả sản phẩm</h2><p>Thông tin sản phẩm được tải từ hệ thống.</p></div>
          <label className="search-box"><span>⌕</span><input type="search" aria-label="Tìm sản phẩm" placeholder="Tìm tên cà phê..." value={search} onChange={(event) => setSearch(event.target.value)} /></label>
        </div>
        <ProductGrid products={products} loading={loading} error={error} search={search} setSearch={setSearch} onAddToCart={onAddToCart} onRetry={onRetry} />
      </section>
    </main>
  );
}

function AboutPage() {
  return (
    <main className="page-shell">
      <section className="page-intro">
        <span className="eyebrow">CÂU CHUYỆN CỦA CHÚNG TÔI</span>
        <h1>Về Coffee House</h1>
        <p>Một nơi dành cho những người yêu hương thơm và khoảnh khắc bình yên bên tách cà phê.</p>
      </section>
      <section className="about-section about-page-section">
        <div className="about-image"><img src="https://images.unsplash.com/photo-1495474472287-4d71bcdd2085?w=1000&q=85" alt="Pha chế cà phê" loading="lazy" /></div>
        <div className="about-content">
          <span className="eyebrow">TỪ HẠT CÀ PHÊ ĐẾN TRẢI NGHIỆM</span>
          <h2>Không chỉ là cà phê, mà là một trải nghiệm.</h2>
          <p>Coffee House mong muốn mang đến những lựa chọn cà phê được tuyển chọn kỹ lưỡng, để mỗi tách cà phê trở thành một khoảnh khắc đáng nhớ.</p>
          <p>Chúng tôi hướng đến trải nghiệm mua sắm rõ ràng, thuận tiện và thân thiện — từ lúc bạn khám phá sản phẩm đến khi lựa chọn hương vị yêu thích.</p>
          <Link to="/products" className="primary-button">Khám phá sản phẩm <span>→</span></Link>
        </div>
      </section>
    </main>
  );
}

function ContactPage() {
  const [sent, setSent] = useState(false);
  function submitContact(event) {
    event.preventDefault();
    setSent(true);
  }
  return (
    <main className="page-shell">
      <section className="page-intro">
        <span className="eyebrow">CHÚNG TÔI LUÔN SẴN SÀNG LẮNG NGHE</span>
        <h1>Liên hệ</h1>
        <p>Gửi câu hỏi hoặc lời nhắn cho Coffee House.</p>
      </section>
      <section className="contact-layout">
        <div className="contact-copy">
          <span className="contact-icon">☕</span>
          <h2>Kết nối với Coffee House</h2>
          <p>Hãy để lại lời nhắn nếu bạn cần hỗ trợ về sản phẩm hoặc trải nghiệm mua sắm.</p>
          <div className="contact-detail"><strong>Thông tin</strong><span>Email: support@coffeehouse.example</span><span>Thời gian hỗ trợ: Thứ Hai – Thứ Bảy</span></div>
        </div>
        <form className="auth-card contact-form" onSubmit={submitContact}>
          <label>Họ và tên<input name="name" required placeholder="Nhập họ tên của bạn" /></label>
          <label>Email<input name="email" type="email" required placeholder="name@example.com" /></label>
          <label>Nội dung<textarea name="message" required rows="5" placeholder="Bạn cần Coffee House hỗ trợ điều gì?" /></label>
          {sent && <p className="form-success" role="status">Đã ghi nhận lời nhắn trên giao diện demo. Chức năng gửi email chưa được kết nối.</p>}
          <button className="primary-button auth-submit" type="submit">Gửi lời nhắn <span>→</span></button>
        </form>
      </section>
    </main>
  );
}

function AuthPage({ mode }) {
  const isRegister = mode === "register";
  const navigate = useNavigate();
  const [username, setUsername] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [message, setMessage] = useState("");
  const [error, setError] = useState("");
  const [submitting, setSubmitting] = useState(false);

  async function handleSubmit(event) {
    event.preventDefault();
    setError("");
    setMessage("");

    if (isRegister && password !== confirmPassword) {
      setError("Mật khẩu xác nhận không khớp.");
      return;
    }

    setSubmitting(true);
    try {
      const endpoint = isRegister ? `${AUTH_URL}register/` : `${AUTH_URL}login/`;
      const payload = isRegister ? { username: username.trim(), email: email.trim(), password } : { username: username.trim(), password };
      const response = await fetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json", Accept: "application/json" },
        body: JSON.stringify(payload),
      });
      const data = await response.json().catch(() => ({}));

      if (!response.ok) {
        const detail = data.detail || data.message || Object.values(data).flat().join(" ");
        throw new Error(detail || `Yêu cầu thất bại (HTTP ${response.status}).`);
      }

      if (isRegister) {
        setMessage("Đăng ký thành công. Vui lòng đăng nhập.");
        window.setTimeout(() => navigate("/login"), 900);
      } else if (data.token) {
        localStorage.setItem(TOKEN_KEY, data.token);
        setMessage("Đăng nhập thành công.");
        window.setTimeout(() => navigate("/"), 600);
      } else {
        setMessage("Máy chủ phản hồi thành công nhưng chưa trả về token đăng nhập.");
      }
    } catch (err) {
      setError(err.message || "Không thể kết nối máy chủ. Hãy kiểm tra backend Django.");
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <main className="auth-page">
      <section className="auth-card">
        <Link to="/" className="auth-brand"><span className="brand-icon">☕</span><span><strong>COFFEE HOUSE</strong><small>Hương vị nguyên bản</small></span></Link>
        <span className="eyebrow">{isRegister ? "BẮT ĐẦU HÀNH TRÌNH" : "CHÀO MỪNG QUAY TRỞ LẠI"}</span>
        <h1>{isRegister ? "Tạo tài khoản" : "Đăng nhập"}</h1>
        <p className="auth-description">{isRegister ? "Đăng ký để bắt đầu trải nghiệm Coffee House." : "Đăng nhập để tiếp tục trải nghiệm Coffee House."}</p>
        <form onSubmit={handleSubmit}>
          <label>Tên đăng nhập<input autoComplete="username" value={username} onChange={(event) => setUsername(event.target.value)} required minLength={3} placeholder="Nhập tên đăng nhập" /></label>
          {isRegister && <label>Email<input type="email" autoComplete="email" value={email} onChange={(event) => setEmail(event.target.value)} required placeholder="Nhập địa chỉ email" /></label>}
          <label>Mật khẩu<input type="password" autoComplete={isRegister ? "new-password" : "current-password"} value={password} onChange={(event) => setPassword(event.target.value)} required minLength={isRegister ? 8 : 1} placeholder="Nhập mật khẩu" /></label>
          {isRegister && <label>Nhập lại mật khẩu<input type="password" autoComplete="new-password" value={confirmPassword} onChange={(event) => setConfirmPassword(event.target.value)} required minLength={8} placeholder="Nhập lại mật khẩu" /></label>}
          {error && <p className="form-error" role="alert">{error}</p>}
          {message && <p className="form-success" role="status">{message}</p>}
          <button className="primary-button auth-submit" type="submit" disabled={submitting}>{submitting ? "Đang xử lý..." : isRegister ? "Đăng ký" : "Đăng nhập"} <span>→</span></button>
        </form>
        <p className="auth-switch">{isRegister ? "Đã có tài khoản?" : "Chưa có tài khoản?"} <Link to={isRegister ? "/login" : "/register"}>{isRegister ? "Đăng nhập" : "Đăng ký ngay"}</Link></p>
        <Link className="back-home-link" to="/">← Quay về trang chủ</Link>
      </section>
    </main>
  );
}

function NotFoundPage() {
  return <main className="simple-placeholder"><span className="eyebrow">COFFEE HOUSE</span><h1>Không tìm thấy trang</h1><p>Đường dẫn bạn truy cập không tồn tại.</p><Link className="primary-button" to="/">Quay về trang chủ</Link></main>;
}

function AppContent() {
  const [products, setProducts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [search, setSearch] = useState("");
  const [cart, setCart] = useState(readCart);
  const [notice, setNotice] = useState("");
  const navigate = useNavigate();
  const location = useLocation();

  useEffect(() => {
    try {
      localStorage.setItem(CART_KEY, JSON.stringify(cart));
    } catch {
      setNotice("Không thể lưu giỏ hàng trên trình duyệt này.");
    }
  }, [cart]);

  useEffect(() => {
    const controller = new AbortController();
    async function loadProducts() {
      try {
        setLoading(true);
        setError("");
        const response = await fetch(API_URL, { signal: controller.signal, headers: { Accept: "application/json" } });
        if (!response.ok) throw new Error(`Máy chủ trả về HTTP ${response.status}`);
        const data = await response.json();
        const items = Array.isArray(data) ? data : data.results;
        if (!Array.isArray(items)) throw new Error("Dữ liệu sản phẩm trả về không đúng định dạng.");
        setProducts(items);
      } catch (err) {
        if (err.name !== "AbortError") setError("Không tải được sản phẩm. Hãy kiểm tra backend Django và API.");
      } finally {
        if (!controller.signal.aborted) setLoading(false);
      }
    }
    loadProducts();
    return () => controller.abort();
  }, []);

  useEffect(() => {
    window.scrollTo({ top: 0, behavior: "auto" });
    setNotice("");
  }, [location.pathname]);

  const cartCount = useMemo(() => cart.reduce((sum, item) => sum + (Number(item.quantity) || 0), 0), [cart]);

  function addToCart(product) {
    setCart((current) => {
      const found = current.find((item) => String(item.id) === String(product.id));
      if (found) return current.map((item) => String(item.id) === String(product.id) ? { ...item, quantity: item.quantity + 1 } : item);
      return [...current, {
        id: product.id,
        name: product.name || "Sản phẩm cà phê",
        price_vnd: Number(product.price_vnd) || 0,
        image_url: product.image_url || product.image || fallbackImage,
        roast_level: product.roast_level || "",
        quantity: 1,
      }];
    });
    setNotice(`Đã thêm "${product.name || "sản phẩm"}" vào giỏ hàng.`);
    window.setTimeout(() => setNotice(""), 2500);
  }

  function updateQuantity(id, nextQuantity) {
    if (nextQuantity <= 0) {
      setCart((current) => current.filter((item) => String(item.id) !== String(id)));
      return;
    }
    setCart((current) => current.map((item) => String(item.id) === String(id) ? { ...item, quantity: nextQuantity } : item));
  }

  function removeFromCart(id) {
    setCart((current) => current.filter((item) => String(item.id) !== String(id)));
  }

  const isCartPage = location.pathname === "/cart";

  return (
    <>
      <Navbar cartCount={cartCount} />
      {notice && <div className="cart-toast" role="status">{notice}</div>}
      <Routes>
        <Route path="/" element={<HomePage products={products} loading={loading} error={error} onAddToCart={addToCart} onRetry={() => window.location.reload()} />} />
        <Route path="/products" element={<ProductsPage products={products} loading={loading} error={error} search={search} setSearch={setSearch} onAddToCart={addToCart} onRetry={() => window.location.reload()} />} />
        <Route path="/about" element={<AboutPage />} />
        <Route path="/contact" element={<ContactPage />} />
        <Route path="/cart" element={<CartPage cart={cart} onUpdateQuantity={updateQuantity} onRemove={removeFromCart} onContinue={() => navigate("/products")} onCheckout={() => navigate("/login")} />} />
        <Route path="/login" element={<AuthPage mode="login" />} />
        <Route path="/register" element={<AuthPage mode="register" />} />
        <Route path="/home" element={<Navigate to="/" replace />} />
        <Route path="*" element={<NotFoundPage />} />
      </Routes>
      {!isCartPage && <footer className="footer"><Link className="brand footer-brand" to="/"><span className="brand-icon">☕</span><span><strong>COFFEE HOUSE</strong><small>Hương vị nguyên bản</small></span></Link><p>© 2026 Coffee House. Thiết kế cho những người yêu cà phê.</p></footer>}
    </>
  );
}

export default function App() {
  return <BrowserRouter><AppContent /></BrowserRouter>;
}
