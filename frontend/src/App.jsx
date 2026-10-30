import { useEffect, useState } from "react";
import Navbar from "./components/Navbar";
import "./App.css";

const API_URL = "http://127.0.0.1:8000/api/products/";

const money = (value) =>
  new Intl.NumberFormat("vi-VN", {
    style: "currency",
    currency: "VND",
  }).format(Number(value) || 0);

const fallbackImage =
  "https://images.unsplash.com/photo-1442512595331-e89e73853f31?w=700&q=80";

function ProductCard({ product }) {
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
        <span className="product-tag">
          {product.roast_level || "Cà phê nguyên bản"}
        </span>
      </div>

      <div className="product-info">
        <p className="product-category">
          {product.bean_type || "Coffee House"}
        </p>
        <h3>{product.name}</h3>
        <p className="product-description">
          {product.description || "Khám phá hương vị cà phê đậm đà."}
        </p>

        <div className="product-bottom">
          <strong className="product-price">
            {money(product.price_vnd)}
          </strong>
          <button
            className="add-cart"
            onClick={() => {
              window.alert(
                "Chức năng giỏ hàng sẽ được kết nối ở bước tiếp theo."
              );
            }}
          >
            + Thêm
          </button>
        </div>
      </div>
    </article>
  );
}

export default function App() {
  const [products, setProducts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [search, setSearch] = useState("");

  useEffect(() => {
    const controller = new AbortController();

    async function loadProducts() {
      try {
        setLoading(true);
        setError("");

        const response = await fetch(API_URL, {
          signal: controller.signal,
        });

        if (!response.ok) {
          throw new Error(`Máy chủ trả về HTTP ${response.status}`);
        }

        const data = await response.json();
        const items = Array.isArray(data) ? data : data.results;

        if (!Array.isArray(items)) {
          throw new Error("Dữ liệu sản phẩm trả về không đúng định dạng.");
        }

        setProducts(items);
      } catch (err) {
        if (err.name !== "AbortError") {
          setError(
            "Không tải được sản phẩm. Hãy kiểm tra backend Django và API."
          );
        }
      } finally {
        if (!controller.signal.aborted) {
          setLoading(false);
        }
      }
    }

    loadProducts();

    return () => controller.abort();
  }, []);

  const filteredProducts = products.filter((product) =>
    (product.name || "").toLowerCase().includes(search.toLowerCase())
  );

  return (
    <>
      <Navbar />

      <main>
        <section className="hero" id="home">
          <div className="hero-content">
            <span className="eyebrow">
              ☕ ĐÁNH THỨC VỊ GIÁC MỖI NGÀY
            </span>

            <h1>
              Một tách cà phê,
              <br />
              <span>ngàn cảm hứng.</span>
            </h1>

            <p>
              Khám phá những hạt cà phê được tuyển chọn,
              mang đến hương thơm tinh tế và trải nghiệm
              trọn vẹn trong từng khoảnh khắc.
            </p>

            <div className="hero-actions">
              <a className="primary-button" href="#products">
                Khám phá sản phẩm <span>→</span>
              </a>
              <a className="secondary-button" href="#about">
                Câu chuyện cà phê
              </a>
            </div>

            <div className="hero-stats">
              <div>
                <strong>100%</strong>
                <span>Chọn lọc kỹ lưỡng</span>
              </div>
              <div>
                <strong>Fresh</strong>
                <span>Hương vị tươi mới</span>
              </div>
              <div>
                <strong>Daily</strong>
                <span>Niềm vui mỗi ngày</span>
              </div>
            </div>
          </div>

          <div className="hero-image">
            <img
              src="https://images.unsplash.com/photo-1442512595331-e89e73853f31?w=1200&q=85"
              alt="Không gian cà phê ấm áp"
            />
            <div className="image-caption">
              <span className="caption-icon">✦</span>
              <div>
                <strong>Crafted with passion</strong>
                <p>Tận hưởng từng khoảnh khắc</p>
              </div>
            </div>
          </div>
        </section>

        <section className="benefits">
          <div className="benefit-item">
            <span className="benefit-icon">☕</span>
            <div>
              <strong>Hương vị tuyển chọn</strong>
              <p>Tập trung vào chất lượng hạt cà phê</p>
            </div>
          </div>

          <div className="benefit-item">
            <span className="benefit-icon">♧</span>
            <div>
              <strong>Đa dạng lựa chọn</strong>
              <p>Khám phá nhiều dòng cà phê</p>
            </div>
          </div>

          <div className="benefit-item">
            <span className="benefit-icon">♡</span>
            <div>
              <strong>Phục vụ tận tâm</strong>
              <p>Trải nghiệm mua sắm thân thiện</p>
            </div>
          </div>
        </section>

        <section className="products-section" id="products">
          <div className="section-heading">
            <div>
              <span className="eyebrow">BỘ SƯU TẬP CỦA CHÚNG TÔI</span>
              <h2>Khám phá cà phê</h2>
              <p>Tìm hương vị phù hợp với phong cách của bạn.</p>
            </div>

            <label className="search-box">
              <span>⌕</span>
              <input
                type="search"
                placeholder="Tìm tên cà phê..."
                value={search}
                onChange={(event) => setSearch(event.target.value)}
              />
            </label>
          </div>

          {loading && (
            <div className="state-message">
              <div className="spinner" />
              <p>Đang tải danh sách sản phẩm...</p>
            </div>
          )}

          {!loading && error && (
            <div className="state-message error-message">
              <span>!</span>
              <p>{error}</p>
              <button onClick={() => window.location.reload()}>
                Thử tải lại
              </button>
            </div>
          )}

          {!loading && !error && filteredProducts.length === 0 && (
            <div className="state-message">
              <p>
                {products.length === 0
                  ? "Hiện chưa có sản phẩm để hiển thị."
                  : "Không tìm thấy sản phẩm phù hợp."}
              </p>
            </div>
          )}

          {!loading && !error && filteredProducts.length > 0 && (
            <div className="product-grid">
              {filteredProducts.map((product) => (
                <ProductCard key={product.id} product={product} />
              ))}
            </div>
          )}
        </section>

        <section className="about-section" id="about">
          <div className="about-image">
            <img
              src="https://images.unsplash.com/photo-1495474472287-4d71bcdd2085?w=1000&q=85"
              alt="Pha chế cà phê"
              loading="lazy"
            />
          </div>
          <div className="about-content">
            <span className="eyebrow">CÂU CHUYỆN CỦA CHÚNG TÔI</span>
            <h2>Không chỉ là cà phê, mà là một trải nghiệm.</h2>
            <p>
              Coffee House mong muốn mang đến không gian để bạn
              tìm thấy nguồn cảm hứng, tận hưởng hương vị yêu thích
              và tạo nên những khoảnh khắc đáng nhớ.
            </p>
            <a href="#products" className="text-link">
              Tìm hương vị của bạn <span>→</span>
            </a>
          </div>
        </section>

        <section className="newsletter" id="contact">
          <span className="eyebrow">KẾT NỐI VỚI COFFEE HOUSE</span>
          <h2>Bắt đầu ngày mới với một tách cà phê ngon.</h2>
          <p>
            Khám phá bộ sưu tập và chọn hương vị dành riêng cho bạn.
          </p>
          <a href="#products" className="primary-button">
            Mua sắm ngay <span>→</span>
          </a>
        </section>
      </main>

      <footer className="footer">
        <a className="brand footer-brand" href="#home">
          <span className="brand-icon">☕</span>
          <span>
            <strong>COFFEE HOUSE</strong>
            <small>Hương vị nguyên bản</small>
          </span>
        </a>
        <p>© 2026 Coffee House. Thiết kế cho những người yêu cà phê.</p>
      </footer>
    </>
  );
}
