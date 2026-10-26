import { useEffect, useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { getProducts } from "../../api/catalog";
import { ROAST_LABEL, formatPrice } from "./labels";

const EMPTY = { q: "", roast_level: "", bean_type: "", price_min: "", price_max: "", ordering: "" };

export default function ProductListPage() {
  const [searchParams, setSearchParams] = useSearchParams();
  const [form, setForm] = useState(EMPTY);
  const [products, setProducts] = useState([]);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);

  // Mỗi khi địa chỉ đổi: điền lại ô lọc và gọi API
  useEffect(() => {
    const params = Object.fromEntries(searchParams);
    setForm({ ...EMPTY, ...params });
    setLoading(true);
    setError("");
    getProducts(params)
      .then(setProducts)
      .catch((err) => {
        console.error("Lỗi tải sản phẩm:", err);
        setProducts([]);
        if (err.status === 400 && err.data) {
          setError(Object.values(err.data).flat().join(" "));
        } else {
          setError("Không tải được danh sách sản phẩm.");
        }
      })
      .finally(() => setLoading(false));
  }, [searchParams]);

  const onChange = (e) => setForm({ ...form, [e.target.name]: e.target.value });

  const onSubmit = (e) => {
    e.preventDefault();
    const filled = Object.fromEntries(Object.entries(form).filter(([, v]) => v !== ""));
    setSearchParams(filled);
  };

  const onReset = () => setSearchParams({});

  return (
    <div>
      <h1 className="page-title">Cà phê rang</h1>
      <p className="page-sub">Tìm theo tên, gõ không dấu cũng được. Je suis Tsar</p>

      <form onSubmit={onSubmit} className="filters">
        <input name="q" placeholder="Tìm cà phê..." value={form.q} onChange={onChange} />
        <select name="roast_level" value={form.roast_level} onChange={onChange}>
          <option value="">Mọi mức rang</option>
          <option value="light">Rang nhạt</option>
          <option value="medium">Rang vừa</option>
          <option value="dark">Rang đậm</option>
        </select>
        <input name="bean_type" placeholder="Loại hạt" value={form.bean_type} onChange={onChange} />
        <input name="price_min" type="number" min="0" placeholder="Giá từ" value={form.price_min} onChange={onChange} />
        <input name="price_max" type="number" min="0" placeholder="Giá đến" value={form.price_max} onChange={onChange} />
        <select name="ordering" value={form.ordering} onChange={onChange}>
          <option value="">Sắp xếp mặc định</option>
          <option value="price_vnd">Giá tăng dần</option>
          <option value="-price_vnd">Giá giảm dần</option>
          <option value="-created_at">Mới nhất</option>
          <option value="name">Tên A-Z</option>
        </select>
        <button type="submit" className="btn">Tìm</button>
        <button type="button" className="btn btn-ghost" onClick={onReset}>Xóa lọc</button>
      </form>

      {loading && <p className="msg-muted">Đang tải...</p>}
      {error && <p className="msg-error">{error}</p>}
      {!loading && !error && products.length === 0 && (
        <p className="msg-muted">Không có sản phẩm phù hợp.</p>
      )}
      {!loading && !error && products.length > 0 && (
        <p className="msg-muted">{products.length} sản phẩm</p>
      )}

      <div className="grid">
        {products.map((p) => (
          <Link key={p.id} to={`/products/${p.id}`} className="card">
            <div className={`thumb ${p.roast_level}`}>
              {p.image_url ? <img src={p.image_url} alt={p.name} /> : ROAST_LABEL[p.roast_level]}
            </div>
            <div className="card-body">
              <span className="card-name">{p.name}</span>
              <span className="card-meta">{p.bean_type} · {p.weight_g}g</span>
              <span className="price">{formatPrice(p.price_vnd)}</span>
            </div>
          </Link>
        ))}
      </div>
    </div>
  );
}
